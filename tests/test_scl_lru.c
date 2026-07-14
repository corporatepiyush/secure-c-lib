#include "scl_lru.h"
#include "scl_test.h"

static void test_lru_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("LRU: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_lru_t cache;
  scl_error_t err = scl_lru_init(alloc, &cache, sizeof(int), sizeof(int), 10);
  SCL_EXPECT_OK(tr, err);
  scl_lru_destroy(alloc, &cache);
  TEST_TRACE_END();
}

static void test_lru_set_get(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("LRU: put and get");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_lru_t cache;
  scl_error_t err = scl_lru_init(alloc, &cache, sizeof(int), sizeof(int), 10);
  SCL_EXPECT_OK(tr, err);
  int key = 1, val = 100;
  err = scl_lru_put(alloc, &cache, &key, &val);
  SCL_EXPECT_OK(tr, err);
  int out;
  err = scl_lru_get(&cache, &key, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 100);
  scl_lru_destroy(alloc, &cache);
  TEST_TRACE_END();
}

static void test_lru_contains(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("LRU: contains");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_lru_t cache;
  scl_error_t err = scl_lru_init(alloc, &cache, sizeof(int), sizeof(int), 10);
  SCL_EXPECT_OK(tr, err);
  int key = 5, val = 50;
  err = scl_lru_put(alloc, &cache, &key, &val);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_TRUE(tr, scl_lru_contains(&cache, &key));
  scl_lru_destroy(alloc, &cache);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_lru_init_destroy(&tr);
  test_lru_set_get(&tr);
  test_lru_contains(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
