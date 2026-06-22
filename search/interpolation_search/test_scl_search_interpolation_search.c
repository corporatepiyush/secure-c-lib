#include "../../testlib/scl_test.h"
#include "scl_search_interpolation_search.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        int64_t arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        size_t idx;
        scl_test_group("interpolation_search");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_interpolation_search(arr, 10, 50, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 4, idx);
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_interpolation_search(arr, 10, 10, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_interpolation_search(arr, 10, 100, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 9, idx);
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_interpolation_search(arr, 5, 5, &idx));
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_interpolation_search(arr, 5, 99, &idx));
    }
    {
        int64_t arr[] = {10, 20, 30, 40, 50};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_interpolation_search(arr, 5, 25, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_interpolation_search(NULL, 0, 5, &idx));
    }
    {
        int64_t arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_interpolation_search(arr, 1, 42, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int64_t arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_interpolation_search(arr, 1, 1, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_interpolation_search((void*)(uintptr_t)1, 0, 5, &idx));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
