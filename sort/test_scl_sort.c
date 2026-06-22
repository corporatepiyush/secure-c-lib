#include "scl_sort.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    if (x < y) return -1;
    if (x > y) return 1;
    return 0;
}

static int sort_check(const int *arr, size_t n)
{
    for (size_t i = 1; i < n; i++)
        if (arr[i - 1] > arr[i]) return 0;
    return 1;
}

static void test_sorts(void)
{
    size_t n = 100;
    int orig[100], arr[100];
    for (size_t i = 0; i < n; i++) orig[i] = (int)(n - 1 - i);

    TEST("quick sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_quick(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("merge sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_merge(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("heap sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_heap(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("insertion sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_insertion(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("selection sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_selection(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("bubble sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_bubble(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("shell sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_shell(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("counting sort");
    int32_t cdata[] = {5, 3, 8, 1, 9, 2, 7, 4, 6, 0};
    assert(scl_sort_counting(cdata, 10) == SCL_OK);
    for (int32_t i = 0; i < 10; i++) assert(cdata[i] == i);
    PASS();

    TEST("radix sort");
    int32_t rdata[] = {170, 45, 75, 90, 2, 24, 802, 66};
    assert(scl_sort_radix(rdata, 8) == SCL_OK);
    assert(rdata[0] == 2 && rdata[7] == 802);
    PASS();

    TEST("bucket sort");
    memcpy(arr, orig, n * sizeof(int));
    assert(scl_sort_bucket(arr, n, sizeof(int), cmp_int) == SCL_OK);
    assert(sort_check(arr, n));
    PASS();

    TEST("sort null checks");
    assert(scl_sort_quick(NULL, 10, sizeof(int), cmp_int) == SCL_ERR_NULL_PTR);
    assert(scl_sort_quick(arr, 10, sizeof(int), NULL) == SCL_ERR_NULL_PTR);
    PASS();
}

int main(void)
{
    printf("=== scl_sort tests ===\n");
    test_sorts();
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
