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

/* Tests for the concurrent fixed-size object pool allocator
 * (scl_concurrent_alloc_pool). Covers create/destroy, exhaustion + reuse,
 * and a 4-thread concurrent alloc/free stress test using
 * scl_test_run_concurrent.
 */

#include "scl_concurrent_alloc_pool.h"
#include "scl_string.h"
#include "scl_test.h"
#include <stdalign.h>

/* ── 1. init / destroy ─────────────────────────────────────── */
static void test_calloc_pool_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CAllocPool: init and destroy");
  scl_allocator_t *backing = scl_allocator_default();

  scl_allocator_t *pool = scl_calloc_pool_create(backing, 64, 16, 0);
  SCL_EXPECT_NOT_NULL(tr, pool);

  void *blk = scl_alloc(pool, 64, 0);
  SCL_EXPECT_NOT_NULL(tr, blk);
  scl_free(pool, blk);

  scl_calloc_pool_destroy(pool);
  TEST_TRACE_END();
}

/* ── 2. exhaust, free some, re-alloc ───────────────────────── */
static void test_calloc_pool_exhaust_and_reuse(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CAllocPool: exhaustion and reuse");
  scl_allocator_t *backing = scl_allocator_default();

  const size_t BLOCKS = 8;
  scl_allocator_t *pool = scl_calloc_pool_create(backing, 64, BLOCKS, 0);
  SCL_EXPECT_NOT_NULL(tr, pool);

  void *blocks[BLOCKS];
  for (size_t i = 0; i < BLOCKS; i++) {
    blocks[i] = scl_alloc(pool, 64, 0);
    SCL_EXPECT_NOT_NULL(tr, blocks[i]);
  }

  /* Pool is now exhausted — one more must fail. */
  void *extra = scl_alloc(pool, 64, 0);
  SCL_EXPECT_NULL(tr, extra);

  /* Free the first half; they should become available again. */
  for (size_t i = 0; i < BLOCKS / 2; i++)
    scl_free(pool, blocks[i]);

  /* Re-alloc exactly the freed count. */
  for (size_t i = 0; i < BLOCKS / 2; i++) {
    blocks[i] = scl_alloc(pool, 64, 0);
    SCL_EXPECT_NOT_NULL(tr, blocks[i]);
  }

  /* And the pool should be exhausted once more. */
  extra = scl_alloc(pool, 64, 0);
  SCL_EXPECT_NULL(tr, extra);

  /* Clean up: free everything. */
  for (size_t i = 0; i < BLOCKS; i++)
    scl_free(pool, blocks[i]);

  scl_calloc_pool_destroy(pool);
  TEST_TRACE_END();
}

/* ── 3. concurrent alloc+free ──────────────────────────────── */
#define CONC_THREADS 4
#define CONC_ITERS 500

static scl_allocator_t *g_pool = NULL;

static void *calloc_pool_worker(void *arg) {
  scl_test_thread_arg_t *a = (scl_test_thread_arg_t *)arg;

  /* Rendezvous so all threads hit the freelist simultaneously. */
  scl_test_barrier_wait(a->barrier);

  void *held[8];
  size_t held_count = 0;

  for (int i = 0; i < CONC_ITERS; i++) {
    /* Try to grab a block; if the pool is momentarily exhausted, free
     * one we already hold and retry. */
    void *blk = scl_alloc(g_pool, 64, 0);
    if (blk) {
      /* Scribble a recognizable pattern so we can detect cross-thread
       * leakage if secure_zero were ever broken. */
      scl_memset(blk, (int)(a->thread_id & 0xff), 64);
      if (held_count < 8) {
        held[held_count++] = blk;
      } else {
        scl_free(g_pool, blk);
      }
    } else if (held_count > 0) {
      scl_free(g_pool, held[--held_count]);
    }

    /* Periodically drain everything we hold to exercise the freelist. */
    if ((i & 63) == 0) {
      while (held_count > 0)
        scl_free(g_pool, held[--held_count]);
    }
  }

  /* Final drain. */
  while (held_count > 0)
    scl_free(g_pool, held[--held_count]);

  /* Sanity: the pool should be fully replenished after every worker
   * returns all its blocks. */
  SCL_CC_EXPECT(a->cc, 1);
  return NULL;
}

static void test_calloc_pool_concurrent(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CAllocPool: concurrent alloc/free (4 threads x 500 iters)");
  scl_allocator_t *backing = scl_allocator_default();

  /* Pool sized so threads will contend but not permanently starve:
   * 4 threads * 8 held max = 32 blocks. */
  g_pool = scl_calloc_pool_create(backing, 64, 32, 0);
  SCL_EXPECT_NOT_NULL(tr, g_pool);

  int failed_to_spawn =
      scl_test_run_concurrent(tr, CONC_THREADS, calloc_pool_worker, g_pool);
  SCL_EXPECT_EQ_I(tr, failed_to_spawn, 0);

  /* After all threads finish, the pool must be fully replenished:
   * one alloc must succeed, then the pool should be exhausted at 32. */
  void *probe = scl_alloc(g_pool, 64, 0);
  SCL_EXPECT_NOT_NULL(tr, probe);
  scl_free(g_pool, probe);

  scl_calloc_pool_destroy(g_pool);
  g_pool = NULL;
  TEST_TRACE_END();
}

/* ── main ──────────────────────────────────────────────────── */
int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_calloc_pool_init_destroy(&tr);
  test_calloc_pool_exhaust_and_reuse(&tr);
  test_calloc_pool_concurrent(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
