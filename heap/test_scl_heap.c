#include "scl_heap.h"
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

static void test_min_heap(scl_test_runner_t *tr)
{
    scl_test_group("min-heap");
    scl_allocator_t *a = scl_allocator_default();
    scl_heap_t h;
    scl_heap_init(a, &h, sizeof(int), 0, cmp_int);
    int vals[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    for (size_t i = 0; i < 10; i++) scl_heap_push(a, &h, &vals[i]);
    SCL_EXPECT_EQ_SZ(tr, scl_heap_count(&h), 10);
    for (int i = 0; i < 10; i++) {
        int v; scl_heap_pop(&h, &v); SCL_EXPECT_EQ_I(tr, v, i);
    }
    scl_heap_destroy(a, &h);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_test_group("search");
    scl_allocator_t *a = scl_allocator_default();
    scl_heap_t h;
    scl_heap_init(a, &h, sizeof(int), 0, cmp_int);
    int vals[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    for (size_t i = 0; i < 10; i++) scl_heap_push(a, &h, &vals[i]);
    size_t idx;
    int key = 5;
    SCL_EXPECT_OK(tr, scl_heap_search(&h, &key, cmp_int, &idx));
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_heap_search(&h, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_heap_destroy(a, &h);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_heap tests ===");
    test_min_heap(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
