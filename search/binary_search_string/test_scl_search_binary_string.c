#include "../../testlib/scl_test.h"
#include "scl_search_binary_string.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        const char *strs[] = {"apple", "banana", "cherry", "date", "elderberry"};
        size_t idx;
        scl_test_group("binary_string");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_string(strs, 5, "cherry", &idx));
        SCL_EXPECT_EQ_SZ(&tr, 2, idx);
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date", "elderberry"};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_string(strs, 5, "apple", &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date", "elderberry"};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_string(strs, 5, "elderberry", &idx));
        SCL_EXPECT_EQ_SZ(&tr, 4, idx);
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date"};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_binary_string(strs, 4, "apricot", &idx));
    }
    {
        const char *strs[] = {"apple", "banana", "cherry", "date"};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_binary_string(strs, 4, "zebra", &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_binary_string(NULL, 0, "x", &idx));
    }
    {
        const char *strs[] = {"only"};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_binary_string(strs, 1, "only", &idx));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        const char *strs[] = {"only"};
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_binary_string(strs, 1, "other", &idx));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_binary_string((void*)(uintptr_t)1, 0, "x", &idx));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
