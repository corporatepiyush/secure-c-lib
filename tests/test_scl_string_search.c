/* Correctness tests for the string matchers: Boyer-Moore, KMP, and the suffix
 * trie. Covers first-match, find-all (incl. overlapping), counting, and edge
 * cases. BM and KMP results are cross-checked for agreement. Run under ASan. */
#include "scl_boyer_moore.h"
#include "scl_kmp.h"
#include "scl_suffix_trie.h"
#include "scl_test.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
  size_t arr[64];
  size_t n;
} collector_t;
static bool collect_cb(size_t pos, void *u) {
  collector_t *c = (collector_t *)u;
  if (c->n < 64)
    c->arr[c->n++] = pos;
  return true;
}
static int cmp_sz(const void *a, const void *b) {
  size_t x = *(const size_t *)a, y = *(const size_t *)b;
  return (x > y) - (x < y);
}
/* Check a collector holds exactly `expected[0..m)` (order-independent). */
static int has_exactly(collector_t *c, const size_t *expected, size_t m) {
  if (c->n != m)
    return 0;
  qsort(c->arr, c->n, sizeof(size_t), cmp_sz);
  for (size_t i = 0; i < m; i++)
    if (c->arr[i] != expected[i])
      return 0;
  return 1;
}

static void test_first_match(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BM/KMP: first match + not found + edges");
  scl_allocator_t *a = scl_allocator_default();
  const char *text = "hello world, hello";
  size_t tlen = strlen(text);
  size_t bp = 0, kp = 0;

  SCL_EXPECT_OK(tr, scl_search_boyer_moore(a, text, tlen, "world", 5, &bp));
  SCL_EXPECT_OK(tr, scl_search_kmp(a, text, tlen, "world", 5, &kp));
  SCL_EXPECT_EQ_SZ(tr, bp, 6);
  SCL_EXPECT_EQ_SZ(tr, kp, 6);

  /* first "hello" at 0 */
  SCL_EXPECT_OK(tr, scl_search_boyer_moore(a, text, tlen, "hello", 5, &bp));
  SCL_EXPECT_EQ_SZ(tr, bp, 0);

  /* not found */
  SCL_EXPECT_TRUE(tr, scl_search_boyer_moore(a, text, tlen, "xyz", 3, &bp) ==
                          SCL_ERR_NOT_FOUND);
  SCL_EXPECT_TRUE(tr, scl_search_kmp(a, text, tlen, "xyz", 3, &kp) ==
                          SCL_ERR_NOT_FOUND);

  /* pattern longer than text */
  SCL_EXPECT_TRUE(tr, scl_search_boyer_moore(a, "hi", 2, "hello", 5, &bp) ==
                          SCL_ERR_NOT_FOUND);
  SCL_EXPECT_TRUE(tr, scl_search_kmp(a, "hi", 2, "hello", 5, &kp) ==
                          SCL_ERR_NOT_FOUND);

  /* whole-text match */
  SCL_EXPECT_OK(tr, scl_search_kmp(a, "abc", 3, "abc", 3, &kp));
  SCL_EXPECT_EQ_SZ(tr, kp, 0);
  TEST_TRACE_END();
}

static void test_find_all_overlap(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BM/KMP: find-all incl. overlapping occurrences");
  scl_allocator_t *a = scl_allocator_default();

  /* "aaaa" / "aa" -> overlapping matches at 0,1,2 */
  {
    collector_t bc = {{0}, 0}, kc = {{0}, 0};
    size_t bn = 0, kn = 0;
    SCL_EXPECT_OK(tr, scl_search_boyer_moore_all(a, "aaaa", 4, "aa", 2,
                                                 collect_cb, &bc, &bn));
    SCL_EXPECT_OK(
        tr, scl_search_kmp_all(a, "aaaa", 4, "aa", 2, collect_cb, &kc, &kn));
    size_t exp[] = {0, 1, 2};
    SCL_EXPECT_EQ_SZ(tr, bn, 3);
    SCL_EXPECT_EQ_SZ(tr, kn, 3);
    SCL_EXPECT_TRUE(tr, has_exactly(&bc, exp, 3));
    SCL_EXPECT_TRUE(tr, has_exactly(&kc, exp, 3));
  }
  /* "abababab" / "abab" -> overlapping at 0,2,4 */
  {
    collector_t bc = {{0}, 0}, kc = {{0}, 0};
    size_t bn = 0, kn = 0;
    SCL_EXPECT_OK(tr, scl_search_boyer_moore_all(a, "abababab", 8, "abab", 4,
                                                 collect_cb, &bc, &bn));
    SCL_EXPECT_OK(tr, scl_search_kmp_all(a, "abababab", 8, "abab", 4,
                                         collect_cb, &kc, &kn));
    size_t exp[] = {0, 2, 4};
    SCL_EXPECT_TRUE(tr, has_exactly(&bc, exp, 3));
    SCL_EXPECT_TRUE(tr, has_exactly(&kc, exp, 3));
  }
  /* count-only mode (cb == NULL) */
  {
    size_t bn = 0, kn = 0;
    SCL_EXPECT_OK(tr, scl_search_boyer_moore_all(a, "mississippi", 11, "issi",
                                                 4, NULL, NULL, &bn));
    SCL_EXPECT_OK(tr, scl_search_kmp_all(a, "mississippi", 11, "issi", 4, NULL,
                                         NULL, &kn));
    SCL_EXPECT_EQ_SZ(tr, bn, 2); /* issi at 1 and 4 (overlapping) */
    SCL_EXPECT_EQ_SZ(tr, kn, 2);
  }
  /* no occurrence */
  {
    size_t n = 99;
    SCL_EXPECT_TRUE(tr, scl_search_kmp_all(a, "abcabc", 6, "abcd", 4, NULL,
                                           NULL, &n) == SCL_ERR_NOT_FOUND);
    SCL_EXPECT_EQ_SZ(tr, n, 0);
  }
  TEST_TRACE_END();
}

