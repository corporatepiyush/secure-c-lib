#include "scl_deque.h"
#include "scl_test.h"

static void test_deque_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Deque: init and destroy");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_deque_t deque;

  scl_error_t err = scl_deque_init(alloc, &deque, sizeof(int), 10);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_deque_count(&deque), 0);
  SCL_EXPECT_TRUE(tr, scl_deque_empty(&deque));

  scl_deque_destroy(alloc, &deque);
  TEST_TRACE_END();
}

static void test_deque_push_pop_front(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Deque: push and pop front");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_deque_t deque;
  scl_deque_init(alloc, &deque, sizeof(int), 10);

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    scl_error_t err = scl_deque_push_front(alloc, &deque, &values[i]);
    SCL_EXPECT_OK(tr, err);
  }
  SCL_EXPECT_EQ_SZ(tr, scl_deque_count(&deque), 3);

  int out;
  scl_error_t err = scl_deque_pop_front(&deque, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 30);

  scl_deque_destroy(alloc, &deque);
  TEST_TRACE_END();
}

static void test_deque_push_pop_back(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Deque: push and pop back");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_deque_t deque;
  scl_deque_init(alloc, &deque, sizeof(int), 10);

  int values[] = {10, 20, 30};
  for (int i = 0; i < 3; i++) {
    scl_error_t err = scl_deque_push_back(alloc, &deque, &values[i]);
    SCL_EXPECT_OK(tr, err);
  }

  int out;
  scl_error_t err = scl_deque_pop_back(&deque, &out);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_I(tr, out, 30);

  scl_deque_destroy(alloc, &deque);
  TEST_TRACE_END();
}

static void test_deque_peek_front_back(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Deque: peek front and back");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_deque_t deque;
  scl_deque_init(alloc, &deque, sizeof(int), 10);

  int val_front = 10, val_back = 30;
  scl_deque_push_front(alloc, &deque, &val_front);
  int mid = 20;
  scl_deque_push_back(alloc, &deque, &mid);
  scl_deque_push_back(alloc, &deque, &val_back);

  int out;
  scl_deque_peek_front(&deque, &out);
  SCL_EXPECT_EQ_I(tr, out, 10);

  scl_deque_peek_back(&deque, &out);
  SCL_EXPECT_EQ_I(tr, out, 30);

  scl_deque_destroy(alloc, &deque);
  TEST_TRACE_END();
}

static void test_deque_mixed_ops(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Deque: mixed operations");

  scl_allocator_t *alloc = scl_allocator_default();
  scl_deque_t deque;
  scl_deque_init(alloc, &deque, sizeof(int), 10);

  int v1 = 1, v2 = 2, v3 = 3;
  scl_deque_push_back(alloc, &deque, &v1);
  scl_deque_push_front(alloc, &deque, &v2);
  scl_deque_push_back(alloc, &deque, &v3);

  SCL_EXPECT_EQ_SZ(tr, scl_deque_count(&deque), 3);

  scl_deque_destroy(alloc, &deque);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_deque_init_destroy(&tr);
  test_deque_push_pop_front(&tr);
  test_deque_push_pop_back(&tr);
  test_deque_peek_front_back(&tr);
  test_deque_mixed_ops(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
