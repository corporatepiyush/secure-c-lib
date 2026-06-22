#include "../../testlib/scl_test.h"
#include "scl_search_boyer_moore.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);

    {
        size_t pos;
        scl_test_group("boyer_moore");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_boyer_moore(scl_allocator_default(), "hello world", 11, "world", 5, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 6, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_boyer_moore(scl_allocator_default(), "hello world", 11, "hello", 5, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 0, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_boyer_moore(scl_allocator_default(), "hello world", 11, "xyz", 3, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_boyer_moore(scl_allocator_default(), "abc", 3, "b", 1, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 1, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_boyer_moore(scl_allocator_default(), "ab", 2, "abc", 3, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_boyer_moore(NULL, NULL, 0, "a", 1, &pos));
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_boyer_moore(scl_allocator_default(), "aaaaa", 5, "aaa", 3, &pos));
        SCL_EXPECT_EQ_SZ(&tr, 0, pos);
    }
    {
        size_t pos;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_boyer_moore(scl_allocator_default(), "", 0, "a", 1, &pos));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
