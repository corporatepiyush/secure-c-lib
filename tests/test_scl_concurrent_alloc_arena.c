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

/* Tests for the concurrent sharded arena allocator.
 *
 * Covers:
 *   1. init / destroy
 *   2. sequential allocation (1000 × 32B in a single-shard arena)
 *   3. reset semantics (alloc, reset, verify the same base pointer is reused)
 *   4. concurrent allocation: 4 threads × 500 allocs via
 * scl_test_run_concurrent
 */

#include "scl_concurrent_alloc_arena.h"
#include "scl_string.h"
#include "scl_test.h"

#define SEQ_ALLOCS 1000
#define SEQ_ALLOC_SZ 32
#define CONC_THREADS 4
#define CONC_PER_THREAD 500

/* ── 1. init / destroy ──────────────────────────────────────── */
static void test_carena_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CArena: init and destroy");

  scl_allocator_t *backing = scl_allocator_default();
  SCL_EXPECT_NOT_NULL(tr, backing);

  scl_allocator_t *a = scl_calloc_arena_create(backing, 4096, 0, 4, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  /* A fresh arena must hand out a valid, aligned pointer. */
  void *p = scl_alloc(a, 64, 0);
  SCL_EXPECT_NOT_NULL(tr, p);
  SCL_EXPECT_TRUE(tr, ((uintptr_t)p % alignof(max_align_t)) == 0);

  scl_calloc_arena_destroy(a);
  TEST_TRACE_END();
}

/* ── 2. sequential allocation in a single-shard arena ───────── */
static void test_carena_sequential(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CArena: sequential allocation");

  scl_allocator_t *backing = scl_allocator_default();
  /* Single shard forces all allocations onto the same bump pointer,
   * exercising the alignment + offset arithmetic densely. */
  scl_allocator_t *a = scl_calloc_arena_create(backing, 64 * 1024, 0, 1, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  void *prev = NULL;
  for (int i = 0; i < SEQ_ALLOCS; i++) {
    void *p = scl_alloc(a, SEQ_ALLOC_SZ, 0);
    SCL_EXPECT_NOT_NULL(tr, p);

    /* Each allocation must be max_align_t-aligned. */
    SCL_EXPECT_TRUE(tr, ((uintptr_t)p % alignof(max_align_t)) == 0);

    /* Bump pointer must move strictly forward. */
    if (prev)
      SCL_EXPECT_TRUE(tr, (uintptr_t)p > (uintptr_t)prev);
    prev = p;

    /* Scribble to catch obvious buffer overlap. */
    scl_memset(p, (int)(i & 0xFF), SEQ_ALLOC_SZ);
  }

  scl_calloc_arena_destroy(a);
  TEST_TRACE_END();
}

/* ── 3. reset semantics ──────────────────────────────────────── */
static void test_carena_reset(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CArena: reset rewinds bump pointer");

  scl_allocator_t *backing = scl_allocator_default();
  scl_allocator_t *a = scl_calloc_arena_create(backing, 8192, 0, 1, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  /* Allocate something so the bump pointer advances. */
  void *p1 = scl_alloc(a, 128, 0);
  SCL_EXPECT_NOT_NULL(tr, p1);
  scl_memset(p1, 0xAB, 128);

  /* Reset must rewind the bump pointer to the start of the buffer. */
  scl_calloc_arena_reset(a);

  /* The next allocation should reuse the same base address. */
  void *p2 = scl_alloc(a, 128, 0);
  SCL_EXPECT_NOT_NULL(tr, p2);
  SCL_EXPECT_EQ_PTR(tr, p1, p2);

  scl_calloc_arena_destroy(a);
  TEST_TRACE_END();
}

/* ── 4. concurrent allocation ────────────────────────────────── */
typedef struct {
  scl_allocator_t *arena;
} carena_concurrent_ctx_t;

static void *carena_concurrent_body(void *arg) {
  scl_test_thread_arg_t *targ = (scl_test_thread_arg_t *)arg;
  carena_concurrent_ctx_t *ctx = (carena_concurrent_ctx_t *)targ->user_data;

  /* Rendezvous so all threads hit the allocator simultaneously. */
  scl_test_barrier_wait(targ->barrier);

  for (int i = 0; i < CONC_PER_THREAD; i++) {
    void *p = scl_alloc(ctx->arena, 32, 0);
    if (!SCL_CC_EXPECT(targ->cc, p != NULL))
      continue;

    /* Alignment invariant. */
    if (!SCL_CC_EXPECT(targ->cc, ((uintptr_t)p % alignof(max_align_t)) == 0))
      continue;

    /* Write a thread-tagged pattern; if the arena ever hands out
     * overlapping memory, a later thread will stomp our bytes. */
    scl_memset(p, (int)(targ->thread_id & 0xFF), 32);
  }

  return NULL;
}

static void test_carena_concurrent(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CArena: concurrent allocation (4 threads × 500)");

  scl_allocator_t *backing = scl_allocator_default();
  /* 4 shards match the 4 threads → ideally each thread owns a shard. */
  scl_allocator_t *a = scl_calloc_arena_create(backing, 256 * 1024, 0, 4, 0);
  SCL_EXPECT_NOT_NULL(tr, a);

  carena_concurrent_ctx_t ctx = {.arena = a};

  int failed_to_spawn =
      scl_test_run_concurrent(tr, CONC_THREADS, carena_concurrent_body, &ctx);
  SCL_EXPECT_EQ_I(tr, failed_to_spawn, 0);

  scl_calloc_arena_destroy(a);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_carena_init_destroy(&tr);
  test_carena_sequential(&tr);
  test_carena_reset(&tr);
  test_carena_concurrent(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
