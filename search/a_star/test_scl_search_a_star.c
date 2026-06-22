#include "../../testlib/scl_test.h"
#include "scl_search_a_star.h"

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();

    {
        int row0[] = {0, 0, 0, 0, 0};
        int row1[] = {0, 1, 1, 1, 0};
        int row2[] = {0, 0, 0, 1, 0};
        int row3[] = {0, 1, 0, 0, 0};
        int row4[] = {0, 0, 0, 1, 0};
        int *grid[] = {row0, row1, row2, row3, row4};
        int px[100], py[100];
        size_t plen;
        scl_test_group("a_star");
        SCL_EXPECT_OK(&tr, scl_search_a_star(a, 0, 0, 4, 4, grid, 5, 5, px, py, &plen, 100));
        SCL_EXPECT_TRUE(&tr, plen > 0);
    }
    {
        int row0[] = {0, 0, 0, 0, 0};
        int row1[] = {1, 1, 1, 1, 1};
        int row2[] = {0, 0, 0, 0, 0};
        int *grid[] = {row0, row1, row2};
        int px[100], py[100];
        size_t plen;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_a_star(a, 0, 0, 4, 2, grid, 5, 3, px, py, &plen, 100));
    }
    {
        int row0[] = {0, 0};
        int *grid[] = {row0};
        int px[100], py[100];
        size_t plen;
        SCL_EXPECT_OK(&tr, scl_search_a_star(a, 0, 0, 1, 0, grid, 2, 1, px, py, &plen, 100));
        SCL_EXPECT_TRUE(&tr, plen > 0);
    }
    {
        int row0[] = {0};
        int *grid[] = {row0};
        int px[100], py[100];
        size_t plen;
        SCL_EXPECT_OK(&tr, scl_search_a_star(a, 0, 0, 0, 0, grid, 1, 1, px, py, &plen, 100));
        SCL_EXPECT_EQ_SZ(&tr, 1, plen);
    }
    {
        size_t plen;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_a_star(a, 0, 0, 1, 1, NULL, 2, 2, (int*)(uintptr_t)1, (int*)(uintptr_t)1, &plen, 100));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
