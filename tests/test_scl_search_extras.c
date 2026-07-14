/* Regression tests for the hardened array/string searches:
 *   - interpolation search on adversarial int64 ranges (no signed-overflow UB),
 *   - hash table insert correctness across tombstones (no duplicate keys),
 *   - sentinel linear search (real sentinel placement),
 *   - quickselect correctness incl. sorted/reverse worst-case inputs,
 *   - exponential string search + NULL-element guard.
 * Run under ASan+UBSan. */
#include "scl_exponential_string.h"
#include "scl_hash_search.h"
#include "scl_interpolation.h"
#include "scl_quickselect.h"
#include "scl_sentinel_linear.h"
#include "scl_test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int cmp_int(const void *a, const void *b) {
  int x = *(const int *)a, y = *(const int *)b;
  return (x > y) - (x < y);
}

static void test_interpolation_adversarial(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("interpolation: adversarial int64 range (no overflow UB)");
  /* {INT64_MIN, INT64_MAX} would make key-arr[lo] overflow in the old code. */
  int64_t arr[] = {INT64_MIN, -1000, 0, 1000, INT64_MAX};
  size_t n = SCL_ARRAY_SIZE(arr), idx = 0;

  SCL_EXPECT_OK(tr, scl_search_interpolation_search(arr, n, INT64_MIN, &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 0);
  SCL_EXPECT_OK(tr, scl_search_interpolation_search(arr, n, 0, &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 2);
  SCL_EXPECT_OK(tr, scl_search_interpolation_search(arr, n, INT64_MAX, &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 4);
  SCL_EXPECT_TRUE(tr, scl_search_interpolation_search(arr, n, 42, &idx) ==
                          SCL_ERR_NOT_FOUND);

  /* Uniform large positives. */
  int64_t up[] = {1000000000000LL, 2000000000000LL, 3000000000000LL};
  SCL_EXPECT_OK(tr,
                scl_search_interpolation_search(up, 3, 2000000000000LL, &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 1);
  TEST_TRACE_END();
}

static void test_hash_tombstones(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("hash: insert across tombstones keeps keys unique");
  scl_allocator_t *a = scl_allocator_default();
  scl_search_ht_t *ht = NULL;
  SCL_EXPECT_OK(tr, scl_search_ht_init(a, &ht, 64));

  char key[16];
  for (int i = 0; i < 20; i++) {
    snprintf(key, sizeof(key), "key%d", i);
    SCL_EXPECT_OK(tr, scl_search_ht_insert(ht, key, (void *)(intptr_t)(i + 1)));
  }
  SCL_EXPECT_EQ_SZ(tr, ht->count, 20);

  /* Delete the even keys -> tombstones scattered through probe paths. */
  for (int i = 0; i < 20; i += 2) {
    snprintf(key, sizeof(key), "key%d", i);
    SCL_EXPECT_OK(tr, scl_search_ht_delete(ht, key));
  }
  SCL_EXPECT_EQ_SZ(tr, ht->count, 10);

  /* Re-insert evens (reuse tombstones) and UPDATE odds (live keys whose probe
   * path now crosses tombstones). The old code inserted a duplicate at the
   * first tombstone instead of updating in place, inflating count. */
  for (int i = 0; i < 20; i++) {
    snprintf(key, sizeof(key), "key%d", i);
    SCL_EXPECT_OK(tr,
                  scl_search_ht_insert(ht, key, (void *)(intptr_t)(i + 100)));
  }
  SCL_EXPECT_EQ_SZ(tr, ht->count, 20); /* no duplicates */

  for (int i = 0; i < 20; i++) {
    snprintf(key, sizeof(key), "key%d", i);
    void *v = NULL;
    SCL_EXPECT_OK(tr, scl_search_ht_search(ht, key, &v));
    SCL_EXPECT_EQ_I(tr, (int)(intptr_t)v, i + 100);
  }
  void *v = NULL;
  SCL_EXPECT_TRUE(tr,
                  scl_search_ht_search(ht, "absent", &v) == SCL_ERR_NOT_FOUND);
  scl_search_ht_destroy(ht);
  TEST_TRACE_END();
}

static void test_sentinel(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("sentinel linear: find present / absent");
  scl_allocator_t *a = scl_allocator_default();
  int arr[] = {5, 3, 9, 1, 7, 2};
  size_t n = SCL_ARRAY_SIZE(arr), idx = 0;
  int key = 7;
  SCL_EXPECT_OK(tr, scl_search_sentinel_linear_search(a, arr, n, sizeof(int),
                                                      &key, cmp_int, &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 4);
  key = 5;
  SCL_EXPECT_OK(tr, scl_search_sentinel_linear_search(a, arr, n, sizeof(int),
                                                      &key, cmp_int, &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 0);
  key = 100;
  SCL_EXPECT_TRUE(tr, scl_search_sentinel_linear_search(a, arr, n, sizeof(int),
                                                        &key, cmp_int, &idx) ==
                          SCL_ERR_NOT_FOUND);
  /* Input array must be unchanged (search works on a copy). */
  SCL_EXPECT_EQ_I(tr, arr[0], 5);
  SCL_EXPECT_EQ_I(tr, arr[5], 2);
  TEST_TRACE_END();
}

static void test_quickselect(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("quickselect: kth-smallest incl. worst-case inputs");
  scl_allocator_t *a = scl_allocator_default();
  size_t n = 2001;
  int *arr = (int *)malloc(n * sizeof(int));
  int *ref = (int *)malloc(n * sizeof(int));
  if (!arr || !ref) {
    SCL_EXPECT_TRUE(tr, 0);
    free(arr);
    free(ref);
    return;
  }

  /* Three shapes that make a fixed-pivot quickselect go quadratic. */
  for (int shape = 0; shape < 3; shape++) {
    for (size_t i = 0; i < n; i++) {
      if (shape == 0)
        arr[i] = (int)i; /* sorted */
      else if (shape == 1)
        arr[i] = (int)(n - i); /* reverse */
      else
        arr[i] = (int)((i * 2654435761u) % 1000); /* pseudo-random */
      ref[i] = arr[i];
    }
    qsort(ref, n, sizeof(int), cmp_int);
    size_t ks[] = {0, 1, n / 2, n - 2, n - 1};
    for (size_t ki = 0; ki < SCL_ARRAY_SIZE(ks); ki++) {
      int out = -1;
      SCL_EXPECT_OK(tr, scl_search_quickselect(a, arr, n, sizeof(int), cmp_int,
                                               ks[ki], &out));
      SCL_EXPECT_EQ_I(tr, out, ref[ks[ki]]);
    }
  }
  free(arr);
  free(ref);
  TEST_TRACE_END();
}

static void test_exp_string(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("exponential string: find + NULL-element guard");
  const char *strs[] = {"alpha", "bravo", "charlie",
                        "delta", "echo",  "foxtrot"};
  size_t n = SCL_ARRAY_SIZE(strs), idx = 0;
  SCL_EXPECT_OK(tr, scl_search_exponential_string(strs, n, "alpha", &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 0);
  SCL_EXPECT_OK(tr, scl_search_exponential_string(strs, n, "echo", &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 4);
  SCL_EXPECT_OK(tr, scl_search_exponential_string(strs, n, "foxtrot", &idx));
  SCL_EXPECT_EQ_SZ(tr, idx, 5);
  SCL_EXPECT_TRUE(tr, scl_search_exponential_string(strs, n, "zulu", &idx) ==
                          SCL_ERR_NOT_FOUND);

  /* A NULL element must be rejected, not dereferenced. */
  const char *bad[] = {"a", NULL, "c", "d"};
  SCL_EXPECT_TRUE(tr, scl_search_exponential_string(bad, 4, "d", &idx) ==
                          SCL_ERR_INVALID_ARG);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_interpolation_adversarial(&tr);
  test_hash_tombstones(&tr);
  test_sentinel(&tr);
  test_quickselect(&tr);
  test_exp_string(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
