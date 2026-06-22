#include "scl_array.h"
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

static void test_init_destroy(scl_test_runner_t *tr)
{
    scl_test_group("init and destroy");
    scl_array_t arr;
    scl_allocator_t *a = scl_allocator_default();
    SCL_EXPECT_OK(tr, scl_array_init(a, &arr, sizeof(int), 0));
    SCL_EXPECT_TRUE(tr, scl_array_empty(&arr));
    SCL_EXPECT_EQ_SZ(tr, scl_array_count(&arr), 0);
    scl_array_destroy(a, &arr);
}

static void test_push_pop(scl_test_runner_t *tr)
{
    scl_test_group("push and pop");
    scl_array_t arr;
    scl_allocator_t *a = scl_allocator_default();
    scl_array_init(a, &arr, sizeof(int), 0);
    for (int i = 0; i < 100; i++)
        SCL_EXPECT_OK(tr, scl_array_push(a, &arr, &i));
    SCL_EXPECT_EQ_SZ(tr, scl_array_count(&arr), 100);
    for (int i = 99; i >= 0; i--) {
        int val;
        SCL_EXPECT_OK(tr, scl_array_pop(&arr, &val));
        SCL_EXPECT_EQ_I(tr, val, i);
    }
    SCL_EXPECT_TRUE(tr, scl_array_empty(&arr));
    scl_array_destroy(a, &arr);
}

static void test_get_set(scl_test_runner_t *tr)
{
    scl_test_group("get and set");
    scl_array_t arr;
    scl_allocator_t *a = scl_allocator_default();
    scl_array_init(a, &arr, sizeof(int), 10);
    int x = 42; scl_array_push(a, &arr, &x);
    int val; SCL_EXPECT_OK(tr, scl_array_get(&arr, 0, &val)); SCL_EXPECT_EQ_I(tr, val, 42);
    x = 99; SCL_EXPECT_OK(tr, scl_array_set(&arr, 0, &x));
    SCL_EXPECT_OK(tr, scl_array_get(&arr, 0, &val)); SCL_EXPECT_EQ_I(tr, val, 99);
    SCL_EXPECT_ERROR(tr, scl_array_get(&arr, 1, &val), SCL_ERR_INVALID_INDEX);
    scl_array_destroy(a, &arr);
}

static void test_insert_remove(scl_test_runner_t *tr)
{
    scl_test_group("insert and remove");
    scl_array_t arr;
    scl_allocator_t *a = scl_allocator_default();
    scl_array_init(a, &arr, sizeof(int), 4);
    for (int i = 0; i < 3; i++) { int v = i * 10; scl_array_push(a, &arr, &v); }
    int v = 99; SCL_EXPECT_OK(tr, scl_array_insert(a, &arr, 1, &v));
    SCL_EXPECT_OK(tr, scl_array_get(&arr, 1, &v)); SCL_EXPECT_EQ_I(tr, v, 99);
    SCL_EXPECT_OK(tr, scl_array_remove(&arr, 1, &v)); SCL_EXPECT_EQ_I(tr, v, 99);
    SCL_EXPECT_EQ_SZ(tr, scl_array_count(&arr), 3);
    scl_array_destroy(a, &arr);
}

static void test_null(scl_test_runner_t *tr)
{
    scl_test_group("null checks");
    scl_allocator_t *a = scl_allocator_default();
    SCL_EXPECT_ERROR(tr, scl_array_init(a, NULL, sizeof(int), 0), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_array_push(a, NULL, &(int){0}), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_array_pop(NULL, &(int){0}), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_array_get(NULL, 0, &(int){0}), SCL_ERR_NULL_PTR);
    scl_array_destroy(a, NULL);
}

static void test_edge(scl_test_runner_t *tr)
{
    scl_allocator_t *a = scl_allocator_default();
    scl_test_group("empty ops");
    scl_array_t arr;
    scl_array_init(a, &arr, sizeof(int), 0);
    SCL_EXPECT_ERROR(tr, scl_array_pop(&arr, &(int){0}), SCL_ERR_EMPTY);
    SCL_EXPECT_ERROR(tr, scl_array_get(&arr, 0, &(int){0}), SCL_ERR_INVALID_INDEX);
    scl_array_destroy(a, &arr);

    scl_test_group("zero element size rejection");
    SCL_EXPECT_ERROR(tr, scl_array_init(a, &arr, 0, 10), SCL_ERR_INVALID_ARG);

    scl_test_group("large data integrity");
    scl_array_init(a, &arr, sizeof(int), 0);
    for (int i = 0; i < 1000; i++) scl_array_push(a, &arr, &i);
    for (int i = 0; i < 1000; i++) {
        int v; scl_array_get(&arr, i, &v); SCL_EXPECT_EQ_I(tr, v, i);
    }
    scl_array_destroy(a, &arr);
}

static void test_search(scl_test_runner_t *tr)
{
    scl_allocator_t *a = scl_allocator_default();
    scl_test_group("linear search");
    scl_array_t arr;
    scl_array_init(a, &arr, sizeof(int), 0);
    for (int i = 0; i < 10; i++) { int v = i * 10; scl_array_push(a, &arr, &v); }
    size_t idx;
    int key = 50;
    SCL_EXPECT_OK(tr, scl_array_linear_search(&arr, &key, cmp_int, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_array_linear_search(&arr, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_array_destroy(a, &arr);

    scl_test_group("binary search");
    scl_array_init(a, &arr, sizeof(int), 0);
    for (int i = 0; i < 10; i++) { int v = i * 10; scl_array_push(a, &arr, &v); }
    key = 50;
    SCL_EXPECT_OK(tr, scl_array_binary_search(&arr, &key, cmp_int, &idx));
    SCL_EXPECT_EQ_SZ(tr, idx, 5);
    key = 999;
    SCL_EXPECT_ERROR(tr, scl_array_binary_search(&arr, &key, cmp_int, &idx), SCL_ERR_NOT_FOUND);
    scl_array_destroy(a, &arr);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("=== scl_array tests ===");
    test_init_destroy(&tr);
    test_push_pop(&tr);
    test_get_set(&tr);
    test_insert_remove(&tr);
    test_null(&tr);
    test_edge(&tr);
    test_search(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
