#include "../testlib/scl_test.h"
#include "scl_sort.h"
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int sort_check(const int *arr, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (arr[i - 1] > arr[i]) return 0;
    return 1;
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_allocator_t *a = scl_allocator_default();

    size_t n = 100;
    int orig[100], arr[100];
    for (size_t i = 0; i < n; i++) orig[i] = (int)(n - 1 - i);

    scl_test_group("quick_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_quick(arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("merge_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_merge(a, arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("heap_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_heap(arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("insertion_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_insertion(arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("selection_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_selection(arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("bubble_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_bubble(arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("shell_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_shell(arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("counting_sort");
    int32_t cdata[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    SCL_EXPECT_OK(&tr, scl_sort_counting(a, cdata, 10));
    for (int32_t i = 0; i < 10; i++)
        SCL_EXPECT_EQ_I(&tr, i, cdata[i]);

    scl_test_group("radix_sort");
    int32_t rdata[] = {170, 45, 75, 90, 2, 24, 802, 66};
    SCL_EXPECT_OK(&tr, scl_sort_radix(a, rdata, 8));
    SCL_EXPECT_EQ_I(&tr, 2, rdata[0]);
    SCL_EXPECT_EQ_I(&tr, 802, rdata[7]);

    scl_test_group("bucket_sort");
    memcpy(arr, orig, n * sizeof(int));
    SCL_EXPECT_OK(&tr, scl_sort_bucket(a, arr, n, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, sort_check(arr, n));

    scl_test_group("null_checks");
    SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_sort_quick(NULL, 10, sizeof(int), cmp_int));
    SCL_EXPECT_EQ_I(&tr, SCL_ERR_NULL_PTR, scl_sort_quick(arr, 10, sizeof(int), NULL));

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
