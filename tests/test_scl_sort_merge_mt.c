/* Tests for the multithreaded mergesort: correctness across sizes and thread
 * counts, stability, and edge cases. Run under TSan to confirm the shared
 * scratch buffer is race-free, and ASan for memory safety. */
#include "scl_test.h"
#include "scl_merge.h"
#include <stdlib.h>
#include <string.h>

static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);
}

/* Deterministic xorshift so runs are reproducible. */
static uint32_t rng_state = 0x12345678u;
static uint32_t rng(void) {
    rng_state ^= rng_state << 13; rng_state ^= rng_state >> 17; rng_state ^= rng_state << 5;
    return rng_state;
}

static int is_sorted_int(const int *a, size_t n) {
    for (size_t i = 1; i < n; i++) if (a[i - 1] > a[i]) return 0;
    return 1;
}

static void test_sizes_threads(scl_test_runner_t *tr) {
    scl_test_group("merge_mt: correct across sizes and thread counts");
    scl_allocator_t *a = scl_allocator_default();
    size_t sizes[] = { 0, 1, 2, 3, 100, 2047, 2048, 4096, 100000 };
    unsigned int threads[] = { 0, 1, 2, 4, 8 };

    for (size_t si = 0; si < SCL_ARRAY_SIZE(sizes); si++) {
        size_t n = sizes[si];
        int *arr = (int *)malloc((n ? n : 1) * sizeof(int));
        int *ref = (int *)malloc((n ? n : 1) * sizeof(int));
        if (!arr || !ref) { SCL_EXPECT_TRUE(tr, 0); free(arr); free(ref); return; }
        for (size_t i = 0; i < n; i++) { arr[i] = (int)(rng() % 1000); ref[i] = arr[i]; }

        for (size_t ti = 0; ti < SCL_ARRAY_SIZE(threads); ti++) {
            memcpy(arr, ref, n * sizeof(int));
            SCL_EXPECT_OK(tr, scl_sort_merge_sort_mt(a, arr, n, sizeof(int), cmp_int, threads[ti]));
            SCL_EXPECT_TRUE(tr, is_sorted_int(arr, n));
        }
        /* Same multiset as libc qsort of the reference. */
        if (n) {
            qsort(ref, n, sizeof(int), cmp_int);
            SCL_EXPECT_TRUE(tr, memcmp(arr, ref, n * sizeof(int)) == 0);
        }
        free(arr); free(ref);
    }
}

typedef struct { int key; int idx; } kv_t;
static int cmp_kv(const void *a, const void *b) {
    int x = ((const kv_t *)a)->key, y = ((const kv_t *)b)->key;
    return (x > y) - (x < y);     /* compare by key only */
}

static void test_stability(scl_test_runner_t *tr) {
    scl_test_group("merge_mt: stable (equal keys keep input order)");
    scl_allocator_t *a = scl_allocator_default();
    size_t n = 50000;
    kv_t *arr = (kv_t *)malloc(n * sizeof(kv_t));
    if (!arr) { SCL_EXPECT_TRUE(tr, 0); return; }
    for (size_t i = 0; i < n; i++) { arr[i].key = (int)(rng() % 50); arr[i].idx = (int)i; }

    SCL_EXPECT_OK(tr, scl_sort_merge_sort_mt(a, arr, n, sizeof(kv_t), cmp_kv, 8));

    int stable = 1, ordered = 1;
    for (size_t i = 1; i < n; i++) {
        if (arr[i - 1].key > arr[i].key) ordered = 0;
        if (arr[i - 1].key == arr[i].key && arr[i - 1].idx > arr[i].idx) stable = 0;
    }
    SCL_EXPECT_TRUE(tr, ordered);
    SCL_EXPECT_TRUE(tr, stable);
    free(arr);
}

static void test_edges(scl_test_runner_t *tr) {
    scl_test_group("merge_mt: edge cases");
    scl_allocator_t *a = scl_allocator_default();
    int one[1] = { 42 };
    SCL_EXPECT_OK(tr, scl_sort_merge_sort_mt(a, one, 1, sizeof(int), cmp_int, 4));
    SCL_EXPECT_EQ_I(tr, one[0], 42);
    SCL_EXPECT_TRUE(tr, scl_sort_merge_sort_mt(a, NULL, 10, sizeof(int), cmp_int, 4) == SCL_ERR_NULL_PTR);
    SCL_EXPECT_TRUE(tr, scl_sort_merge_sort_mt(a, one, 1, 0, cmp_int, 4) == SCL_ERR_INVALID_ARG);

    /* Already-sorted and reverse-sorted inputs. */
    size_t n = 5000;
    int *arr = (int *)malloc(n * sizeof(int));
    if (!arr) { SCL_EXPECT_TRUE(tr, 0); return; }
    for (size_t i = 0; i < n; i++) arr[i] = (int)i;
    SCL_EXPECT_OK(tr, scl_sort_merge_sort_mt(a, arr, n, sizeof(int), cmp_int, 4));
    SCL_EXPECT_TRUE(tr, is_sorted_int(arr, n));
    for (size_t i = 0; i < n; i++) arr[i] = (int)(n - i);
    SCL_EXPECT_OK(tr, scl_sort_merge_sort_mt(a, arr, n, sizeof(int), cmp_int, 4));
    SCL_EXPECT_TRUE(tr, is_sorted_int(arr, n));
    free(arr);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_sizes_threads(&tr);
    test_stability(&tr);
    test_edges(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
