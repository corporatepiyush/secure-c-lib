#include "../../testlib/scl_test.h"
#include "scl_search_binary_search.h"

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        scl_test_group("binary_search");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_search(arr, 7, sizeof(int), &(int){7}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 3, idx);
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_search(arr, 7, sizeof(int), &(int){1}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_search(arr, 7, sizeof(int), &(int){13}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 6, idx);
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_binary_search(arr, 7, sizeof(int), &(int){0}, cmp_int, &idx));
    }
    {
        int arr[] = {1, 3, 5, 7, 9, 11, 13};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_binary_search(arr, 7, sizeof(int), &(int){99}, cmp_int, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_binary_search(NULL, 0, sizeof(int), &(int){1}, cmp_int, &idx));
    }
    {
        int arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_search(arr, 1, sizeof(int), &(int){42}, cmp_int, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_binary_search(arr, 1, sizeof(int), &(int){1}, cmp_int, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_binary_search((void*)(uintptr_t)1, 0, sizeof(int), &(int){1}, cmp_int, &idx));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
