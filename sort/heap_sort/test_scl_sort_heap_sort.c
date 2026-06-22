#include "../../testlib/scl_test.h"
#include "scl_sort_heap_sort.h"
#include <string.h>

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

static int is_sorted(const int *arr, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (arr[i - 1] > arr[i]) return 0;
    return 1;
}

int main(void)
{
    scl_test_runner_t tr;
    scl_test_init(&tr);
    scl_test_group("heap_sort");

    int orig[100], arr[100];
    for (size_t i = 0; i < 100; i++) orig[i] = (int)(99 - i);
    memcpy(arr, orig, sizeof(orig));
    SCL_EXPECT_OK(&tr, scl_sort_heap_sort(arr, 100, sizeof(int), cmp_int));
    SCL_EXPECT_TRUE(&tr, is_sorted(arr, 100));

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
