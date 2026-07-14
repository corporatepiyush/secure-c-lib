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

/* Tests for lock-free concurrent buddy allocator. */

#include "scl_concurrent_alloc_buddy.h"
#include "scl_string.h"
#include "scl_test.h"
#include <stdalign.h>

static void test_cbuddy_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("ConcurrentBuddy: init and destroy");

  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *buddy = scl_calloc_buddy_create(backing, 64 * 1024, 0);

  SCL_EXPECT_NOT_NULL(tr, buddy);

  /* Single alloc/free */
  void *p = scl_alloc(buddy, 256, 0);
  SCL_EXPECT_NOT_NULL(tr, p);
  scl_free(buddy, p);

  scl_calloc_buddy_destroy(buddy);
  TEST_TRACE_END();
}

static void test_cbuddy_alloc_free(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("ConcurrentBuddy: sequential alloc/free");

  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *buddy = scl_calloc_buddy_create(backing, 256 * 1024, 0);

  SCL_EXPECT_NOT_NULL(tr, buddy);

  void *ptrs[16];
  for (int i = 0; i < 16; i++) {
    ptrs[i] = scl_alloc(buddy, 128 + i * 16, 0);
    SCL_EXPECT_NOT_NULL(tr, ptrs[i]);
    scl_memset(ptrs[i], (int)(i & 0xFF), 128 + i * 16);
  }

  for (int i = 0; i < 16; i++) {
    scl_free(buddy, ptrs[i]);
  }

  /* Verify pool is replenished */
  void *verify = scl_alloc(buddy, 512, 0);
  SCL_EXPECT_NOT_NULL(tr, verify);
  scl_free(buddy, verify);

  scl_calloc_buddy_destroy(buddy);
  TEST_TRACE_END();
}

#define CBUDDY_THREADS 4
#define CBUDDY_ITERS 100

static scl_allocator_t *g_buddy = NULL;

static void *cbuddy_worker(void *arg) {
  scl_test_thread_arg_t *a = (scl_test_thread_arg_t *)arg;

  scl_test_barrier_wait(a->barrier);

  void *held[8];
  size_t held_count = 0;

  for (int i = 0; i < CBUDDY_ITERS; i++) {
    size_t sz = 64 + (a->thread_id * 13 + i * 7) % 512;
    void *p = scl_alloc(g_buddy, sz, 0);

    if (p) {
      scl_memset(p, (int)(a->thread_id & 0xFF), sz);
      if (held_count < 8) {
        held[held_count++] = p;
      } else {
        scl_free(g_buddy, p);
      }
    } else if (held_count > 0) {
      scl_free(g_buddy, held[--held_count]);
    }

    if ((i & 15) == 0) {
      while (held_count > 0)
        scl_free(g_buddy, held[--held_count]);
    }
  }

  while (held_count > 0)
    scl_free(g_buddy, held[--held_count]);

  SCL_CC_EXPECT(a->cc, 1);
  return NULL;
}

static void test_cbuddy_concurrent(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group(
      "ConcurrentBuddy: lock-free concurrent (4 threads x 100 iters)");

  scl_allocator_t *backing = scl_allocator_default();
  g_buddy = scl_calloc_buddy_create(backing, 2 * 1024 * 1024, 0);

  SCL_EXPECT_NOT_NULL(tr, g_buddy);

  int failed =
      scl_test_run_concurrent(tr, CBUDDY_THREADS, cbuddy_worker, g_buddy);
  SCL_EXPECT_EQ_I(tr, failed, 0);

  /* Verify pool still functional after concurrent load */
  void *probe = scl_alloc(g_buddy, 1024, 0);
  SCL_EXPECT_NOT_NULL(tr, probe);
  scl_free(g_buddy, probe);

  scl_calloc_buddy_destroy(g_buddy);
  g_buddy = NULL;
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cbuddy_init_destroy(&tr);
  test_cbuddy_alloc_free(&tr);
  test_cbuddy_concurrent(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
