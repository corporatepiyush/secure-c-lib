#include "../../testlib/scl_test.h"
#include "scl_search_sentinel_linear_search.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();

    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        scl_test_group("sentinel_linear_search");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_sentinel_linear_search(a, arr, 6, sizeof(int), &(int){8}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 2, idx);
    }
    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_sentinel_linear_search(a, arr, 6, sizeof(int), &(int){5}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_sentinel_linear_search(a, arr, 6, sizeof(int), &(int){3}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 5, idx);
    }
    {
        int arr[] = {5, 2, 8, 1, 9, 3};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_sentinel_linear_search(a, arr, 6, sizeof(int), &(int){99}, cmp_int, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_sentinel_linear_search(a, NULL, 0, sizeof(int), &(int){1}, cmp_int, &idx));
    }
    {
        int arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_sentinel_linear_search(a, arr, 1, sizeof(int), &(int){42}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_sentinel_linear_search(a, arr, 1, sizeof(int), &(int){1}, cmp_int, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_sentinel_linear_search(a, (void*)(uintptr_t)1, 0, sizeof(int), &(int){1}, cmp_int, &idx));
    }
    {
        int arr[] = {1, 2, 2, 2, 3};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_sentinel_linear_search(a, arr, 5, sizeof(int), &(int){2}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 1, idx);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
