#include "scl_stdlib.h"
#include "scl_test.h"
#include <limits.h>

static void test_atoi(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atoi");
  SCL_EXPECT_EQ_I(tr, scl_atoi("123"), 123);
  SCL_EXPECT_EQ_I(tr, scl_atoi("-1"), -1);
  SCL_EXPECT_EQ_I(tr, scl_atoi("0"), 0);
  SCL_EXPECT_EQ_I(tr, scl_atoi(NULL), 0);
  SCL_EXPECT_EQ_I(tr, scl_atoi(""), 0);
  TEST_TRACE_END();
}

static void test_atol(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_atol");
  SCL_EXPECT_EQ_I(tr, scl_atol("9223372036854775807"), 9223372036854775807L);
  SCL_EXPECT_EQ_I(tr, scl_atol(NULL), 0);
  TEST_TRACE_END();
}

static void test_strtol(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_strtol");
  char *end = NULL;
  SCL_EXPECT_EQ_I(tr, scl_strtol("42", &end, 10), 42);
  SCL_EXPECT_EQ_I(tr, scl_strtol(NULL, NULL, 0), 0);
  end = NULL;
  SCL_EXPECT_EQ_I(tr, scl_strtol("ff", &end, 16), 255);
  TEST_TRACE_END();
}

static void test_abs(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_abs");
  SCL_EXPECT_EQ_I(tr, scl_abs(5), 5);
  SCL_EXPECT_EQ_I(tr, scl_abs(-5), 5);
  SCL_EXPECT_EQ_I(tr, scl_abs(0), 0);
  SCL_EXPECT_EQ_I(tr, scl_abs(INT_MIN), INT_MAX);
  TEST_TRACE_END();
}

static void test_labs(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_labs");
  SCL_EXPECT_EQ_I(tr, scl_labs(LONG_MIN), LONG_MAX);
  TEST_TRACE_END();
}

static void test_rand(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_rand");
  int a = scl_rand();
  int b = scl_rand();
  (void)a;
  (void)b;
  SCL_EXPECT_TRUE(tr, 1);
  TEST_TRACE_END();
}

static void test_getenv(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("scl_getenv");
  SCL_EXPECT_NOT_NULL(tr, scl_getenv("PATH"));
  SCL_EXPECT_NULL(tr, scl_getenv(NULL));
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_atoi(&tr);
  test_atol(&tr);
  test_strtol(&tr);
  test_abs(&tr);
  test_labs(&tr);
  test_rand(&tr);
  test_getenv(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
