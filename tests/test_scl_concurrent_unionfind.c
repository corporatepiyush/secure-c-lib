#include "scl_atomic.h"
#include "scl_concurrent_unionfind.h"
#include "scl_pthread.h"
#include "scl_test.h"

#define NTHREADS 4
#define N 128

static void test_cunionfind_init_destroy(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CUnionFind: init and destroy");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_unionfind_t uf;
  scl_error_t err = scl_cunionfind_init(alloc, &uf, 10);
  SCL_EXPECT_OK(tr, err);
  SCL_EXPECT_EQ_SZ(tr, scl_cunionfind_count(&uf), 10);
  SCL_EXPECT_EQ_SZ(tr, scl_cunionfind_sets(&uf), 10);
  scl_cunionfind_destroy(alloc, &uf);
  TEST_TRACE_END();
}

static void test_cunionfind_union(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CUnionFind: union");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_unionfind_t uf;
  scl_cunionfind_init(alloc, &uf, 10);

  SCL_EXPECT_OK(tr, scl_cunionfind_union(&uf, 0, 1));
  SCL_EXPECT_TRUE(tr, scl_cunionfind_connected(&uf, 0, 1));
  SCL_EXPECT_EQ_SZ(tr, scl_cunionfind_sets(&uf), 9);

  scl_cunionfind_destroy(alloc, &uf);
  TEST_TRACE_END();
}

static void test_cunionfind_find(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CUnionFind: find");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_unionfind_t uf;
  scl_cunionfind_init(alloc, &uf, 10);

  scl_cunionfind_union(&uf, 0, 1);
  scl_cunionfind_union(&uf, 1, 2);
  size_t r = scl_cunionfind_find(&uf, 2);
  SCL_EXPECT_TRUE(tr, scl_cunionfind_connected(&uf, 0, 2));
  SCL_EXPECT_EQ_SZ(tr, scl_cunionfind_find(&uf, 0), r);
  SCL_EXPECT_EQ_SZ(tr, scl_cunionfind_sets(&uf), 8);

  scl_cunionfind_destroy(alloc, &uf);
  TEST_TRACE_END();
}

static void test_cunionfind_not_connected(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CUnionFind: not connected");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_unionfind_t uf;
  scl_cunionfind_init(alloc, &uf, 10);
  SCL_EXPECT_FALSE(tr, scl_cunionfind_connected(&uf, 0, 1));
  scl_cunionfind_destroy(alloc, &uf);
  TEST_TRACE_END();
}

typedef struct {
  scl_concurrent_unionfind_t *uf;
} cunionfind_arg_t;

static void *cunionfind_union_thread(void *arg) {
  scl_concurrent_unionfind_t *uf = arg;
  for (size_t i = 0; i < N - 1; i += 2)
    scl_cunionfind_union(uf, i, i + 1);
  return NULL;
}

static void test_cunionfind_concurrent_union(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("CUnionFind: concurrent unions");
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_unionfind_t uf;
  scl_cunionfind_init(alloc, &uf, N);

  pthread_t threads[NTHREADS];
  for (int i = 0; i < NTHREADS; i++)
    pthread_create(&threads[i], NULL, cunionfind_union_thread, &uf);
  for (int i = 0; i < NTHREADS; i++)
    pthread_join(threads[i], NULL);

  for (size_t i = 0; i < N - 1; i += 2)
    SCL_EXPECT_TRUE(tr, scl_cunionfind_connected(&uf, i, i + 1));

  scl_cunionfind_destroy(alloc, &uf);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_cunionfind_init_destroy(&tr);
  test_cunionfind_union(&tr);
  test_cunionfind_find(&tr);
  test_cunionfind_not_connected(&tr);
  test_cunionfind_concurrent_union(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
