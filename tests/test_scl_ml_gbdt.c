/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "tree/scl_ml_gbdt.h"
#include "preprocessing/scl_ml_metrics.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── GBDT Tests ── */

static void test_gbdt_regression(scl_test_runner_t *tr) {
    scl_test_group("gbdt_regression");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 1));
    for (size_t i = 0; i < 20; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)i;
        ds.targets[i] = 2.0f * (SCL_ML_FLOAT)i + 3.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gbdt_params_t params = SCL_ML_GBDT_PARAMS_DEFAULT();
    params.n_estimators = 20;
    params.learning_rate = 0.5f;
    params.max_depth = 3;
    params.random_seed = 42;
    scl_ml_gbdt_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_gbdt_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_gbdt_get_n_features(model), 1);

    SCL_ML_FLOAT pred[20];
    SCL_EXPECT_OK(tr, scl_ml_gbdt_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 20);
    SCL_EXPECT_TRUE(tr, r2 > 0.90f);

    scl_ml_gbdt_free(model);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_gbdt_errors(scl_test_runner_t *tr) {
    scl_test_group("gbdt_errors");
    scl_ml_gbdt_params_t gep = SCL_ML_GBDT_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_gbdt_new(NULL, gep), SCL_ERR_NULL_PTR);
    scl_ml_gbdt_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_new(&m, gep));
    SCL_EXPECT_ERROR(tr, scl_ml_gbdt_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_gbdt_free(m);
}
static void test_gbdt_serialization(scl_test_runner_t *tr) {
    scl_test_group("gbdt_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)i;
        ds.targets[i] = (SCL_ML_FLOAT)(2 * i);
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gbdt_t *model = NULL;
    scl_ml_gbdt_params_t params = SCL_ML_GBDT_PARAMS_DEFAULT();
    params.n_estimators = 10;
    params.max_depth = 2;
    params.random_seed = 42;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_gbdt_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[10];
    SCL_EXPECT_OK(tr, scl_ml_gbdt_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_gbdt_free(model);

    scl_ml_gbdt_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);

    SCL_ML_FLOAT pred_load[10];
    SCL_EXPECT_OK(tr, scl_ml_gbdt_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 10; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-4f);

    scl_ml_gbdt_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_gbdt_regression(&tr);
    test_gbdt_errors(&tr);
    test_gbdt_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
