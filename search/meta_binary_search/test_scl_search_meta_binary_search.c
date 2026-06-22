#include "../../testlib/scl_test.h"
#include "scl_search_meta_binary_search.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        int32_t arr[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t idx;
        scl_test_group("meta_binary_search");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_meta_binary_search(arr, 8, 7, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 3, idx);
    }
    {
        int32_t arr[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_meta_binary_search(arr, 8, 1, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int32_t arr[] = {1, 3, 5, 7, 9, 11, 13, 15};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_meta_binary_search(arr, 8, 15, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 7, idx);
    }
    {
        int32_t arr[] = {1, 3, 5, 7, 9};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_meta_binary_search(arr, 5, 4, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_meta_binary_search(NULL, 0, 1, &idx));
    }
    {
        int32_t arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_meta_binary_search(arr, 1, 42, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        int32_t arr[] = {42};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_meta_binary_search(arr, 1, 1, &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_meta_binary_search((void*)(uintptr_t)1, 0, 5, &idx));
    }
    {
        int32_t arr[] = {2, 4, 6, 8, 10, 12, 14, 16};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_meta_binary_search(arr, 8, 10, &idx));
        SCL_EXPECT_EQ_SZ(&tr, 4, idx);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
