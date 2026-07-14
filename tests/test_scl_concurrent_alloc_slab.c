/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Tests for the concurrent slab allocator:
 *   - init / destroy
 *   - alloc / free across several bucket sizes
 *   - realloc that stays within the same bucket returns the same pointer
 *   - concurrent alloc(64)+free under 4 threads via scl_test_run_concurrent
 *
 * The concurrent case is designed to fault under ThreadSanitizer if the
 * per-bucket spinlock or free-list linkage is wrong. */

#include "scl_common.h"
#include "scl_concurrent_alloc_slab.h"
#include "scl_concurrent_common.h"
#include "scl_string.h"
#include "scl_test.h"
#include <stdatomic.h>

/* ── init / destroy ────────────────────────────────────────── */
static void test_cslab_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSlab: init and destroy");

  scl_allocator_t *alloc =
      scl_calloc_slab_create(scl_allocator_default(), NULL, 0, 0);
  SCL_EXPECT_NOT_NULL(tr, alloc);
  if (!alloc)
    return;

  scl_calloc_slab_destroy(alloc);
  TEST_TRACE_END();
}

/* ── alloc / free various sizes ────────────────────────────── */
static void test_cslab_alloc_free_sizes(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSlab: alloc/free various sizes");

  scl_allocator_t *alloc =
      scl_calloc_slab_create(scl_allocator_default(), NULL, 0, 0);
  SCL_EXPECT_NOT_NULL(tr, alloc);
  if (!alloc)
    return;

  const size_t sizes[] = {16, 64, 512};
  void *ptrs[3];
  for (int i = 0; i < 3; i++) {
    ptrs[i] = scl_alloc(alloc, sizes[i], alignof(max_align_t));
    SCL_EXPECT_NOT_NULL(tr, ptrs[i]);
    if (ptrs[i]) {
      /* Write a distinctive pattern; free must wipe it. */
      scl_memset(ptrs[i], (int)(0xA0 + i), sizes[i]);
    }
  }

  /* Freeing should succeed and securely zero the block. The first
   * sizeof(void*) bytes are then overwritten by the free-list link, so
   * only the bytes beyond the link word are guaranteed zero. */
  for (int i = 0; i < 3; i++) {
    scl_free(alloc, ptrs[i]);
    unsigned char *bp = (unsigned char *)ptrs[i];
    size_t link = sizeof(void *);
    int tail_zero = 1;
    for (size_t b = link; b < sizes[i]; b++) {
      if (bp[b] != 0) {
        tail_zero = 0;
        break;
      }
    }
    SCL_EXPECT_TRUE(tr, tail_zero);
  }

  /* Re-allocate the same sizes — should succeed (blocks returned to pools). */
  for (int i = 0; i < 3; i++) {
    void *p = scl_alloc(alloc, sizes[i], alignof(max_align_t));
    SCL_EXPECT_NOT_NULL(tr, p);
    scl_free(alloc, p);
  }

  scl_calloc_slab_destroy(alloc);
  TEST_TRACE_END();
}

/* ── realloc same-bucket returns same pointer ────────────────
 * 33 and 40 both fall in the 40-byte bucket (the default size table
 * includes 40), so realloc(33 -> 40) must hand back the identical
 * pointer without copying. */
static void test_cslab_realloc_same_bucket(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSlab: realloc same bucket keeps pointer");

  scl_allocator_t *alloc =
      scl_calloc_slab_create(scl_allocator_default(), NULL, 0, 0);
  SCL_EXPECT_NOT_NULL(tr, alloc);
  if (!alloc)
    return;

  void *p = scl_alloc(alloc, 33, alignof(max_align_t));
  SCL_EXPECT_NOT_NULL(tr, p);
  if (!p) {
    scl_calloc_slab_destroy(alloc);
    return;
  }

  scl_memset(p, 0x5A, 33);

  void *q = scl_realloc(alloc, p, 33, 40, alignof(max_align_t));
  SCL_EXPECT_NOT_NULL(tr, q);
  /* Same bucket → same pointer. */
  SCL_EXPECT_EQ_PTR(tr, q, p);

  if (q) {
    /* First 33 bytes must be preserved. */
    unsigned char *bp = (unsigned char *)q;
    int preserved = 1;
    for (size_t i = 0; i < 33; i++) {
      if (bp[i] != 0x5A) {
        preserved = 0;
        break;
      }
    }
    SCL_EXPECT_TRUE(tr, preserved);
    scl_free(alloc, q);
  }

  scl_calloc_slab_destroy(alloc);
  TEST_TRACE_END();
}

/* ── concurrent alloc(64) + free ───────────────────────────── */
#define CSLAB_NTHREADS 4
#define CSLAB_ITERS 300

static void *cslab_worker(void *p) {
  scl_test_thread_arg_t *arg = (scl_test_thread_arg_t *)p;
  scl_allocator_t *alloc = (scl_allocator_t *)arg->user_data;

  scl_test_barrier_wait(arg->barrier);

  for (int i = 0; i < CSLAB_ITERS; i++) {
    void *block = scl_alloc(alloc, 64, alignof(max_align_t));
    if (!SCL_CC_EXPECT(arg->cc, block != NULL))
      continue;

    /* Scribble to ensure the block is writable and distinct per thread. */
    scl_memset(block, (int)(arg->thread_id & 0xFF), 64);

    scl_free(alloc, block);
  }
  return NULL;
}

static void test_cslab_concurrent(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSlab: concurrent alloc/free (4 threads x 300 iters)");

  scl_allocator_t *alloc =
      scl_calloc_slab_create(scl_allocator_default(), NULL, 0, 0);
  SCL_EXPECT_NOT_NULL(tr, alloc);
  if (!alloc)
    return;

  int failed = scl_test_run_concurrent(tr, CSLAB_NTHREADS, cslab_worker, alloc);
  SCL_EXPECT_EQ_I(tr, failed, 0);

  scl_calloc_slab_destroy(alloc);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cslab_init_destroy(&tr);
  test_cslab_alloc_free_sizes(&tr);
  test_cslab_realloc_same_bucket(&tr);
  test_cslab_concurrent(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
