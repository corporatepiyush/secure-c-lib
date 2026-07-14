#include "scl_atomic.h"
#include "scl_concurrent_rbtree.h"
#include "scl_pthread.h"
#include "scl_test.h"

#define NTHREADS 4
#define OPS_PER_THREAD 500

static int int_cmp(const void *a, const void *b) {
  int va = *(int *)a, vb = *(int *)b;
  return (va > vb) - (va < vb);
}

static void test_crbtree_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CRBTree: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_rbtree_t t;
  scl_error_t err = scl_crbtree_init(alloc, &t, sizeof(int), int_cmp);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_crbtree_count(&t), 0);
  SCL_EXPECT_TRUE(tr, scl_crbtree_empty(&t));
  scl_crbtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

static void test_crbtree_insert_find(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CRBTree: insert and find");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_rbtree_t t;
  scl_crbtree_init(alloc, &t, sizeof(int), int_cmp);

  int v = 42;
  SCL_EXPECT_OK(tr, scl_crbtree_insert(alloc, &t, &v));
  SCL_EXPECT_EQ_SZ(tr, scl_crbtree_count(&t), 1);
  SCL_EXPECT_TRUE(tr, scl_crbtree_contains(&t, &v));

  int out = 0;
  SCL_EXPECT_OK(tr, scl_crbtree_find(&t, &v, &out));
  SCL_EXPECT_EQ_I(tr, out, 42);

  scl_crbtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

static void test_crbtree_remove(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CRBTree: remove");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_rbtree_t t;
  scl_crbtree_init(alloc, &t, sizeof(int), int_cmp);

  int v = 7;
  scl_crbtree_insert(alloc, &t, &v);
  SCL_EXPECT_TRUE(tr, scl_crbtree_contains(&t, &v));
  SCL_EXPECT_OK(tr, scl_crbtree_remove(alloc, &t, &v));
  SCL_EXPECT_FALSE(tr, scl_crbtree_contains(&t, &v));
  SCL_EXPECT_EQ_SZ(tr, scl_crbtree_count(&t), 0);

  scl_crbtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

static void test_crbtree_missing(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CRBTree: find missing returns error");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_rbtree_t t;
  scl_crbtree_init(alloc, &t, sizeof(int), int_cmp);
  int key = 999, out;
  SCL_EXPECT_TRUE(tr, scl_crbtree_find(&t, &key, &out) != SCL_OK);
  scl_crbtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

static void test_crbtree_multiple(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CRBTree: multiple entries");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_rbtree_t t;
  scl_crbtree_init(alloc, &t, sizeof(int), int_cmp);

  for (int i = 0; i < 50; i++)
    scl_crbtree_insert(alloc, &t, &i);
  SCL_EXPECT_EQ_SZ(tr, scl_crbtree_count(&t), 50);

  for (int i = 0; i < 50; i++) {
    int out = -1;
    SCL_EXPECT_OK(tr, scl_crbtree_find(&t, &i, &out));
    SCL_EXPECT_EQ_I(tr, out, i);
  }
  scl_crbtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

typedef struct {
  scl_concurrent_rbtree_t *t;
  int base;
} crbtree_arg_t;

static void *crbtree_insert_thread(void *arg) {
  crbtree_arg_t *a = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  for (int i = 0; i < OPS_PER_THREAD; i++) {
    int k = a->base + i;
    scl_crbtree_insert(alloc, a->t, &k);
  }
  return NULL;
}

static void test_crbtree_concurrent_insert(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CRBTree: concurrent inserts");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_rbtree_t t;
  scl_crbtree_init(alloc, &t, sizeof(int), int_cmp);

  pthread_t threads[NTHREADS];
  crbtree_arg_t args[NTHREADS];
  for (int i = 0; i < NTHREADS; i++) {
    args[i] = (crbtree_arg_t){.t = &t, .base = i * OPS_PER_THREAD};
    pthread_create(&threads[i], NULL, crbtree_insert_thread, &args[i]);
  }
  for (int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

  SCL_EXPECT_EQ_SZ(tr, scl_crbtree_count(&t),
                   (size_t)(NTHREADS * OPS_PER_THREAD));
  scl_crbtree_destroy(alloc, &t);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_crbtree_init_destroy(&tr);
  test_crbtree_insert_find(&tr);
  test_crbtree_remove(&tr);
  test_crbtree_missing(&tr);
  test_crbtree_multiple(&tr);
  test_crbtree_concurrent_insert(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
