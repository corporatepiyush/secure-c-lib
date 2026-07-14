#include "scl_sparse.h"
#include "scl_test.h"

static void combine_min(void *out, const void *a, const void *b) {
  int va = *(int *)a, vb = *(int *)b;
  *(int *)out = va < vb ? va : vb;
}

static void test_sparse_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Sparse: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_sparse_t st;
  int data[] = {3, 2, -1, 6, 5, 4, -2, 3};
  scl_error_t err =
      scl_sparse_init(alloc, &st, 8, sizeof(int), data, combine_min);
  SCL_EXPECT_OK(tr, err);
  scl_sparse_destroy(alloc, &st);
  TEST_TRACE_END();
}

static void test_sparse_query(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Sparse: query");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_sparse_t st;
  int data[] = {3, 2, 1, 6, 5, 4, 2, 3};
  scl_sparse_init(alloc, &st, 8, sizeof(int), data, combine_min);
  int result;
  scl_error_t err = scl_sparse_query(&st, 2, 4, &result);
  SCL_EXPECT_OK(tr, err);
  scl_sparse_destroy(alloc, &st);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_sparse_init_destroy(&tr);
  test_sparse_query(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
