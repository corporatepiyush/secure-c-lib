#include "scl_atomic.h"
#include "scl_concurrent_queue.h"
#include "scl_pthread.h"
#include "scl_test.h"
#include <sched.h>

#define NTHREADS 4
#define OPS_PER_THREAD 1000

static void test_cqueue_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CQueue: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_queue_t q;
  scl_error_t err = scl_cqueue_init(alloc, &q, sizeof(int));
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_cqueue_count(&q), 0);
  SCL_EXPECT_TRUE(tr, scl_cqueue_empty(&q));
  scl_cqueue_destroy(alloc, &q);
  TEST_TRACE_END();
}

static void test_cqueue_enqueue_dequeue(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CQueue: enqueue and dequeue");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_queue_t q;
  scl_cqueue_init(alloc, &q, sizeof(int));

  int v = 42;
  SCL_EXPECT_OK(tr, scl_cqueue_enqueue(alloc, &q, &v));
  SCL_EXPECT_EQ_SZ(tr, scl_cqueue_count(&q), 1);
  SCL_EXPECT_FALSE(tr, scl_cqueue_empty(&q));

  int out = 0;
  SCL_EXPECT_OK(tr, scl_cqueue_dequeue(alloc, &q, &out));
  SCL_EXPECT_EQ_I(tr, out, 42);
  SCL_EXPECT_TRUE(tr, scl_cqueue_empty(&q));

  scl_cqueue_destroy(alloc, &q);
  TEST_TRACE_END();
}

static void test_cqueue_fifo_order(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CQueue: FIFO ordering");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_queue_t q;
  scl_cqueue_init(alloc, &q, sizeof(int));

  for (int i = 0; i < 10; i++)
    scl_cqueue_enqueue(alloc, &q, &i);

  for (int i = 0; i < 10; i++) {
    int out = -1;
    scl_cqueue_dequeue(alloc, &q, &out);
    SCL_EXPECT_EQ_I(tr, out, i);
  }
  scl_cqueue_destroy(alloc, &q);
  TEST_TRACE_END();
}

static void test_cqueue_empty_dequeue(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CQueue: dequeue from empty returns error");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_queue_t q;
  scl_cqueue_init(alloc, &q, sizeof(int));
  int out;
  scl_error_t err = scl_cqueue_dequeue(alloc, &q, &out);
  SCL_EXPECT_TRUE(tr, err != SCL_OK);
  scl_cqueue_destroy(alloc, &q);
  TEST_TRACE_END();
}

typedef struct {
  scl_concurrent_queue_t *q;
  int base;
} producer_args_t;

static void *producer_thread(void *arg) {
  producer_args_t *a = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  for (int i = 0; i < OPS_PER_THREAD; i++) {
    int v = a->base + i;
    while (scl_cqueue_enqueue(alloc, a->q, &v) != SCL_OK)
      sched_yield();
  }
  return NULL;
}

static atomic_int consumed_count;

static void *consumer_thread(void *arg) {
  scl_concurrent_queue_t *q = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  int total = NTHREADS * OPS_PER_THREAD;
  while (atomic_load(&consumed_count) < total) {
    int out;
    if (scl_cqueue_dequeue(alloc, q, &out) == SCL_OK)
      atomic_fetch_add(&consumed_count, 1);
    else
      sched_yield();
  }
  return NULL;
}

static void test_cqueue_concurrent_mpsc(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CQueue: multi-producer single-consumer");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_queue_t q;
  scl_cqueue_init(alloc, &q, sizeof(int));

  atomic_init(&consumed_count, 0);

  pthread_t producers[NTHREADS], consumer;
  producer_args_t args[NTHREADS];
  for (int i = 0; i < NTHREADS; i++) {
    args[i] = (producer_args_t){.q = &q, .base = i * OPS_PER_THREAD};
    pthread_create(&producers[i], NULL, producer_thread, &args[i]);
  }
  pthread_create(&consumer, NULL, consumer_thread, &q);

  for (int i = 0; i < NTHREADS; i++)
    pthread_join(producers[i], NULL);
  pthread_join(consumer, NULL);

  SCL_EXPECT_EQ_I(tr, atomic_load(&consumed_count), NTHREADS * OPS_PER_THREAD);
  SCL_EXPECT_TRUE(tr, scl_cqueue_empty(&q));

  scl_cqueue_destroy(alloc, &q);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cqueue_init_destroy(&tr);
  test_cqueue_enqueue_dequeue(&tr);
  test_cqueue_fifo_order(&tr);
  test_cqueue_empty_dequeue(&tr);
  test_cqueue_concurrent_mpsc(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
