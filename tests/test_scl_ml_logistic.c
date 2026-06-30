/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "linear_model/scl_ml_logistic.h"
#include "preprocessing/scl_ml_scaler.h"
#include "preprocessing/scl_ml_metrics.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── Logistic Regression Tests ── */

static void test_logistic_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("logistic_fit_predict");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    /* Binary classification: x<5 -> 0, x>=5 -> 1 */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 2));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i * 0.5f;
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i);
        ds.targets[i] = 0.0f;
    }
    for (size_t i = 10; i < 20; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(5.0f + i * 0.5f);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i + 10);
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_standard_scaler_t *log_scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&log_scaler));
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_fit_transform(log_scaler, &ds));

    scl_ml_logistic_params_t params = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    params.max_iter = 5000;
    params.learning_rate = 0.5f;
    params.alpha = 1e-6f;
    params.random_seed = 42;
    scl_ml_logistic_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_logistic_fit(model, &ds));

    SCL_ML_FLOAT pred[20];
    SCL_EXPECT_OK(tr, scl_ml_logistic_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 20);
    SCL_EXPECT_TRUE(tr, acc > 0.70f);

    scl_ml_logistic_free(model);
    scl_ml_standard_scaler_free(log_scaler);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_logistic_predict_proba(scl_test_runner_t *tr) {
    scl_test_group("logistic_predict_proba");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_standard_scaler_t *log_scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&log_scaler));
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_fit_transform(log_scaler, &ds));

    scl_ml_logistic_params_t params = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    params.max_iter = 5000;
    params.learning_rate = 0.5f;
    params.alpha = 1e-6f;
    params.random_seed = 42;
    scl_ml_logistic_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_logistic_fit(model, &ds));

    SCL_ML_FLOAT proba[10];
    SCL_EXPECT_OK(tr, scl_ml_logistic_predict_proba(model, &ds, proba));
    for (size_t i = 0; i < 10; i++)
        SCL_EXPECT_TRUE(tr, proba[i] >= 0.0f && proba[i] <= 1.0f);

    scl_ml_logistic_free(model);
    scl_ml_standard_scaler_free(log_scaler);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_logistic_getters(scl_test_runner_t *tr) {
    scl_test_group("logistic_getters");
    scl_ml_logistic_params_t p = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    scl_ml_logistic_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&m, p));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_logistic_get_n_features(m), 0);
    SCL_ML_NEAR(tr, scl_ml_logistic_get_intercept(m), 0.0f, 1e-5f);
    SCL_EXPECT_NULL(tr, scl_ml_logistic_get_coef(m));
    scl_ml_logistic_free(m);
}
static void test_logistic_errors(scl_test_runner_t *tr) {
    scl_test_group("logistic_errors");
    scl_ml_logistic_params_t ep = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_logistic_new(NULL, ep), SCL_ERR_NULL_PTR);
    scl_ml_logistic_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&m, ep));
    SCL_EXPECT_ERROR(tr, scl_ml_logistic_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_logistic_free(m);
}
static void test_logistic_serialization(scl_test_runner_t *tr) {
    scl_test_group("logistic_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_logistic_params_t params = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    params.max_iter = 500;
    params.random_seed = 42;
    scl_ml_logistic_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_logistic_fit(model, &ds));

    SCL_ML_FLOAT coef_orig = scl_ml_logistic_get_coef(model)[0];
    SCL_ML_FLOAT int_orig = scl_ml_logistic_get_intercept(model);

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_logistic_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_logistic_free(model);

    scl_ml_logistic_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_ML_NEAR(tr, scl_ml_logistic_get_coef(loaded)[0], coef_orig, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_logistic_get_intercept(loaded), int_orig, 1e-5f);

    scl_ml_logistic_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_logistic_fit_predict(&tr);
    test_logistic_predict_proba(&tr);
    test_logistic_getters(&tr);
    test_logistic_errors(&tr);
    test_logistic_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
