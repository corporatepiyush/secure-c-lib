#include "scl_atomic.h"
#include "scl_concurrent_stack.h"
#include "scl_pthread.h"
#include "scl_test.h"
#include <sched.h>

#define NTHREADS 4
#define OPS_PER_THREAD 1000

static void test_cstack_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CStack: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_stack_t s;
  scl_error_t err = scl_cstack_init(alloc, &s, sizeof(int));
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_cstack_count(&s), 0);
  SCL_EXPECT_TRUE(tr, scl_cstack_empty(&s));
  scl_cstack_destroy(alloc, &s);
  TEST_TRACE_END();
}

static void test_cstack_push_pop(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CStack: push and pop");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_stack_t s;
  scl_cstack_init(alloc, &s, sizeof(int));

  int v = 99;
  SCL_EXPECT_OK(tr, scl_cstack_push(alloc, &s, &v));
  SCL_EXPECT_EQ_SZ(tr, scl_cstack_count(&s), 1);
  SCL_EXPECT_FALSE(tr, scl_cstack_empty(&s));

  int out = 0;
  SCL_EXPECT_OK(tr, scl_cstack_pop(alloc, &s, &out));
  SCL_EXPECT_EQ_I(tr, out, 99);
  SCL_EXPECT_TRUE(tr, scl_cstack_empty(&s));

  scl_cstack_destroy(alloc, &s);
  TEST_TRACE_END();
}

static void test_cstack_lifo_order(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CStack: LIFO ordering");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_stack_t s;
  scl_cstack_init(alloc, &s, sizeof(int));

  for (int i = 0; i < 8; i++)
    scl_cstack_push(alloc, &s, &i);

  for (int i = 7; i >= 0; i--) {
    int out = -1;
    scl_cstack_pop(alloc, &s, &out);
    SCL_EXPECT_EQ_I(tr, out, i);
  }
  scl_cstack_destroy(alloc, &s);
  TEST_TRACE_END();
}

static void test_cstack_empty_pop(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CStack: pop from empty returns error");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_stack_t s;
  scl_cstack_init(alloc, &s, sizeof(int));
  int out;
  scl_error_t err = scl_cstack_pop(alloc, &s, &out);
  SCL_EXPECT_TRUE(tr, err != SCL_OK);
  scl_cstack_destroy(alloc, &s);
  TEST_TRACE_END();
}

static atomic_int push_count;
static atomic_int pop_count;

static void *push_thread(void *arg) {
  scl_concurrent_stack_t *s = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  for (int i = 0; i < OPS_PER_THREAD; i++) {
    int v = i;
    while (scl_cstack_push(alloc, s, &v) != SCL_OK)
      sched_yield();
    atomic_fetch_add(&push_count, 1);
  }
  return NULL;
}

static void *pop_thread(void *arg) {
  scl_concurrent_stack_t *s = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  int total = NTHREADS * OPS_PER_THREAD;
  while (atomic_load(&pop_count) < total) {
    int out;
    if (scl_cstack_pop(alloc, s, &out) == SCL_OK)
      atomic_fetch_add(&pop_count, 1);
    else
      sched_yield();
  }
  return NULL;
}

static void test_cstack_concurrent(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CStack: concurrent push/pop");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_stack_t s;
  scl_cstack_init(alloc, &s, sizeof(int));

  atomic_init(&push_count, 0);
  atomic_init(&pop_count, 0);

  pthread_t pushers[NTHREADS], popper;
  for (int i = 0; i < NTHREADS; i++)
    pthread_create(&pushers[i], NULL, push_thread, &s);
  pthread_create(&popper, NULL, pop_thread, &s);

  for (int i = 0; i < NTHREADS; i++)
    pthread_join(pushers[i], NULL);
  pthread_join(popper, NULL);

  SCL_EXPECT_EQ_I(tr, atomic_load(&pop_count), NTHREADS * OPS_PER_THREAD);
  SCL_EXPECT_TRUE(tr, scl_cstack_empty(&s));

  scl_cstack_destroy(alloc, &s);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cstack_init_destroy(&tr);
  test_cstack_push_pop(&tr);
  test_cstack_lifo_order(&tr);
  test_cstack_empty_pop(&tr);
  test_cstack_concurrent(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
