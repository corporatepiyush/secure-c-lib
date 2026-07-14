#include "scl_heap.h"
#include "scl_test.h"

static int int_cmp_min(const void *a, const void *b) {
  int va = *(int *)a, vb = *(int *)b;
  return (va > vb) - (va < vb);
}

static void test_heap_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Heap: init and destroy");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_heap_t heap;

  scl_error_t err = scl_heap_init(alloc, &heap, sizeof(int), 10, int_cmp_min);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_heap_count(&heap), 0);
  SCL_EXPECT_TRUE(tr, scl_heap_empty(&heap));

  scl_heap_destroy(alloc, &heap);
  TEST_TRACE_END();
}

static void test_heap_push_pop_minheap(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Heap: push and pop (min-heap)");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_heap_t heap;
  scl_heap_init(alloc, &heap, sizeof(int), 10, int_cmp_min);

  int values[] = {30, 10, 20};
  for (int i = 0; i < 3; i++) {
    scl_error_t err = scl_heap_push(alloc, &heap, &values[i]);
    SCL_EXPECT_OK(tr, err);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_heap_count(&heap), 3);

  int out;
  scl_error_t err = scl_heap_pop(&heap, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 10);
  SCL_EXPECT_EQ_SZ(tr, scl_heap_count(&heap), 2);

  scl_heap_destroy(alloc, &heap);
  TEST_TRACE_END();
}

static void test_heap_peek(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Heap: peek without pop");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_heap_t heap;
  scl_heap_init(alloc, &heap, sizeof(int), 10, int_cmp_min);

  int val = 42;
  scl_heap_push(alloc, &heap, &val);

  int out;
  scl_error_t err = scl_heap_peek(&heap, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 42);
  SCL_EXPECT_EQ_SZ(tr, scl_heap_count(&heap), 1);

  scl_heap_destroy(alloc, &heap);
  TEST_TRACE_END();
}

static void test_heap_ordering(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Heap: heap ordering");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_heap_t heap;
  scl_heap_init(alloc, &heap, sizeof(int), 10, int_cmp_min);

  int values[] = {5, 3, 7, 1, 9, 2, 8};
  for (int i = 0; i < 7; i++) {
    scl_heap_push(alloc, &heap, &values[i]);
  }

  int sorted[] = {1, 2, 3, 5, 7, 8, 9};
  for (int i = 0; i < 7; i++) {
    int out;
    scl_heap_pop(&heap, &out);
    SCL_EXPECT_EQ_I(tr, out, sorted[i]);
  }

  scl_heap_destroy(alloc, &heap);
  TEST_TRACE_END();
}

static void test_heap_empty_checks(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Heap: empty checks");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_heap_t heap;
  scl_heap_init(alloc, &heap, sizeof(int), 10, int_cmp_min);

  int out;
  scl_error_t err = scl_heap_pop(&heap, &out);
  SCL_EXPECT_TRUE(tr, err == SCL_ERR_EMPTY);

  scl_heap_destroy(alloc, &heap);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_heap_init_destroy(&tr);
  test_heap_push_pop_minheap(&tr);
  test_heap_peek(&tr);
  test_heap_ordering(&tr);
  test_heap_empty_checks(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
