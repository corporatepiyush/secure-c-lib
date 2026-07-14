/* Auto-split from test_scl_ml.c */

#include "preprocessing/scl_ml_metrics.h"
#include "scl_ml.h"
#include "scl_test.h"
#include <math.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps)                                             \
  do {                                                                         \
    float _d = (float)fabs((double)(a) - (double)(b));                         \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__,                        \
                    "near check: " #a " ~ " #b);                               \
  } while (0)

/* ── Metrics Tests ── */

static void test_metrics_scoring(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("metrics_scoring");
  float yt[5] = {1, 2, 3, 4, 5};
  float yp[5] = {1.1f, 1.9f, 3.2f, 3.8f, 5.1f};
  float mse = scl_ml_mean_squared_error(yt, yp, 5);
  float mae = scl_ml_mean_absolute_error(yt, yp, 5);
  SCL_ML_NEAR(tr, mse, (0.01f + 0.01f + 0.04f + 0.04f + 0.01f) / 5.0f, 1e-5f);
  SCL_ML_NEAR(tr, mae, (0.1f + 0.1f + 0.2f + 0.2f + 0.1f) / 5.0f, 1e-5f);

  float r2 = scl_ml_r2_score(yt, yp, 5);
  float var_y = (4 + 1 + 0 + 1 + 4) / 5.0f;
  float mse_v = 0.11f / 5.0f;
  SCL_ML_NEAR(tr, r2, 1.0f - mse_v / var_y, 1e-4f);

  float acc = scl_ml_accuracy(yt, yp, 5);
  SCL_ML_NEAR(tr, acc, 0.0f, 1e-5f);

  float yc[3] = {0, 1, 0}, ypc[3] = {0, 1, 0};
  SCL_ML_NEAR(tr, scl_ml_accuracy(yc, ypc, 3), 1.0f, 1e-5f);
  TEST_TRACE_END();
}
static void test_metrics_logloss(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("metrics_logloss");
  float yt[4] = {0, 1, 0, 1};
  float yp[4] = {0.1f, 0.9f, 0.2f, 0.8f};
  float ll = scl_ml_log_loss(yt, yp, 4);
  float expected = 0.0f;
  for (size_t i = 0; i < 4; i++) {
    float p = yp[i];
    float t = yt[i];
    float eps = 1e-15f;
    p = fminf(fmaxf(p, eps), 1.0f - eps);
    expected -= (t * logf(p) + (1 - t) * logf(1 - p));
  }
  expected /= 4.0f;
  SCL_ML_NEAR(tr, ll, expected, 1e-4f);
  TEST_TRACE_END();
}
static void test_metrics_distance_matrix(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("metrics_distance_matrix");
  float a[6] = {0, 0, 1, 1, 2, 2};
  float b[4] = {0, 0, 1, 1};
  float out[8];
  SCL_EXPECT_OK(tr,
                scl_ml_distance_matrix(out, a, b, 3, 2, 2, SCL_ML_DISTANCE_L2));
  /* L2 squared distance */
  SCL_ML_NEAR(tr, out[0], 0.0f, 1e-5f);
  SCL_ML_NEAR(tr, out[1], 2.0f, 1e-5f);
  SCL_ML_NEAR(tr, out[2], 2.0f, 1e-5f);
  SCL_ML_NEAR(tr, out[3], 0.0f, 1e-5f);
  SCL_ML_NEAR(tr, out[4], 8.0f, 1e-5f);
  SCL_ML_NEAR(tr, out[5], 2.0f, 1e-5f);
  TEST_TRACE_END();
}
static void test_metrics_kernel(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("metrics_kernel");
  float x[3] = {1, 0, 1};
  float y[3] = {0, 1, 1};
  float k = scl_ml_kernel(x, y, 3, SCL_ML_KERNEL_RBF, 1.0f, 0, 3);
  /* K(x,y) = exp(-||x-y||^2) = exp(-(1+1+0)) = exp(-2) */
  SCL_ML_NEAR(tr, k, expf(-2.0f), 1e-4f);
  TEST_TRACE_END();
}
static void test_metrics_errors(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("metrics_errors");
  SCL_EXPECT_ERROR(tr, scl_ml_distance_matrix(NULL, NULL, NULL, 0, 0, 0, 0),
                   SCL_ERR_NULL_PTR);
  float a[1] = {0}, b[1] = {0}, o[1];
  SCL_EXPECT_ERROR(tr, scl_ml_distance_matrix(o, NULL, b, 1, 1, 1, 0),
                   SCL_ERR_NULL_PTR);
  SCL_EXPECT_ERROR(tr, scl_ml_distance_matrix(o, a, NULL, 1, 1, 1, 0),
                   SCL_ERR_NULL_PTR);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_metrics_scoring(&tr);
  test_metrics_logloss(&tr);
  test_metrics_distance_matrix(&tr);
  test_metrics_kernel(&tr);
  test_metrics_errors(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