static void test_suffix_trie(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("suffix trie: contains / count / find_all");
  scl_allocator_t *a = scl_allocator_default();
  const char *text = "banana";
  scl_suffix_trie_t *t = NULL;
  SCL_EXPECT_OK(tr, scl_suffix_trie_build(a, &t, text, 6, 0));
  SCL_EXPECT_NOT_NULL(tr, t);

  SCL_EXPECT_TRUE(tr, scl_suffix_trie_contains(t, "ana", 3));
  SCL_EXPECT_TRUE(tr, scl_suffix_trie_contains(t, "banana", 6));
  SCL_EXPECT_TRUE(tr, scl_suffix_trie_contains(t, "", 0)); /* empty substring */
  SCL_EXPECT_TRUE(tr, !scl_suffix_trie_contains(t, "xyz", 3));
  SCL_EXPECT_TRUE(tr, !scl_suffix_trie_contains(t, "anan a", 6));

  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t, "a", 1), 3); /* a at 1,3,5 */
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t, "ana", 3),
                   2); /* 1,3 (overlap) */
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t, "ban", 3), 1);
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t, "na", 2), 2);
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t, "z", 1), 0);

  {
    collector_t c = {{0}, 0};
    size_t n = 0;
    SCL_EXPECT_OK(tr,
                  scl_suffix_trie_find_all(t, "ana", 3, collect_cb, &c, &n));
    size_t exp[] = {1, 3};
    SCL_EXPECT_EQ_SZ(tr, n, 2);
    SCL_EXPECT_TRUE(tr, has_exactly(&c, exp, 2));
  }
  {
    collector_t c = {{0}, 0};
    size_t n = 0;
    SCL_EXPECT_OK(tr, scl_suffix_trie_find_all(t, "a", 1, collect_cb, &c, &n));
    size_t exp[] = {1, 3, 5};
    SCL_EXPECT_TRUE(tr, has_exactly(&c, exp, 3));
  }
  {
    size_t n = 7;
    SCL_EXPECT_TRUE(tr, scl_suffix_trie_find_all(t, "zzz", 3, NULL, NULL, &n) ==
                            SCL_ERR_NOT_FOUND);
    SCL_EXPECT_EQ_SZ(tr, n, 0);
    SCL_EXPECT_TRUE(tr, scl_suffix_trie_find_all(t, "", 0, NULL, NULL, &n) ==
                            SCL_ERR_INVALID_ARG);
  }
  scl_suffix_trie_destroy(t);

  /* Cross-check the trie against BM for a longer text. */
  const char *big = "the quick brown fox jumps over the lazy dog, the fox runs";
  size_t blen = strlen(big);
  scl_suffix_trie_t *t2 = NULL;
  SCL_EXPECT_OK(tr, scl_suffix_trie_build(a, &t2, big, blen, 0));
  size_t bm_n = 0;
  SCL_EXPECT_OK(tr, scl_search_boyer_moore_all(a, big, blen, "the", 3, NULL,
                                               NULL, &bm_n));
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t2, "the", 3), bm_n);
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t2, "fox", 3), 2);
  scl_suffix_trie_destroy(t2);
  TEST_TRACE_END();
}

static void test_suffix_trie_bounds(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("suffix trie: max-length bound rejects large text");
  scl_allocator_t *a = scl_allocator_default();
  char buf[200];
  memset(buf, 'a', sizeof(buf));
  scl_suffix_trie_t *t = NULL;
  /* text length 200 with max_len 100 must be rejected (memory bound). */
  SCL_EXPECT_TRUE(tr, scl_suffix_trie_build(a, &t, buf, 200, 100) ==
                          SCL_ERR_SIZE_OVERFLOW);
  SCL_EXPECT_NULL(tr, t);
  /* within bound it builds */
  SCL_EXPECT_OK(tr, scl_suffix_trie_build(a, &t, buf, 100, 100));
  SCL_EXPECT_EQ_SZ(tr, scl_suffix_trie_count(t, "aaa", 3), 98);
  scl_suffix_trie_destroy(t);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_first_match(&tr);
  test_find_all_overlap(&tr);
  test_suffix_trie(&tr);
  test_suffix_trie_bounds(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
