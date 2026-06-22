#include "../testlib/scl_test.h"
#include "scl_threadpool.h"

static int g_counter = 0;
static void inc_counter(void *arg) {
    (void)arg;
    __sync_fetch_and_add(&g_counter, 1);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_threadpool_init(alloc, NULL, 4) == SCL_ERR_NULL_PTR);
        SCL_EXPECT_TRUE(&tr, scl_threadpool_enqueue(NULL, inc_counter, NULL) == SCL_ERR_NULL_PTR);
        SCL_EXPECT_TRUE(&tr, scl_threadpool_wait(NULL) == SCL_ERR_NULL_PTR);
        SCL_EXPECT_TRUE(&tr, scl_threadpool_destroy(NULL) == SCL_ERR_NULL_PTR);
    }

    scl_test_group("init with zero threads");
    {
        scl_threadpool_t pool;
        SCL_EXPECT_TRUE(&tr, scl_threadpool_init(alloc, &pool, 0) == SCL_ERR_INVALID_ARG);
    }

    scl_test_group("basic enqueue and wait");
    {
        scl_threadpool_t pool;
        scl_error_t e = scl_threadpool_init(alloc, &pool, 4);
        SCL_EXPECT_TRUE(&tr, e == SCL_OK);

        g_counter = 0;
        for (int i = 0; i < 100; i++)
            scl_threadpool_enqueue(&pool, inc_counter, NULL);

        scl_threadpool_wait(&pool);
        SCL_EXPECT_EQ_I(&tr, g_counter, 100);

        scl_threadpool_destroy(&pool);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
