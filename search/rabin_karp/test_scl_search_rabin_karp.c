#include "../../testlib/scl_test.h"
#include "scl_search_rabin_karp.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        size_t pos;
        scl_test_group("rabin_karp");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_rabin_karp("hello world", 11, "world", 5, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 6, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_rabin_karp("hello world", 11, "hello", 5, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 0, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_rabin_karp("hello world", 11, "xyz", 3, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_rabin_karp("abc", 3, "b", 1, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 1, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_rabin_karp("ab", 2, "abc", 3, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_rabin_karp(NULL, 0, "a", 1, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_rabin_karp("aaaaa", 5, "aaa", 3, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 0, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_rabin_karp("", 0, "a", 1, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_rabin_karp("abcde", 5, "cd", 2, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 2, pos);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
