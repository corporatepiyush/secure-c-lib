#include "../../testlib/scl_test.h"
#include "scl_search_quickselect.h"

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
        int arr[] = {9, 5, 7, 1, 3, 8, 2, 6, 4};
        int out;
        scl_test_group("quickselect");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_quickselect(a, arr, 9, sizeof(int), cmp_int, 0, &out));
        SCL_EXPECT_EQ_I(&tr, 1, out);
    }
    {
        int arr[] = {9, 5, 7, 1, 3, 8, 2, 6, 4};
        int out;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_quickselect(a, arr, 9, sizeof(int), cmp_int, 4, &out));
        SCL_EXPECT_EQ_I(&tr, 5, out);
    }
    {
        int arr[] = {9, 5, 7, 1, 3, 8, 2, 6, 4};
        int out;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_quickselect(a, arr, 9, sizeof(int), cmp_int, 8, &out));
        SCL_EXPECT_EQ_I(&tr, 9, out);
    }
    {
        int arr[] = {42};
        int out;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_quickselect(a, arr, 1, sizeof(int), cmp_int, 0, &out));
        SCL_EXPECT_EQ_I(&tr, 42, out);
    }
    {
        int arr[] = {5, 3, 3, 1, 3};
        int out;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_quickselect(a, arr, 5, sizeof(int), cmp_int, 2, &out));
        SCL_EXPECT_EQ_I(&tr, 3, out);
    }
    {
        int arr[] = {10, 20, 30};
        int out;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_quickselect(a, arr, 3, sizeof(int), cmp_int, 1, &out));
        SCL_EXPECT_EQ_I(&tr, 20, out);
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_quickselect(a, NULL, 1, sizeof(int), cmp_int, 0, &idx));
    }
    {
        int arr[] = {1, 2, 3};
        int out;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_INVALID_INDEX, scl_search_quickselect(a, arr, 3, sizeof(int), cmp_int, 5, &out));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
