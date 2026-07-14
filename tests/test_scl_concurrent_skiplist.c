#include "scl_atomic.h"
#include "scl_concurrent_skiplist.h"
#include "scl_pthread.h"
#include "scl_test.h"

#define NTHREADS 4
#define OPS_PER_THREAD 500

static int int_cmp(const void *a, const void *b) {
  int va = *(int *)a, vb = *(int *)b;
  return (va > vb) - (va < vb);
}

static void test_cskiplist_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSkipList: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_skiplist_t sl;
  scl_error_t err = scl_cskiplist_init(alloc, &sl, sizeof(int), int_cmp);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_cskiplist_count(&sl), 0);
  scl_cskiplist_destroy(alloc, &sl);
  TEST_TRACE_END();
}

static void test_cskiplist_insert_find(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSkipList: insert and find");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_skiplist_t sl;
  scl_cskiplist_init(alloc, &sl, sizeof(int), int_cmp);

  int v = 42;
  SCL_EXPECT_OK(tr, scl_cskiplist_insert(alloc, &sl, &v));
  SCL_EXPECT_EQ_SZ(tr, scl_cskiplist_count(&sl), 1);
  SCL_EXPECT_TRUE(tr, scl_cskiplist_contains(&sl, &v));

  int out = 0;
  SCL_EXPECT_OK(tr, scl_cskiplist_find(&sl, &v, &out));
  SCL_EXPECT_EQ_I(tr, out, 42);

  scl_cskiplist_destroy(alloc, &sl);
  TEST_TRACE_END();
}

static void test_cskiplist_remove(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSkipList: remove");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_skiplist_t sl;
  scl_cskiplist_init(alloc, &sl, sizeof(int), int_cmp);

  int v = 7;
  scl_cskiplist_insert(alloc, &sl, &v);
  SCL_EXPECT_TRUE(tr, scl_cskiplist_contains(&sl, &v));
  SCL_EXPECT_OK(tr, scl_cskiplist_remove(alloc, &sl, &v));
  SCL_EXPECT_FALSE(tr, scl_cskiplist_contains(&sl, &v));
  SCL_EXPECT_EQ_SZ(tr, scl_cskiplist_count(&sl), 0);

  scl_cskiplist_destroy(alloc, &sl);
  TEST_TRACE_END();
}

static void test_cskiplist_missing(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSkipList: find missing returns error");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_skiplist_t sl;
  scl_cskiplist_init(alloc, &sl, sizeof(int), int_cmp);
  int key = 999, out;
  SCL_EXPECT_TRUE(tr, scl_cskiplist_find(&sl, &key, &out) != SCL_OK);
  scl_cskiplist_destroy(alloc, &sl);
  TEST_TRACE_END();
}

static void test_cskiplist_multiple(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSkipList: multiple entries");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_skiplist_t sl;
  scl_cskiplist_init(alloc, &sl, sizeof(int), int_cmp);

  for (int i = 0; i < 50; i++)
    scl_cskiplist_insert(alloc, &sl, &i);
  SCL_EXPECT_EQ_SZ(tr, scl_cskiplist_count(&sl), 50);

  for (int i = 0; i < 50; i++) {
    int out = -1;
    SCL_EXPECT_OK(tr, scl_cskiplist_find(&sl, &i, &out));
    SCL_EXPECT_EQ_I(tr, out, i);
  }
  scl_cskiplist_destroy(alloc, &sl);
  TEST_TRACE_END();
}

typedef struct {
  scl_concurrent_skiplist_t *sl;
  int base;
} cskiplist_arg_t;

static void *cskiplist_insert_thread(void *arg) {
  cskiplist_arg_t *a = arg;
  scl_allocator_t *alloc = scl_allocator_default();
  for (int i = 0; i < OPS_PER_THREAD; i++) {
    int k = a->base + i;
    scl_cskiplist_insert(alloc, a->sl, &k);
  }
  return NULL;
}

static void test_cskiplist_concurrent_insert(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CSkipList: concurrent inserts");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_skiplist_t sl;
  scl_cskiplist_init(alloc, &sl, sizeof(int), int_cmp);

  pthread_t threads[NTHREADS];
  cskiplist_arg_t args[NTHREADS];
  for (int i = 0; i < NTHREADS; i++) {
    args[i] = (cskiplist_arg_t){.sl = &sl, .base = i * OPS_PER_THREAD};
    pthread_create(&threads[i], NULL, cskiplist_insert_thread, &args[i]);
  }
  for (int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

  SCL_EXPECT_EQ_SZ(tr, scl_cskiplist_count(&sl),
                   (size_t)(NTHREADS * OPS_PER_THREAD));
  scl_cskiplist_destroy(alloc, &sl);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cskiplist_init_destroy(&tr);
  test_cskiplist_insert_find(&tr);
  test_cskiplist_remove(&tr);
  test_cskiplist_missing(&tr);
  test_cskiplist_multiple(&tr);
  test_cskiplist_concurrent_insert(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
