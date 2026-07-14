#include "scl_btree.h"
#include "scl_test.h"

static int int_cmp(const void *a, const void *b) {
  int va = *(int *)a, vb = *(int *)b;
  return (va > vb) - (va < vb);
}

static void test_btree_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BTree: init and destroy");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_btree_t tree;

  scl_error_t err =
      scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 0);
  SCL_EXPECT_TRUE(tr, scl_btree_empty(&tree));

  scl_btree_destroy(alloc, &tree);
  TEST_TRACE_END();
}

static void test_btree_insert_get(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BTree: insert and get");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_btree_t tree;
  scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);

  int key = 42, val = 100;
  scl_error_t err = scl_btree_insert(alloc, &tree, &key, &val);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 1);

  int out;
  err = scl_btree_get(&tree, &key, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 100);

  scl_btree_destroy(alloc, &tree);
  TEST_TRACE_END();
}

static void test_btree_contains(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BTree: contains check");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_btree_t tree;
  scl_error_t err =
      scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);
  (void)err;

  int key = 10, val = 99;
  scl_btree_insert(alloc, &tree, &key, &val);

  SCL_EXPECT_TRUE(tr, scl_btree_contains(&tree, &key));

  int other_key = 20;
  SCL_EXPECT_FALSE(tr, scl_btree_contains(&tree, &other_key));

  scl_btree_destroy(alloc, &tree);
  TEST_TRACE_END();
}

static void test_btree_ordered_insert(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BTree: ordered insert");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_btree_t tree;
  scl_error_t err =
      scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 4, int_cmp);
  (void)err;

  int entries[] = {5, 3, 7, 1, 9, 2, 8};
  for (int i = 0; i < 7; i++) {
    int val = entries[i] * 10;
    scl_btree_insert(alloc, &tree, &entries[i], &val);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 7);

  for (int i = 0; i < 7; i++) {
    int out;
    scl_btree_get(&tree, &entries[i], &out);
    SCL_EXPECT_EQ_I(tr, out, entries[i] * 10);
  }

  scl_btree_destroy(alloc, &tree);
  TEST_TRACE_END();
}

static void test_btree_remove(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("BTree: remove");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_btree_t tree;
  scl_error_t err =
      scl_btree_init(alloc, &tree, sizeof(int), sizeof(int), 2, int_cmp);
  (void)err;

  int keys[] = {5, 3, 7, 1, 9, 2, 8, 4, 6};
  int n = (int)(sizeof(keys) / sizeof(keys[0]));
  for (int i = 0; i < n; i++) {
    int val = keys[i] * 10;
    scl_btree_insert(alloc, &tree, &keys[i], &val);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), (size_t)n);

  /* Remove leaves */
  int key1 = 1;
  err = scl_btree_remove(alloc, &tree, &key1);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_FALSE(tr, scl_btree_contains(&tree, &key1));

  /* Remove internal node */
  int key2 = 5;
  err = scl_btree_remove(alloc, &tree, &key2);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_FALSE(tr, scl_btree_contains(&tree, &key2));

  /* Remove remaining */
  int rem[] = {3, 7, 9, 2, 8, 4, 6};
  int rem_n = (int)(sizeof(rem) / sizeof(rem[0]));
  for (int i = 0; i < rem_n; i++) {
    err = scl_btree_remove(alloc, &tree, &rem[i]);
    SCL_EXPECT_OK(tr, err);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree), 0);
  SCL_EXPECT_TRUE(tr, scl_btree_empty(&tree));

  /* Remove from empty returns not found */
  int no_key = 99;
  err = scl_btree_remove(alloc, &tree, &no_key);
  SCL_EXPECT_ERROR(tr, err, SCL_ERR_NOT_FOUND);

  scl_btree_destroy(alloc, &tree);

  /* Test with degree 3 (larger nodes) */
  scl_btree_t tree2;
  scl_btree_init(alloc, &tree2, sizeof(int), sizeof(int), 3, int_cmp);
  int big_n = 20;
  for (int i = 0; i < big_n; i++) {
    scl_btree_insert(alloc, &tree2, &i, &i);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree2), (size_t)big_n);
  /* Remove every other element */
  for (int i = 0; i < big_n; i += 2) {
    err = scl_btree_remove(alloc, &tree2, &i);
    SCL_EXPECT_OK(tr, err);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_btree_count(&tree2), (size_t)(big_n / 2));
  /* Verify remaining elements exist and have correct values */
  for (int i = 1; i < big_n; i += 2) {
    SCL_EXPECT_TRUE(tr, scl_btree_contains(&tree2, &i));
    int out;
    scl_error_t ge = scl_btree_get(&tree2, &i, &out);
    SCL_EXPECT_OK(tr, ge);
    SCL_EXPECT_EQ_I(tr, out, i);
  }
  scl_btree_destroy(alloc, &tree2);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_btree_init_destroy(&tr);
  test_btree_insert_get(&tr);
  test_btree_contains(&tr);
  test_btree_ordered_insert(&tr);
  test_btree_remove(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}