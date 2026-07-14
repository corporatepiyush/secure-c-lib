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

/* Test assertion macros:
 * expect_true/false/eq_i/eq_u/eq_str/eq_mem/eq_ptr/null/not_null/lt/gt/le/ge.
 * File+line reporting via __FILE__/__LINE__. */

#ifndef SCL_TEST_H
#define SCL_TEST_H

#include "scl_atomic.h"
#include "scl_common.h"
#include "scl_pthread.h"
#include "scl_stdio.h"
#include "scl_string.h"
#include <time.h>

/* ── Timestamp helper ─────────────────────────────────────────
 * Returns a static buffer with the current time as HH:MM:SS.mmm.
 * Not thread-safe, but sufficient for test output serialization. */
static inline const char *scl_test_timestamp(void) {
  static char buf[32];
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  long ms = ts.tv_nsec / 1000000L;
  struct tm *tm_info = localtime(&ts.tv_sec);
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld", tm_info->tm_hour,
           tm_info->tm_min, tm_info->tm_sec, ms);
  return buf;
}

/* ── Test state ─────────────────────────────────────────────── */
typedef struct {
  int passed;
  int failed;
  int asserts;
} scl_test_runner_t;

void scl_test_init(scl_test_runner_t *tr);

/* ── Test naming ────────────────────────────────────────────── */
void scl_test_group(const char *name);

/* ── Assertions (return true on success, false on failure) ─── */
bool scl_expect_true(scl_test_runner_t *tr, bool cond, const char *file,
                     int line, const char *expr);
bool scl_expect_false(scl_test_runner_t *tr, bool cond, const char *file,
                      int line, const char *expr);
bool scl_expect_eq_i(scl_test_runner_t *tr, int64_t a, int64_t b,
                     const char *file, int line);
bool scl_expect_eq_u(scl_test_runner_t *tr, uint64_t a, uint64_t b,
                     const char *file, int line);
bool scl_expect_eq_sz(scl_test_runner_t *tr, size_t a, size_t b,
                      const char *file, int line);
bool scl_expect_eq_ptr(scl_test_runner_t *tr, const void *a, const void *b,
                       const char *file, int line);
bool scl_expect_eq_str(scl_test_runner_t *tr, const char *a, const char *b,
                       const char *file, int line);
bool scl_expect_eq_mem(scl_test_runner_t *tr, const void *a, const void *b,
                       size_t n, const char *file, int line);
bool scl_expect_null(scl_test_runner_t *tr, const void *p, const char *file,
                     int line);
bool scl_expect_not_null(scl_test_runner_t *tr, const void *p, const char *file,
                         int line);
bool scl_expect_ok(scl_test_runner_t *tr, scl_error_t err, const char *file,
                   int line);
bool scl_expect_error(scl_test_runner_t *tr, scl_error_t err,
                      scl_error_t expected, const char *file, int line);

/* ── Summary ────────────────────────────────────────────────── */
void scl_test_summary(scl_test_runner_t *tr);

