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

#include "scl_concurrent_sharded_array.h"
#include "scl_test.h"
#include <pthread.h>

#define CSA_THREADS 4
#define CSA_PER_THREAD 256

static void test_csa_init_destroy(scl_test_runner_t *tr) {
  scl_test_group("CSA: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_sharded_array_t csa;
  SCL_EXPECT_OK(tr, scl_csa_init(alloc, &csa, sizeof(int), 256));
  SCL_EXPECT_EQ_SZ(tr, scl_csa_count(&csa), 0);
  scl_csa_destroy(alloc, &csa);
}

static void test_csa_append_get(scl_test_runner_t *tr) {
  scl_test_group("CSA: append and get");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_sharded_array_t csa;
  scl_csa_init(alloc, &csa, sizeof(int), 4);

  for (int i = 0; i < 10; i++) {
    size_t idx;
    SCL_EXPECT_OK(tr, scl_csa_append(alloc, &csa, &i, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, (size_t)i);
  }

  SCL_EXPECT_EQ_SZ(tr, scl_csa_count(&csa), 10);

  for (int i = 0; i < 10; i++) {
    int *val = scl_csa_get(&csa, (size_t)i);
    SCL_EXPECT_NOT_NULL(tr, val);
    SCL_EXPECT_EQ_I(tr, *val, i);
  }

  scl_csa_destroy(alloc, &csa);
}

static void test_csa_small_shard(scl_test_runner_t *tr) {
  scl_test_group("CSA: small shard triggers growth");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_sharded_array_t csa;
  scl_csa_init(alloc, &csa, sizeof(int), 2);

  for (int i = 0; i < 10; i++) {
    size_t idx;
    SCL_EXPECT_OK(tr, scl_csa_append(alloc, &csa, &i, &idx));
  }

  SCL_EXPECT_EQ_SZ(tr, scl_csa_count(&csa), 10);

  for (int i = 0; i < 10; i++) {
    int *val = scl_csa_get(&csa, (size_t)i);
    SCL_EXPECT_NOT_NULL(tr, val);
    SCL_EXPECT_EQ_I(tr, *val, i);
  }

  scl_csa_destroy(alloc, &csa);
}

typedef struct {
  scl_concurrent_sharded_array_t *csa;
  int thread_id;
  int nthreads;
  scl_test_barrier_t *barrier;
  scl_test_concurrent_counters_t *cc;
} csa_thread_arg_t;

static void *csa_concurrent_append_thread(void *arg) {
  csa_thread_arg_t *a = arg;
  scl_allocator_t *alloc = scl_allocator_default();

  scl_test_barrier_wait(a->barrier);

  for (int i = 0; i < CSA_PER_THREAD; i++) {
    int val = a->thread_id * CSA_PER_THREAD + i;
    size_t idx;
    if (!scl_csa_append(alloc, a->csa, &val, &idx)) {
      /* idx is a global sequence number: with N threads interleaving it
       * can land anywhere below the total number of appends. Uniqueness
       * is verified after the join via the found[] sweep. */
      SCL_CC_EXPECT(a->cc, idx < (size_t)a->nthreads * CSA_PER_THREAD);
    } else {
      SCL_CC_EXPECT(a->cc, false && "append failed");
    }
  }

  return NULL;
}

static void test_csa_concurrent(scl_test_runner_t *tr) {
  scl_test_group("CSA: concurrent append from multiple threads");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_sharded_array_t csa;
  SCL_EXPECT_OK(tr, scl_csa_init(alloc, &csa, sizeof(int), 64));

  scl_test_concurrent_counters_t cc;
  scl_test_cc_init(&cc);

  scl_test_barrier_t barrier;
  scl_test_barrier_init(&barrier, CSA_THREADS);

  pthread_t threads[CSA_THREADS];
  csa_thread_arg_t args[CSA_THREADS];

  for (int i = 0; i < CSA_THREADS; i++) {
    args[i].csa = &csa;
    args[i].thread_id = i;
    args[i].nthreads = CSA_THREADS;
    args[i].barrier = &barrier;
    args[i].cc = &cc;
    pthread_create(&threads[i], NULL, csa_concurrent_append_thread, &args[i]);
  }

  for (int i = 0; i < CSA_THREADS; i++)
    pthread_join(threads[i], NULL);

  scl_test_cc_merge(tr, &cc);

  SCL_EXPECT_EQ_SZ(tr, scl_csa_count(&csa),
                   (size_t)CSA_THREADS * CSA_PER_THREAD);

  /* Verify every value exists */
  int found[CSA_THREADS * CSA_PER_THREAD] = {0};
  for (size_t i = 0; i < scl_csa_count(&csa); i++) {
    int *val = scl_csa_get(&csa, i);
    if (val && *val >= 0 && *val < CSA_THREADS * CSA_PER_THREAD)
      found[*val] = 1;
  }
  int all_found = 1;
  for (int i = 0; i < CSA_THREADS * CSA_PER_THREAD; i++)
    all_found &= found[i];
  SCL_EXPECT_TRUE(tr, all_found);

  scl_csa_destroy(alloc, &csa);
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_csa_init_destroy(&tr);
  test_csa_append_get(&tr);
  test_csa_small_shard(&tr);
  test_csa_concurrent(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}