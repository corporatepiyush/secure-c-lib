#include "scl_atomic.h"
#include "scl_test.h"

static void test_init_load_store(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atomic init / load / store");

  scl_atomic_int a;
  scl_atomic_init(&a, 42);
  SCL_EXPECT_EQ_I(tr, scl_atomic_load(&a), 42);

  scl_atomic_store(&a, 100);
  SCL_EXPECT_EQ_I(tr, scl_atomic_load(&a), 100);

  scl_atomic_store_explicit(&a, 200, scl_memory_order_release);
  SCL_EXPECT_EQ_I(tr, scl_atomic_load_explicit(&a, scl_memory_order_acquire),
                  200);

  scl_atomic_size_t s;
  scl_atomic_init_sz(&s, 99);
  SCL_EXPECT_EQ_SZ(tr, scl_atomic_load_sz(&s), 99);
  scl_atomic_store_sz_explicit(&s, 50, scl_memory_order_relaxed);
  SCL_EXPECT_EQ_SZ(
      tr, scl_atomic_load_sz_explicit(&s, scl_memory_order_relaxed), 50);
  TEST_TRACE_END();
}

static void test_fetch_add_sub(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atomic fetch_add / fetch_sub");

  scl_atomic_int a;
  scl_atomic_init(&a, 10);
  int prev = scl_atomic_fetch_add_int(&a, 5);
  SCL_EXPECT_EQ_I(tr, prev, 10);
  SCL_EXPECT_EQ_I(tr, scl_atomic_load(&a), 15);

  prev = scl_atomic_fetch_sub_int(&a, 3);
  SCL_EXPECT_EQ_I(tr, prev, 15);
  SCL_EXPECT_EQ_I(tr, scl_atomic_load(&a), 12);

  scl_atomic_size_t s;
  scl_atomic_init_sz(&s, 100);
  size_t sprev = scl_atomic_fetch_add_sz(&s, 20);
  SCL_EXPECT_EQ_SZ(tr, sprev, 100);
  SCL_EXPECT_EQ_SZ(tr, scl_atomic_load_sz(&s), 120);
  TEST_TRACE_END();
}

static void test_fetch_or(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atomic fetch_or");

  scl_atomic_uint u;
  scl_atomic_init_uint(&u, 0);
  unsigned int uprev = scl_atomic_fetch_or_uint(&u, 0x0F);
  SCL_EXPECT_EQ_U(tr, uprev, 0);
  SCL_EXPECT_EQ_U(tr, scl_atomic_load(&u), 0x0Fu);
  TEST_TRACE_END();
}

static void test_flag(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atomic_flag");

  scl_atomic_flag f;
  scl_atomic_init_flag(&f, false);
  SCL_EXPECT_FALSE(tr, scl_atomic_flag_test_and_set(&f));
  SCL_EXPECT_TRUE(tr, scl_atomic_flag_test_and_set(&f));

  scl_atomic_flag_clear(&f);
  SCL_EXPECT_FALSE(
      tr, scl_atomic_flag_test_and_set_explicit(&f, scl_memory_order_acquire));
  TEST_TRACE_END();
}

static void test_cas(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atomic CAS");

  scl_atomic_int a;
  scl_atomic_init(&a, 30);

  int expected = 30;
  int desired = 99;
  bool ok = scl_atomic_cas(&a, &expected, desired);
  SCL_EXPECT_TRUE(tr, ok);
  SCL_EXPECT_EQ_I(tr, scl_atomic_load(&a), 99);

  expected = 30;
  ok = scl_atomic_cas(&a, &expected, 0);
  SCL_EXPECT_FALSE(tr, ok);
  SCL_EXPECT_EQ_I(tr, expected, 99);
  TEST_TRACE_END();
}

static void test_fence(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atomic fence");
  scl_atomic_thread_fence(scl_memory_order_seq_cst);
  SCL_EXPECT_TRUE(tr,
                  1); /* fence smoke test — if it compiles and runs it's OK */
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_init_load_store(&tr);
  test_fetch_add_sub(&tr);
  test_fetch_or(&tr);
  test_flag(&tr);
  test_cas(&tr);
  test_fence(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