/* Convenience macros (file/line captured automatically) */
#define SCL_EXPECT_TRUE(tr, cond)                                              \
  scl_expect_true(tr, !!(cond), __FILE__, __LINE__, #cond)
#define SCL_EXPECT_FALSE(tr, cond)                                             \
  scl_expect_false(tr, !!(cond), __FILE__, __LINE__, #cond)
#define SCL_EXPECT_EQ_I(tr, a, b)                                              \
  scl_expect_eq_i(tr, (int64_t)(a), (int64_t)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_U(tr, a, b)                                              \
  scl_expect_eq_u(tr, (uint64_t)(a), (uint64_t)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_SZ(tr, a, b)                                             \
  scl_expect_eq_sz(tr, (size_t)(a), (size_t)(b), __FILE__, __LINE__)
#define SCL_EXPECT_EQ_PTR(tr, a, b)                                            \
  scl_expect_eq_ptr(tr, (const void *)(a), (const void *)(b), __FILE__,        \
                    __LINE__)
#define SCL_EXPECT_EQ_STR(tr, a, b)                                            \
  scl_expect_eq_str(tr, (a), (b), __FILE__, __LINE__)
#define SCL_EXPECT_NULL(tr, p) scl_expect_null(tr, (p), __FILE__, __LINE__)
#define SCL_EXPECT_NOT_NULL(tr, p)                                             \
  scl_expect_not_null(tr, (p), __FILE__, __LINE__)
#define SCL_EXPECT_OK(tr, err) scl_expect_ok(tr, (err), __FILE__, __LINE__)
#define SCL_EXPECT_ERROR(tr, err, exp)                                         \
  scl_expect_error(tr, (err), (exp), __FILE__, __LINE__)

/* ── Per-test-case debug tracing ──────────────────────────────────── */
#define TEST_TRACE_BEGIN()                                                     \
  do {                                                                         \
    fprintf(stdout, "  [%s] >>> %s:%d: BEGIN\n", scl_test_timestamp(),         \
            __FILE__, __LINE__);                                               \
    fflush(stdout);                                                            \
  } while (0)

#define TEST_TRACE_END()                                                       \
  do {                                                                         \
    fprintf(stdout, "  [%s] <<< %s:%d: END\n", scl_test_timestamp(), __FILE__, \
            __LINE__);                                                         \
    fflush(stdout);                                                            \
  } while (0)

/* ── Concurrent test helpers ───────────────────────────────── */

/* Thread-safe assertion counters (for concurrent tests) */
typedef struct {
  atomic_int passed;
  atomic_int failed;
  atomic_int asserts;
} scl_test_concurrent_counters_t;

static inline void scl_test_cc_init(scl_test_concurrent_counters_t *c) {
  atomic_init(&c->passed, 0);
  atomic_init(&c->failed, 0);
  atomic_init(&c->asserts, 0);
}

/* Atomically record a result; returns true on pass, false on fail */
static inline bool scl_test_cc_record(scl_test_concurrent_counters_t *c,
                                      bool ok, const char *file, int line,
                                      const char *msg) {
  atomic_fetch_add_explicit(&c->asserts, 1, memory_order_relaxed);
  if (ok) {
    atomic_fetch_add_explicit(&c->passed, 1, memory_order_relaxed);
  } else {
    atomic_fetch_add_explicit(&c->failed, 1, memory_order_relaxed);
    fprintf(stderr, "  FAIL %s:%d: %s\n", file, line, msg);
  }
  return ok;
}

/* Merge concurrent counters into a test runner */
static inline void scl_test_cc_merge(scl_test_runner_t *tr,
                                     scl_test_concurrent_counters_t *c) {
  tr->passed += atomic_load_explicit(&c->passed, memory_order_relaxed);
  tr->failed += atomic_load_explicit(&c->failed, memory_order_relaxed);
  tr->asserts += atomic_load_explicit(&c->asserts, memory_order_relaxed);
}

/* Spin-based barrier for synchronizing threads in tests */
typedef struct {
  atomic_int count;
  int total;
  atomic_int phase;
} scl_test_barrier_t;

static inline void scl_test_barrier_init(scl_test_barrier_t *b, int count) {
  atomic_init(&b->count, count);
  b->total = count;
  atomic_init(&b->phase, 0);
}

static inline void scl_test_barrier_wait(scl_test_barrier_t *b) {
  int phase = atomic_load_explicit(&b->phase, memory_order_acquire);
  int prev = atomic_fetch_sub_explicit(&b->count, 1, memory_order_acq_rel) - 1;
  if (prev > 0) {
    while (atomic_load_explicit(&b->phase, memory_order_acquire) == phase)
      scl_cpu_pause();
  } else {
    atomic_store_explicit(&b->count, b->total, memory_order_release);
    atomic_store_explicit(&b->phase, phase + 1, memory_order_release);
  }
}

/* Standard argument struct for thread functions in concurrent tests */
typedef struct {
  int thread_id;
  int nthreads;
  scl_test_barrier_t *barrier;
  scl_test_concurrent_counters_t *cc;
  void *user_data;
} scl_test_thread_arg_t;

/* Record an assertion onto a concurrent counter set (thread-safe). */
#define SCL_CC_EXPECT(cc, cond)                                                \
  scl_test_cc_record((cc), !!(cond), __FILE__, __LINE__, #cond)

/* ── One-call concurrent test driver ───────────────────────────
 * Spawns `nthreads` running `body`, each receiving an scl_test_thread_arg_t
 * with a shared barrier (so they can rendezvous before the hot section),
 * shared concurrent counters, its own thread_id, and `user_data`. Joins all
 * threads and merges the pass/fail counts into the runner. The body should
 * use SCL_CC_EXPECT(arg->cc, ...) for its assertions and may call
 * scl_test_barrier_wait(arg->barrier) to maximize contention.
 *
 * Returns the number of threads that failed to spawn (0 on full success). */
static inline int scl_test_run_concurrent(scl_test_runner_t *tr, int nthreads,
                                          void *(*body)(void *),
                                          void *user_data) {
  if (nthreads < 1)
    nthreads = 1;
  if (nthreads > 1024)
    nthreads = 1024;

  scl_test_concurrent_counters_t cc;
  scl_test_barrier_t barrier;
  scl_test_cc_init(&cc);
  scl_test_barrier_init(&barrier, nthreads);

  pthread_t *threads = (pthread_t *)calloc((size_t)nthreads, sizeof(pthread_t));
  scl_test_thread_arg_t *args = (scl_test_thread_arg_t *)calloc(
      (size_t)nthreads, sizeof(scl_test_thread_arg_t));
  if (!threads || !args) {
    free(threads);
    free(args);
    tr->failed++; /* infrastructure failure counts against the suite */
    return nthreads;
  }

  int spawned = 0;
  for (int i = 0; i < nthreads; i++) {
    args[i].thread_id = i;
    args[i].nthreads = nthreads;
    args[i].barrier = &barrier;
    args[i].cc = &cc;
    args[i].user_data = user_data;
    if (pthread_create(&threads[i], NULL, body, &args[i]) == 0)
      spawned++;
    else
      threads[i] = (pthread_t)0;
  }

  for (int i = 0; i < nthreads; i++)
    if (threads[i])
      pthread_join(threads[i], NULL);

  scl_test_cc_merge(tr, &cc);
  free(threads);
  free(args);
  return nthreads - spawned;
}

#endif
