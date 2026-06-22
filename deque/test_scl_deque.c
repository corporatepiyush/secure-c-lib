#include "scl_deque.h"
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

static void test_push_pop(scl_test_runner_t *tr)
{
    scl_test_group("push/pop both ends");
    scl_allocator_t *a = scl_allocator_default();
    scl_deque_t d;
    scl_deque_init(a, &d, sizeof(int), 0);
    for (int i = 1; i <= 5; i++) {
        scl_deque_push_back(a, &d, &i);
        scl_deque_push_front(a, &d, &i);
    }
    SCL_EXPECT_EQ_SZ(tr, scl_deque_count(&d), 10);
    for (int i = 5; i >= 1; i--) {
        int v; scl_deque_pop_front(&d, &v); SCL_EXPECT_EQ_I(tr, v, i);
        scl_deque_pop_back(&d, &v); SCL_EXPECT_EQ_I(tr, v, i);
    }
    scl_deque_destroy(a, &d);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_deque_t d;
    scl_deque_init(a, &d, sizeof(int), 0);
    for (int i = 0; i < 10; i++) scl_deque_push_back(a, &d, &i);
    size_t idx;
    int key = 5;
    SCL_EXPECT_OK(tr, scl_deque_search(&d, &key, cmp_int, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_deque_search(&d, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_deque_destroy(a, &d);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_deque tests ===");
    test_push_pop(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
