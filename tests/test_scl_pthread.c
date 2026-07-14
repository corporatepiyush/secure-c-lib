#include "scl_pthread.h"
#include "scl_test.h"

/* ── Helper: increment shared counter ───────────────────────── */
typedef struct {
  scl_mutex_t *lock;
  int *counter;
} worker_arg_t;

static void *worker_fn(void *arg) {
  worker_arg_t *wa = (worker_arg_t *)arg;
  for (int i = 0; i < 100; i++) {
    scl_mutex_lock(wa->lock);
    (*wa->counter)++;
    scl_mutex_unlock(wa->lock);
  }
  return NULL;
}

static void test_mutex_create_join(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_mutex / thread create/join");

  scl_mutex_t lock;
  SCL_EXPECT_OK(tr, scl_mutex_init(&lock));

  int counter = 0;
  worker_arg_t wa = {&lock, &counter};

  scl_thread_t t1, t2;
  SCL_EXPECT_OK(tr, scl_thread_create(&t1, worker_fn, &wa));
  SCL_EXPECT_OK(tr, scl_thread_create(&t2, worker_fn, &wa));

  SCL_EXPECT_OK(tr, scl_thread_join(t1, NULL));
  SCL_EXPECT_OK(tr, scl_thread_join(t2, NULL));

  SCL_EXPECT_EQ_I(tr, counter, 200);

  scl_mutex_destroy(&lock);
  TEST_TRACE_END();
}

/* ── Recursive mutex ────────────────────────────────────────── */
static void test_recursive_mutex(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_mutex recursive init");

  scl_mutex_t m;
  SCL_EXPECT_OK(tr, scl_mutex_init_recursive(&m));

  /* single thread: lock twice, unlock twice */
  SCL_EXPECT_OK(tr, scl_mutex_lock(&m));
  SCL_EXPECT_OK(tr, scl_mutex_lock(&m));
  SCL_EXPECT_OK(tr, scl_mutex_unlock(&m));
  SCL_EXPECT_OK(tr, scl_mutex_unlock(&m));

  scl_mutex_destroy(&m);
  TEST_TRACE_END();
}

/* ── Trylock ────────────────────────────────────────────────── */
static void test_trylock(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_mutex trylock");

  scl_mutex_t m;
  scl_mutex_init(&m);

  SCL_EXPECT_OK(tr, scl_mutex_trylock(&m));
  SCL_EXPECT_EQ_I(tr, scl_mutex_trylock(&m) != SCL_OK,
                  1); /* already held (debug) or EBUSY */
  scl_mutex_unlock(&m);
  SCL_EXPECT_OK(tr, scl_mutex_trylock(&m));
  scl_mutex_unlock(&m);

  scl_mutex_destroy(&m);
  TEST_TRACE_END();
}

/* ── Helper for condvar test ────────────────────────────────── */
typedef struct {
  scl_mutex_t *lock;
  scl_cond_t *cond;
  int *flag;
} cv_arg_t;

static void *waiter_fn(void *arg) {
  cv_arg_t *ca = (cv_arg_t *)arg;
  scl_mutex_lock(ca->lock);
  while (*(ca->flag) == 0)
    scl_cond_wait(ca->cond, ca->lock);
  scl_mutex_unlock(ca->lock);
  return NULL;
}

static void *signaler_fn(void *arg) {
  cv_arg_t *ca = (cv_arg_t *)arg;
  scl_mutex_lock(ca->lock);
  *(ca->flag) = 1;
  scl_cond_signal(ca->cond);
  scl_mutex_unlock(ca->lock);
  return NULL;
}

static void test_condvar(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_cond wait / signal");

  scl_mutex_t lock;
  scl_cond_t cond;
  SCL_EXPECT_OK(tr, scl_mutex_init(&lock));
  SCL_EXPECT_OK(tr, scl_cond_init(&cond));

  int flag = 0;
  cv_arg_t ca = {&lock, &cond, &flag};

  scl_thread_t waiter_t, signaler_t;
  SCL_EXPECT_OK(tr, scl_thread_create(&waiter_t, waiter_fn, &ca));
  SCL_EXPECT_OK(tr, scl_thread_create(&signaler_t, signaler_fn, &ca));

  SCL_EXPECT_OK(tr, scl_thread_join(waiter_t, NULL));
  SCL_EXPECT_OK(tr, scl_thread_join(signaler_t, NULL));

  SCL_EXPECT_EQ_I(tr, flag, 1);

  scl_cond_destroy(&cond);
  scl_mutex_destroy(&lock);
  TEST_TRACE_END();
}

