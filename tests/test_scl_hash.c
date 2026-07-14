#include "scl_hash.h"
#include "scl_test.h"

static void test_hash_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Hash: init and destroy");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_hash_t ht;

  scl_error_t err = scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16,
                                  scl_hash_djb2, scl_hash_eq_mem);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_hash_count(&ht), 0);

  scl_hash_destroy(alloc, &ht);
  TEST_TRACE_END();
}

static void test_hash_insert_get(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Hash: insert and get");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_hash_t ht;
  scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16, scl_hash_djb2,
                scl_hash_eq_mem);

  int key = 42, val = 100;
  scl_error_t err = scl_hash_insert(alloc, &ht, &key, &val);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_hash_count(&ht), 1);

  int out;
  err = scl_hash_get(&ht, &key, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 100);

  scl_hash_destroy(alloc, &ht);
  TEST_TRACE_END();
}

static void test_hash_contains(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Hash: contains check");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_hash_t ht;
  scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16, scl_hash_djb2,
                scl_hash_eq_mem);

  int key = 10, val = 99;
  scl_hash_insert(alloc, &ht, &key, &val);

  SCL_EXPECT_TRUE(tr, scl_hash_contains(&ht, &key));

  int other_key = 20;
  SCL_EXPECT_FALSE(tr, scl_hash_contains(&ht, &other_key));

  scl_hash_destroy(alloc, &ht);
  TEST_TRACE_END();
}

static void test_hash_remove(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Hash: remove");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_hash_t ht;
  scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16, scl_hash_djb2,
                scl_hash_eq_mem);

  int key = 5, val = 50;
  scl_hash_insert(alloc, &ht, &key, &val);
  SCL_EXPECT_EQ_SZ(tr, scl_hash_count(&ht), 1);

  scl_error_t err = scl_hash_remove(alloc, &ht, &key);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_hash_count(&ht), 0);
  SCL_EXPECT_FALSE(tr, scl_hash_contains(&ht, &key));

  scl_hash_destroy(alloc, &ht);
  TEST_TRACE_END();
}

static void test_hash_multiple_entries(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Hash: multiple entries");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_hash_t ht;
  scl_hash_init(alloc, &ht, sizeof(int), sizeof(int), 16, scl_hash_djb2,
                scl_hash_eq_mem);

  int entries[] = {1, 2, 3, 4, 5};
  for (int i = 0; i < 5; i++) {
    int val = entries[i] * 10;
    scl_hash_insert(alloc, &ht, &entries[i], &val);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_hash_count(&ht), 5);

  for (int i = 0; i < 5; i++) {
    int out;
    scl_hash_get(&ht, &entries[i], &out);
    SCL_EXPECT_EQ_I(tr, out, entries[i] * 10);
  }

  scl_hash_destroy(alloc, &ht);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_hash_init_destroy(&tr);
  test_hash_insert_get(&tr);
  test_hash_contains(&tr);
  test_hash_remove(&tr);
  test_hash_multiple_entries(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
