#include "scl_test.h"
#include "scl_pthread.h"

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

static void test_thread_create_join(scl_test_runner_t *tr) {
    scl_test_group("scl_pthread create / join");

    scl_mutex_t lock;
    SCL_EXPECT_OK(tr, scl_mutex_init(&lock, NULL));

    int counter = 0;
    worker_arg_t wa = { &lock, &counter };

    scl_pthread_t t1, t2;
    SCL_EXPECT_OK(tr, scl_pthread_create(&t1, NULL, worker_fn, &wa));
    SCL_EXPECT_OK(tr, scl_pthread_create(&t2, NULL, worker_fn, &wa));

    SCL_EXPECT_OK(tr, scl_pthread_join(t1, NULL));
    SCL_EXPECT_OK(tr, scl_pthread_join(t2, NULL));

    SCL_EXPECT_EQ_I(tr, counter, 200);

    scl_mutex_destroy(&lock);
}

/* ── Helper for condvar test ────────────────────────────────── */
typedef struct {
    scl_mutex_t *lock;
    scl_cond_t  *cond;
    int         *flag;
} cv_arg_t;

static void *waiter_fn(void *arg) {
    cv_arg_t *ca = (cv_arg_t *)arg;
    scl_mutex_lock(ca->lock);
    while (*(ca->flag) == 0) {
        scl_cond_wait(ca->cond, ca->lock);
    }
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
    scl_test_group("scl_cond wait / signal");

    scl_mutex_t lock;
    scl_cond_t  cond;
    SCL_EXPECT_OK(tr, scl_mutex_init(&lock, NULL));
    SCL_EXPECT_OK(tr, scl_cond_init(&cond, NULL));

    int flag = 0;
    cv_arg_t ca = { &lock, &cond, &flag };

    scl_pthread_t waiter, signaler;
    SCL_EXPECT_OK(tr, scl_pthread_create(&waiter, NULL, waiter_fn, &ca));
    SCL_EXPECT_OK(tr, scl_pthread_create(&signaler, NULL, signaler_fn, &ca));

    SCL_EXPECT_OK(tr, scl_pthread_join(waiter, NULL));
    SCL_EXPECT_OK(tr, scl_pthread_join(signaler, NULL));

    SCL_EXPECT_EQ_I(tr, flag, 1);

    scl_cond_destroy(&cond);
    scl_mutex_destroy(&lock);
}

static void test_mutex_error_null(scl_test_runner_t *tr) {
    scl_test_group("scl_mutex NULL errors");
    SCL_EXPECT_ERROR(tr, scl_mutex_lock(NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_mutex_unlock(NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_mutex_init(NULL, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_mutex_destroy(NULL), SCL_ERR_NULL_PTR);
}

static void test_condvar_error_null(scl_test_runner_t *tr) {
    scl_test_group("scl_cond NULL errors");
    SCL_EXPECT_ERROR(tr, scl_cond_wait(NULL, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_cond_signal(NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_cond_broadcast(NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_cond_init(NULL, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_cond_destroy(NULL), SCL_ERR_NULL_PTR);
}

static void test_thread_error_null(scl_test_runner_t *tr) {
    scl_test_group("scl_pthread NULL errors");
    SCL_EXPECT_ERROR(tr, scl_pthread_create(NULL, NULL, NULL, NULL), SCL_ERR_NULL_PTR);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_mutex_error_null(&tr);
    test_condvar_error_null(&tr);
    test_thread_error_null(&tr);
    test_thread_create_join(&tr);
    test_condvar(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
