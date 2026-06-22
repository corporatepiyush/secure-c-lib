#include "scl_queue.h"
#include "../testlib/scl_test.h"
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static void test_basic(scl_test_runner_t *tr)
{
    scl_test_group("basic FIFO");
    scl_allocator_t *a = scl_allocator_default();
    scl_queue_t q;
    scl_queue_init(a, &q, sizeof(int), 0);
    for (int i = 0; i < 100; i++) SCL_EXPECT_OK(tr, scl_queue_enqueue(a, &q, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_queue_count(&q), 100);
    for (int i = 0; i < 100; i++) {
        int v; scl_queue_dequeue(&q, &v); SCL_EXPECT_EQ_I(tr, v, i);
    }
    SCL_EXPECT_TRUE(tr, scl_queue_empty(&q));
    scl_queue_destroy(a, &q);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_queue_t q;
    scl_queue_init(a, &q, sizeof(int), 0);
    for (int i = 0; i < 10; i++) scl_queue_enqueue(a, &q, &i);
    size_t idx;
    int key = 5;
    SCL_EXPECT_OK(tr, scl_queue_search(&q, &key, cmp_int, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_queue_search(&q, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_queue_destroy(a, &q);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_queue tests ===");
    test_basic(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
