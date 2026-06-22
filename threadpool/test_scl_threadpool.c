#include "scl_threadpool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

typedef struct {
    int *counter;
    int id;
    atomic_int *done;
} task_arg_t;

static void increment_task(void *arg) {
    task_arg_t *ta = (task_arg_t *)arg;
    if (ta->counter) atomic_fetch_add((atomic_int *)ta->counter, 1);
    if (ta->done) atomic_fetch_add(ta->done, 1);
}

int main(void) {
    printf("=== scl_threadpool tests ===\n");

    TEST("init and destroy");
    {
        scl_threadpool_t pool;
        scl_error_t e = scl_threadpool_init(&pool, 4);
        if (e == SCL_OK) {
            scl_threadpool_destroy(&pool);
            PASS();
        } else { FAIL("init failed"); }
    }

    TEST("submit N tasks, wait, verify counter");
    {
        scl_threadpool_t pool;
        scl_threadpool_init(&pool, 4);
        atomic_int counter = 0;
        atomic_int done = 0;
        task_arg_t args[1000];
        int ok = 1;

        for (int i = 0; i < 1000; i++) {
            args[i].counter = (int *)&counter;
            args[i].id = i;
            args[i].done = &done;
            if (scl_threadpool_submit(&pool, increment_task, &args[i]) != SCL_OK) {
                ok = 0; break;
            }
        }

        scl_threadpool_wait(&pool);
        if (ok && atomic_load(&counter) == 1000) { PASS(); }
        else { FAIL("counter mismatch"); }
        scl_threadpool_destroy(&pool);
    }

    TEST("NULL pool returns ERR_NULL_PTR");
    {
        if (scl_threadpool_init(NULL, 4) == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    TEST("submit after destroy returns error");
    {
        scl_threadpool_t pool;
        scl_threadpool_init(&pool, 2);
        scl_threadpool_destroy(&pool);
        if (scl_threadpool_submit(&pool, increment_task, NULL) == SCL_ERR_NULL_PTR ||
            scl_threadpool_submit(&pool, increment_task, NULL) != SCL_OK) { PASS(); }
        else { FAIL("should have errored"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
