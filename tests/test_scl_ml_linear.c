/* Auto-split from test_scl_ml.c */

#include "linear_model/scl_ml_linear.h"
#include "preprocessing/scl_ml_metrics.h"
#include "scl_alloc_arena.h"
#include "scl_ml.h"
#include "scl_test.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps)                                             \
  do {                                                                         \
    float _d = (float)fabs((double)(a) - (double)(b));                         \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__,                        \
                    "near check: " #a " ~ " #b);                               \
  } while (0)

/* ── Linear Model Tests ── */

static void test_linear_ols_normal_eq(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("linear_ols_normal_eq");
  /* y = 3*x0 - 2*x1 + 5, generate data with noise */
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_dataset_t ds;
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 50, 2));
  srand(42);
  for (size_t i = 0; i < 50; i++) {
    float x0 = (float)((double)rand() / (double)RAND_MAX * 10.0);
    float x1 = (float)((double)rand() / (double)RAND_MAX * 10.0);
    float noise = (float)((double)rand() / (double)RAND_MAX * 0.1 - 0.05);
    ds.data[i * ds.row_stride + 0] = x0;
    ds.data[i * ds.row_stride + 1] = x1;
    ds.targets[i] = 3.0f * x0 - 2.0f * x1 + 5.0f + noise;
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
  params.solver = SCL_ML_SOLVER_NORMAL_EQ;
  params.penalty = SCL_ML_PENALTY_NONE;
  params.alloc = a;

  scl_ml_linear_t *model = NULL;
  SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
  SCL_EXPECT_NOT_NULL(tr, model);

  SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

  /* Check weights */
  const SCL_ML_FLOAT *w = scl_ml_linear_get_weights(model);
  float intercept = scl_ml_linear_get_intercept(model);
  SCL_EXPECT_NOT_NULL(tr, w);
  SCL_ML_NEAR(tr, w[0], 3.0f, 0.5f);
  SCL_ML_NEAR(tr, w[1], -2.0f, 0.5f);
  SCL_ML_NEAR(tr, intercept, 5.0f, 0.5f);

  /* Predict and check R2 near 1 */
  float *pred = (float *)calloc(50, sizeof(float));
  SCL_EXPECT_OK(tr, scl_ml_linear_predict(model, &ds, pred));
  float r2 = scl_ml_r2_score(ds.targets, pred, 50);
  SCL_EXPECT_TRUE(tr, r2 > 0.99f);
  free(pred);

  scl_ml_linear_free(model);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_linear_ridge_sgd(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("linear_ridge_sgd");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_dataset_t ds;
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 30, 1));
  for (size_t i = 0; i < 30; i++) {
    float x = (float)i;
    ds.data[i * ds.row_stride] = x;
    ds.targets[i] = 2.0f * x - 1.0f;
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
  params.solver = SCL_ML_SOLVER_SGD;
  params.penalty = SCL_ML_PENALTY_L2;
  params.alpha = 0.001;
  params.learning_rate = 0.01;
  params.max_iter = 3000;
  params.batch_size = 0;
  params.alloc = a;

  scl_ml_linear_t *model = NULL;
  SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
  SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

  float *pred = (float *)calloc(30, sizeof(float));
  SCL_EXPECT_OK(tr, scl_ml_linear_predict(model, &ds, pred));
  float r2 = scl_ml_r2_score(ds.targets, pred, 30);
  SCL_EXPECT_TRUE(tr, r2 > 0.95f);
  free(pred);

  scl_ml_linear_free(model);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_linear_lasso_cd(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("linear_lasso_cd");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_dataset_t ds;
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 3));
  for (size_t i = 0; i < 20; i++) {
    ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i);
    ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 2);
    ds.data[i * ds.row_stride + 2] = (SCL_ML_FLOAT)(i * 3);
    ds.targets[i] = (SCL_ML_FLOAT)(i);
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
  params.solver = SCL_ML_SOLVER_CD;
  params.penalty = SCL_ML_PENALTY_L1;
  params.alpha = 0.01;
  params.max_iter = 1000;
  params.alloc = a;

  scl_ml_linear_t *model = NULL;
  SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
  SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

  float *pred = (float *)calloc(20, sizeof(float));
  SCL_EXPECT_OK(tr, scl_ml_linear_predict(model, &ds, pred));
  float r2 = scl_ml_r2_score(ds.targets, pred, 20);
  SCL_EXPECT_TRUE(tr, r2 > 0.90f);
  free(pred);

  scl_ml_linear_free(model);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_linear_model_accessors(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("linear_model_accessors");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_linear_params_t ap = SCL_ML_LINEAR_PARAMS_DEFAULT();
  ap.alloc = a;
  scl_ml_linear_t *m = NULL;
  SCL_EXPECT_OK(tr, scl_ml_linear_new(&m, ap));
  SCL_EXPECT_EQ_SZ(tr, scl_ml_linear_get_n_features(m), 0);
  SCL_ML_NEAR(tr, scl_ml_linear_get_intercept(m), 0.0f, 1e-5f);
  SCL_EXPECT_NULL(tr, scl_ml_linear_get_weights(m));
  scl_ml_linear_free(m);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_linear_errors(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("linear_errors");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_linear_params_t ep = SCL_ML_LINEAR_PARAMS_DEFAULT();
  ep.alloc = a;
  scl_ml_linear_t *m = NULL;

  SCL_EXPECT_ERROR(tr, scl_ml_linear_new(NULL, ep), SCL_ERR_NULL_PTR);

  scl_ml_linear_params_t params = ep;
  SCL_EXPECT_OK(tr, scl_ml_linear_new(&m, params));

  SCL_EXPECT_ERROR(tr, scl_ml_linear_fit(m, NULL), SCL_ERR_NULL_PTR);

  scl_ml_linear_free(m);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}

static void test_linear_serialization(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("linear_serialization");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);

  scl_ml_dataset_t ds;
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 2));
  srand(42);
  for (size_t i = 0; i < 10; i++) {
    float x0 = (float)(i);
    float x1 = (float)(rand() % 100);
    ds.data[i * ds.row_stride + 0] = x0;
    ds.data[i * ds.row_stride + 1] = x1;
    ds.targets[i] = 3.0f * x0 + 2.0f * x1 + 1.0f;
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
  params.solver = SCL_ML_SOLVER_NORMAL_EQ;
  params.alloc = a;

  scl_ml_linear_t *model = NULL;
  SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
  SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

  /* Save */
  uint8_t *buf = NULL;
  size_t len = 0;
  SCL_EXPECT_OK(tr, scl_ml_linear_save(model, &buf, &len, a));
  SCL_EXPECT_NOT_NULL(tr, buf);
  SCL_EXPECT_TRUE(tr, len > sizeof(scl_ml_serial_header_t));

  const SCL_ML_FLOAT *w_orig = scl_ml_linear_get_weights(model);
  float w_saved[2] = {w_orig[0], w_orig[1]};
  float b_saved = scl_ml_linear_get_intercept(model);
  scl_ml_linear_free(model);

  /* Load */
  scl_ml_linear_params_t load_params = SCL_ML_LINEAR_PARAMS_DEFAULT();
  load_params.alloc = a;
  scl_ml_linear_t *loaded = NULL;
  SCL_EXPECT_OK(tr, scl_ml_linear_load(&loaded, buf, len, load_params));
  SCL_EXPECT_NOT_NULL(tr, loaded);

  const SCL_ML_FLOAT *w_loaded = scl_ml_linear_get_weights(loaded);
  SCL_EXPECT_NOT_NULL(tr, w_loaded);
  SCL_ML_NEAR(tr, scl_ml_linear_get_intercept(loaded), b_saved, 1e-5f);
  for (size_t i = 0; i < 2; i++)
    SCL_ML_NEAR(tr, w_loaded[i], w_saved[i], 1e-5f);

  float *pred = (float *)calloc(10, sizeof(float));
  SCL_EXPECT_OK(tr, scl_ml_linear_predict(loaded, &ds, pred));
  float r2 = scl_ml_r2_score(ds.targets, pred, 10);
  SCL_EXPECT_TRUE(tr, r2 > 0.99f);
  free(pred);

  scl_ml_linear_free(loaded);
  scl_free(a, buf);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_linear_ols_normal_eq(&tr);
  test_linear_ridge_sgd(&tr);
  test_linear_lasso_cd(&tr);
  test_linear_model_accessors(&tr);
  test_linear_serialization(&tr);
  test_linear_errors(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
