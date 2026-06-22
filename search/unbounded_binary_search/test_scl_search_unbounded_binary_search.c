#include "../../testlib/scl_test.h"
#include "scl_search_unbounded_binary_search.h"

static int arr[] = {2, 4, 6, 8, 10, 12, 14, 16, 18, 20};
static size_t arr_len = 10;

static void *getter(size_t i, void *ctx SCL_UNUSED)
{
    if (i >= arr_len) return NULL;
    return &arr[i];
}

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
        size_t idx;
        scl_test_group("unbounded_binary_search");
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_unbounded_binary_search(cmp_int, &(int){8}, &idx, getter, NULL, arr_len));
        SCL_EXPECT_EQ_SZ(&tr, 3, idx);
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_unbounded_binary_search(cmp_int, &(int){2}, &idx, getter, NULL, arr_len));
        SCL_EXPECT_EQ_SZ(&tr, 0, idx);
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_OK, scl_search_unbounded_binary_search(cmp_int, &(int){20}, &idx, getter, NULL, arr_len));
        SCL_EXPECT_EQ_SZ(&tr, 9, idx);
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NOT_FOUND, scl_search_unbounded_binary_search(cmp_int, &(int){5}, &idx, getter, NULL, arr_len));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_search_unbounded_binary_search(NULL, &(int){1}, &idx, getter, NULL, 5));
    }
    {
        size_t idx;
        SCL_EXPECT_EQ_I(&tr, SCL_ERR_EMPTY, scl_search_unbounded_binary_search(cmp_int, &(int){1}, &idx, getter, NULL, 0));
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
