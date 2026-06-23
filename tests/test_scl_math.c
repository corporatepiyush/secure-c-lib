#include "scl_test.h"
#include "scl_math.h"
#include <math.h>

static void test_sqrt(scl_test_runner_t *tr) {
    scl_test_group("scl_sqrt");
    SCL_EXPECT_EQ_I(tr, scl_sqrt(0.0) == 0.0, 1);
    SCL_EXPECT_EQ_I(tr, scl_sqrt(1.0) == 1.0, 1);
    SCL_EXPECT_EQ_I(tr, scl_sqrt(4.0) == 2.0, 1);
    SCL_EXPECT_TRUE(tr, scl_isnan(scl_sqrt(-1.0)));
}

static void test_log(scl_test_runner_t *tr) {
    scl_test_group("scl_log");
    SCL_EXPECT_EQ_I(tr, scl_log(1.0) == 0.0, 1);
    SCL_EXPECT_TRUE(tr, scl_isinf(scl_log(0.0)));
    SCL_EXPECT_TRUE(tr, scl_isnan(scl_log(-1.0)));
}

static void test_pow(scl_test_runner_t *tr) {
    scl_test_group("scl_pow");
    SCL_EXPECT_EQ_I(tr, scl_pow(2.0, 3.0) == 8.0, 1);
    SCL_EXPECT_EQ_I(tr, scl_pow(0.0, 0.0) == 1.0, 1);
}

static void test_fmod(scl_test_runner_t *tr) {
    scl_test_group("scl_fmod");
    SCL_EXPECT_EQ_I(tr, scl_fmod(10.0, 3.0) == 1.0, 1);
    SCL_EXPECT_TRUE(tr, scl_isnan(scl_fmod(10.0, 0.0)));
}

static void test_abs_fp(scl_test_runner_t *tr) {
    scl_test_group("scl_fabs");
    SCL_EXPECT_EQ_I(tr, scl_fabs(-3.5) == 3.5, 1);
    SCL_EXPECT_EQ_I(tr, scl_fabs(3.5) == 3.5, 1);
}

static void test_classify(scl_test_runner_t *tr) {
    scl_test_group("scl_isnan/isinf/isfinite");
    SCL_EXPECT_TRUE(tr, scl_isnan(NAN));
    SCL_EXPECT_TRUE(tr, scl_isinf(HUGE_VAL));
    SCL_EXPECT_TRUE(tr, scl_isfinite(3.14));
    SCL_EXPECT_FALSE(tr, scl_isfinite(HUGE_VAL));
}

static void test_min_max_clamp(scl_test_runner_t *tr) {
    scl_test_group("scl_min/max/clamp");
    SCL_EXPECT_EQ_I(tr, scl_min_i64(1, 2), 1);
    SCL_EXPECT_EQ_I(tr, scl_max_i64(1, 2), 2);
    SCL_EXPECT_EQ_I(tr, scl_min_u64(5, 10), 5);
    SCL_EXPECT_EQ_I(tr, scl_max_u64(5, 10), 10);
    SCL_EXPECT_EQ_I(tr, scl_min_f64(1.5, 2.5) == 1.5, 1);
    SCL_EXPECT_EQ_I(tr, scl_max_f64(1.5, 2.5) == 2.5, 1);
    SCL_EXPECT_EQ_I(tr, scl_clamp_f64(5.0, 0.0, 3.0) == 3.0, 1);
    SCL_EXPECT_EQ_I(tr, scl_clamp_f64(-1.0, 0.0, 3.0) == 0.0, 1);
}

static void test_trig(scl_test_runner_t *tr) {
    scl_test_group("scl_sin/cos");
    SCL_EXPECT_EQ_I(tr, scl_sin(0.0) == 0.0, 1);
    SCL_EXPECT_EQ_I(tr, scl_cos(0.0) == 1.0, 1);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_sqrt(&tr);
    test_log(&tr);
    test_pow(&tr);
    test_fmod(&tr);
    test_abs_fp(&tr);
    test_classify(&tr);
    test_min_max_clamp(&tr);
    test_trig(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
