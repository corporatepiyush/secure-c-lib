/* Auto-split from test_scl_ml.c */

#include "preprocessing/scl_ml_metrics.h"
#include "scl_alloc_arena.h"
#include "scl_ml.h"
#include "scl_test.h"
#include "svm/scl_ml_svm.h"
#include <math.h>
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps)                                             \
  do {                                                                         \
    float _d = (float)fabs((double)(a) - (double)(b));                         \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__,                        \
                    "near check: " #a " ~ " #b);                               \
  } while (0)

/* ── SVM Tests ── */

static void test_svm_linear(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("svm_linear");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_dataset_t ds;
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 2));
  for (size_t i = 0; i < 5; i++) {
    ds.data[i * ds.row_stride + 0] = 0.0f;
    ds.data[i * ds.row_stride + 1] = 0.0f;
    ds.targets[i] = -1.0f;
  }
  for (size_t i = 5; i < 10; i++) {
    ds.data[i * ds.row_stride + 0] = 10.0f;
    ds.data[i * ds.row_stride + 1] = 10.0f;
    ds.targets[i] = 1.0f;
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_svm_params_t params = SCL_ML_SVM_PARAMS_DEFAULT();
  params.kernel = SCL_ML_KERNEL_LINEAR;
  params.C = 1.0f;
  params.max_iter = 500;
  params.random_seed = 42;
  params.alloc = a;
  scl_ml_svm_t *model = NULL;
  SCL_EXPECT_OK(tr, scl_ml_svm_new(&model, params));
  SCL_EXPECT_NOT_NULL(tr, model);
  SCL_EXPECT_OK(tr, scl_ml_svm_fit(model, &ds));
  SCL_EXPECT_EQ_SZ(tr, scl_ml_svm_get_n_features(model), 2);
  SCL_EXPECT_TRUE(tr, scl_ml_svm_get_n_support(model) > 0);

  SCL_ML_FLOAT pred[10];
  SCL_EXPECT_OK(tr, scl_ml_svm_predict(model, &ds, pred));
  float acc = scl_ml_accuracy(ds.targets, pred, 10);
  SCL_EXPECT_TRUE(tr, acc > 0.99f);

  scl_ml_svm_free(model);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_svm_rbf(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("svm_rbf");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_dataset_t ds;
  /* Non-linear separable pattern */
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 12, 2));
  for (size_t i = 0; i < 6; i++) {
    ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i;
    ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)i;
    ds.targets[i] = -1.0f;
  }
  for (size_t i = 6; i < 12; i++) {
    ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i + 20);
    ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i + 20);
    ds.targets[i] = 1.0f;
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_svm_params_t params = SCL_ML_SVM_PARAMS_DEFAULT();
  params.kernel = SCL_ML_KERNEL_RBF;
  params.C = 10.0f;
  params.gamma = 0.01f;
  params.tol = 1e-3f;
  params.max_iter = 1000;
  params.random_seed = 42;
  params.alloc = a;
  scl_ml_svm_t *model = NULL;
  SCL_EXPECT_OK(tr, scl_ml_svm_new(&model, params));
  SCL_EXPECT_OK(tr, scl_ml_svm_fit(model, &ds));

  SCL_ML_FLOAT pred[12];
  SCL_EXPECT_OK(tr, scl_ml_svm_predict(model, &ds, pred));
  float acc = scl_ml_accuracy(ds.targets, pred, 12);
  SCL_EXPECT_TRUE(tr, acc > 0.90f);

  scl_ml_svm_free(model);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_svm_errors(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("svm_errors");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_svm_params_t sep = SCL_ML_SVM_PARAMS_DEFAULT();
  sep.alloc = a;
  SCL_EXPECT_ERROR(tr, scl_ml_svm_new(NULL, sep), SCL_ERR_NULL_PTR);
  scl_ml_svm_t *m = NULL;
  SCL_EXPECT_OK(tr, scl_ml_svm_new(&m, sep));
  SCL_EXPECT_ERROR(tr, scl_ml_svm_fit(m, NULL), SCL_ERR_NULL_PTR);
  scl_ml_svm_free(m);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}
static void test_svm_serialization(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("svm_serialization");
  scl_allocator_t *a =
      scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0, 0);
  scl_ml_dataset_t ds;
  SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
  for (size_t i = 0; i < 5; i++) {
    ds.data[i * ds.row_stride] = 0.0f;
    ds.targets[i] = -1.0f;
  }
  for (size_t i = 5; i < 10; i++) {
    ds.data[i * ds.row_stride] = 10.0f;
    ds.targets[i] = 1.0f;
  }
  SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

  scl_ml_svm_t *model = NULL;
  scl_ml_svm_params_t params = SCL_ML_SVM_PARAMS_DEFAULT();
  params.kernel = SCL_ML_KERNEL_LINEAR;
  params.C = 1.0f;
  params.max_iter = 200;
  params.random_seed = 42;
  params.alloc = a;
  SCL_EXPECT_OK(tr, scl_ml_svm_new(&model, params));
  SCL_EXPECT_OK(tr, scl_ml_svm_fit(model, &ds));

  SCL_ML_FLOAT pred_orig[10];
  SCL_EXPECT_OK(tr, scl_ml_svm_predict(model, &ds, pred_orig));
  size_t ns = scl_ml_svm_get_n_support(model);

  uint8_t *buf = NULL;
  size_t len = 0;
  SCL_EXPECT_OK(tr, scl_ml_svm_save(model, &buf, &len, a));
  SCL_EXPECT_TRUE(tr, len > 0);
  scl_ml_svm_free(model);

  scl_ml_svm_params_t load_params = SCL_ML_SVM_PARAMS_DEFAULT();
  load_params.alloc = a;
  scl_ml_svm_t *loaded = NULL;
  SCL_EXPECT_OK(tr, scl_ml_svm_load(&loaded, buf, len, load_params));
  SCL_EXPECT_NOT_NULL(tr, loaded);
  SCL_EXPECT_EQ_SZ(tr, scl_ml_svm_get_n_support(loaded), ns);

  SCL_ML_FLOAT pred_load[10];
  SCL_EXPECT_OK(tr, scl_ml_svm_predict(loaded, &ds, pred_load));
  for (size_t i = 0; i < 10; i++)
    SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

  scl_ml_svm_free(loaded);
  scl_free(a, buf);
  scl_ml_dataset_destroy(&ds, a);
  scl_alloc_arena_destroy(a);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);

  test_svm_linear(&tr);
  test_svm_rbf(&tr);
  test_svm_errors(&tr);
  test_svm_serialization(&tr);

  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
