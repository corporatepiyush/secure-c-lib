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

/* Tests for the concurrent TLSF allocator (scl_calloc_tlsf_*). */

#include "scl_common.h"
#include "scl_concurrent_alloc_tlsf.h"
#include "scl_string.h"
#include "scl_test.h"

#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>

/* ── 1. init / destroy ─────────────────────────────────────── */
static void test_ctlsf_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CTLSF: init and destroy (65536 pool)");
  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *a = scl_calloc_tlsf_create(backing, 65536, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  /* A freshly created pool must be able to service a modest alloc. */
  void *p = scl_alloc(a, 128, 0);
  SCL_EXPECT_NOT_NULL(tr, p);
  scl_free(a, p);

  scl_calloc_tlsf_destroy(a);
  TEST_TRACE_END();
}

/* ── 2. alloc 128B, free, re-alloc 128B ─────────────────────── */
static void test_ctlsf_alloc_free_realloc(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CTLSF: alloc 128B, free, re-alloc 128B");
  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *a = scl_calloc_tlsf_create(backing, 65536, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  void *p1 = scl_alloc(a, 128, 0);
  SCL_EXPECT_NOT_NULL(tr, p1);
  scl_memset(p1, 0xAB, 128);
  scl_free(a, p1);

  /* Re-allocating the same size should succeed — and reusing the
   * just-freed block (p2 == p1) is the *desired* freelist behaviour,
   * so the address must not be asserted either way. */
  void *p2 = scl_alloc(a, 128, 0);
  SCL_EXPECT_NOT_NULL(tr, p2);
  scl_memset(p2, 0x5A, 128); /* block must be writable */
  scl_free(a, p2);

  scl_calloc_tlsf_destroy(a);
  TEST_TRACE_END();
}

/* ── 3. varied sizes ────────────────────────────────────────── */
static void test_ctlsf_varied_sizes(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CTLSF: varied sizes alloc/free");
  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *a = scl_calloc_tlsf_create(backing, 1 << 20, 0); /* 1 MB */
  SCL_EXPECT_NOT_NULL(tr, a);

  static const size_t sizes[] = {8, 16, 64, 128, 256, 512, 1024, 4096};
  enum { NSZ = (int)(sizeof(sizes) / sizeof(sizes[0])) };

  void *ptrs[NSZ];
  for (int i = 0; i < NSZ; i++) {
    ptrs[i] = scl_alloc(a, sizes[i], 0);
    SCL_EXPECT_NOT_NULL(tr, ptrs[i]);
    /* Verify max_align_t alignment of every returned pointer. */
    SCL_EXPECT_TRUE(tr, ((uintptr_t)ptrs[i] % alignof(max_align_t)) == 0);
    /* Scribble to confirm the region is writable & distinct. */
    scl_memset(ptrs[i], (int)(i + 1), sizes[i]);
  }

  /* Distinct allocations must not alias. */
  for (int i = 0; i < NSZ; i++)
    for (int j = i + 1; j < NSZ; j++)
      SCL_EXPECT_TRUE(tr, ptrs[i] != ptrs[j]);

  for (int i = 0; i < NSZ; i++)
    scl_free(a, ptrs[i]);

  /* After freeing everything, a large alloc should still succeed
   * (coalescing rebuilt the big free block). */
  void *big = scl_alloc(a, 1 << 16, 0); /* 64 KB */
  SCL_EXPECT_NOT_NULL(tr, big);
  scl_free(a, big);

  scl_calloc_tlsf_destroy(a);
  TEST_TRACE_END();
}

/* ── 4. concurrent alloc/free ───────────────────────────────── */
typedef struct {
  scl_allocator_t *alloc;
  int iters;
} ctlsf_concurrent_ctx_t;

static void *ctlsf_worker(void *arg) {
  scl_test_thread_arg_t *targ = (scl_test_thread_arg_t *)arg;
  ctlsf_concurrent_ctx_t *ctx = (ctlsf_concurrent_ctx_t *)targ->user_data;
  scl_allocator_t *a = ctx->alloc;

  /* Rendezvous so all threads hit the allocator simultaneously. */
  scl_test_barrier_wait(targ->barrier);

  for (int i = 0; i < ctx->iters; i++) {
    /* Vary the size between 64 and 192 bytes to exercise many bins. */
    size_t sz = 64 + (size_t)((i * 7 + targ->thread_id * 13) % 129);
    void *p = scl_alloc(a, sz, 0);
    if (!SCL_CC_EXPECT(targ->cc, p != NULL)) {
      sched_yield();
      continue;
    }
    /* Touch the memory so sanitizers catch any overlap. */
    scl_memset(p, (int)(targ->thread_id & 0xff), sz);
    scl_free(a, p);
  }
  return NULL;
}

static void test_ctlsf_concurrent(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CTLSF: 4 threads x 100 iterations alloc/free (2 MB pool)");
  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *a = scl_calloc_tlsf_create(backing, 2 * 1024 * 1024, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  ctlsf_concurrent_ctx_t ctx = {.alloc = a, .iters = 100};
  int failed = scl_test_run_concurrent(tr, 4, ctlsf_worker, &ctx);
  SCL_EXPECT_EQ_I(tr, failed, 0);

  scl_calloc_tlsf_destroy(a);
  TEST_TRACE_END();
}

/* ── main ──────────────────────────────────────────────────── */
int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_ctlsf_init_destroy(&tr);
  test_ctlsf_alloc_free_realloc(&tr);
  test_ctlsf_varied_sizes(&tr);
  test_ctlsf_concurrent(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