/* ── Timed wait ─────────────────────────────────────────────── */
static void test_cond_timedwait_timeout(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_cond timed wait timeout");

  scl_mutex_t lock;
  scl_cond_t cond;
  scl_mutex_init(&lock);
  scl_cond_init(&cond);

  scl_mutex_lock(&lock);
  scl_error_t err = scl_cond_wait_for(&cond, &lock, 10); /* 10ms */
  SCL_EXPECT_EQ_I(tr, err, SCL_ERR_TIMEOUT);
  scl_mutex_unlock(&lock);

  scl_cond_destroy(&cond);
  scl_mutex_destroy(&lock);
  TEST_TRACE_END();
}

/* ── NULL error paths ───────────────────────────────────────── */
static void test_null_errors(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("NULL pointer errors");

  SCL_EXPECT_ERROR(tr, scl_mutex_lock(NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_mutex_unlock(NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_mutex_init(NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_mutex_init_recursive(NULL), SCL_ERR_NULL_PTR);

  SCL_EXPECT_ERROR(tr, scl_cond_wait(NULL, NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_cond_signal(NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_cond_broadcast(NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_cond_init(NULL), SCL_ERR_NULL_PTR);

  SCL_EXPECT_ERROR(tr, scl_thread_create(NULL, NULL, NULL), SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_thread_create_named(NULL, "x", NULL, NULL),
                   SCL_ERR_NULL_PTR);
  TEST_TRACE_END();
}

/* ── scl_thread_self / equal ────────────────────────────────── */
static void test_thread_self_equal(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_thread_self / equal");

  scl_thread_t self = scl_thread_self();
  SCL_EXPECT_TRUE(tr, scl_thread_equal(self, scl_thread_self()));
  TEST_TRACE_END();
}

/* ── scl_rwlock ─────────────────────────────────────────────── */
static void test_rwlock(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_rwlock rdlock/wrlock");

  scl_rwlock_t rw;
  SCL_EXPECT_OK(tr, scl_rwlock_init(&rw));

  SCL_EXPECT_OK(tr, scl_rwlock_rdlock(&rw));
  SCL_EXPECT_OK(tr, scl_rwlock_rdlock(&rw)); /* recursive read OK */
  SCL_EXPECT_OK(tr, scl_rwlock_unlock(&rw));
  SCL_EXPECT_OK(tr, scl_rwlock_unlock(&rw));

  SCL_EXPECT_OK(tr, scl_rwlock_wrlock(&rw));
  SCL_EXPECT_OK(tr, scl_rwlock_unlock(&rw));

  scl_rwlock_destroy(&rw);
  TEST_TRACE_END();
}

/* ── scl_once ────────────────────────────────────────────────── */
static int once_flag = 0;
static void once_fn(void) { once_flag++; }

static void test_once(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_once");

  once_flag = 0;
  scl_once_t once = SCL_ONCE_INIT;
  SCL_EXPECT_OK(tr, scl_once(&once, once_fn));
  SCL_EXPECT_OK(tr, scl_once(&once, once_fn)); /* second call should be no-op */
  SCL_EXPECT_EQ_I(tr, once_flag, 1);
  TEST_TRACE_END();
}

/* ── scl_barrier ────────────────────────────────────────────── */
static void *barrier_worker(void *arg) {
  scl_barrier_t *b = (scl_barrier_t *)arg;
  scl_barrier_wait(b);
  return NULL;
}

static void test_barrier(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_barrier");

  scl_barrier_t b;
  SCL_EXPECT_OK(tr, scl_barrier_init(&b, 3));

  scl_thread_t t1, t2;
  scl_thread_create(&t1, barrier_worker, &b);
  scl_thread_create(&t2, barrier_worker, &b);

  scl_barrier_wait(&b); /* main thread is the 3rd */

  scl_thread_join(t1, NULL);
  scl_thread_join(t2, NULL);

  scl_barrier_destroy(&b);
  TEST_TRACE_END();
}

/* ── SCL_LOCK macro ─────────────────────────────────────────── */
static void test_scoped_lock_macro(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("SCL_LOCK macro");

  scl_mutex_t m;
  scl_mutex_init(&m);
  int counter = 0;

  SCL_LOCK(&m) { counter++; }

  SCL_EXPECT_EQ_I(tr, counter, 1);
  scl_mutex_destroy(&m);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_null_errors(&tr);
  test_mutex_create_join(&tr);
  test_recursive_mutex(&tr);
  test_trylock(&tr);
  test_condvar(&tr);
  test_cond_timedwait_timeout(&tr);
  test_thread_self_equal(&tr);
  test_rwlock(&tr);
  test_once(&tr);
  test_barrier(&tr);
  test_scoped_lock_macro(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
