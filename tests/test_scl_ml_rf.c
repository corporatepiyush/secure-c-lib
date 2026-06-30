/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "tree/scl_ml_rf.h"
#include "preprocessing/scl_ml_metrics.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── Random Forest Tests ── */

static void test_rf_classification(scl_test_runner_t *tr) {
    scl_test_group("rf_classification");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 12, 2));
    for (size_t i = 0; i < 6; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i * 0.5f;
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)i * 0.5f;
        ds.targets[i] = 0.0f;
    }
    for (size_t i = 6; i < 12; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(10.0f + i);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(10.0f + i);
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_rf_params_t params = SCL_ML_RF_PARAMS_DEFAULT();
    params.n_estimators = 10;
    params.max_depth = 4;
    params.random_seed = 42;
    scl_ml_rf_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_rf_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_rf_get_n_features(model), 2);

    SCL_ML_FLOAT pred[12];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 12);
    SCL_EXPECT_TRUE(tr, acc > 0.90f);

    scl_ml_rf_free(model);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_rf_regression(scl_test_runner_t *tr) {
    scl_test_group("rf_regression");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)i;
        ds.targets[i] = 3.0f * (SCL_ML_FLOAT)i - 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_rf_params_t params = SCL_ML_RF_PARAMS_DEFAULT();
    params.n_estimators = 10;
    params.max_depth = 4;
    params.criterion = SCL_ML_CRITERION_MSE;
    params.random_seed = 42;
    scl_ml_rf_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_rf_fit(model, &ds));

    SCL_ML_FLOAT pred[10];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 10);
    SCL_EXPECT_TRUE(tr, r2 > 0.80f);

    scl_ml_rf_free(model);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_rf_errors(scl_test_runner_t *tr) {
    scl_test_group("rf_errors");
    scl_ml_rf_params_t rep = SCL_ML_RF_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_rf_new(NULL, rep), SCL_ERR_NULL_PTR);
    scl_ml_rf_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&m, rep));
    SCL_EXPECT_ERROR(tr, scl_ml_rf_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_rf_free(m);
}
static void test_rf_serialization(scl_test_runner_t *tr) {
    scl_test_group("rf_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_rf_t *model = NULL;
    scl_ml_rf_params_t params = SCL_ML_RF_PARAMS_DEFAULT();
    params.n_estimators = 5;
    params.random_seed = 42;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_rf_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[10];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_rf_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_rf_free(model);

    scl_ml_rf_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);

    SCL_ML_FLOAT pred_load[10];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 10; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_rf_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}


/* ── Edge Cases ───────────────────────────────────────────────*/
static void test_rf_parallel_determinism(scl_test_runner_t *tr) {
    scl_test_group("rf_parallel_determinism");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 12, 2));
    for (size_t i = 0; i < 6; i++) { ds.data[i*ds.row_stride+0]=(SCL_ML_FLOAT)i; ds.data[i*ds.row_stride+1]=(SCL_ML_FLOAT)i; ds.targets[i]=0.0f; }
    for (size_t i = 6; i < 12; i++) { ds.data[i*ds.row_stride+0]=(SCL_ML_FLOAT)(50+i); ds.data[i*ds.row_stride+1]=(SCL_ML_FLOAT)(50+i); ds.targets[i]=1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    /* Sequential */
    scl_ml_rf_params_t params = SCL_ML_RF_PARAMS_DEFAULT();
    params.n_estimators = 20; params.n_threads = 1; params.random_seed = 99;
    scl_ml_rf_t *seq = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&seq, params));
    SCL_EXPECT_OK(tr, scl_ml_rf_fit(seq, &ds));
    SCL_ML_FLOAT pred_seq[12];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(seq, &ds, pred_seq));

    /* Parallel (4 threads) — same seed, MUST produce same predictions */
    params.n_threads = 4;
    scl_ml_rf_t *par = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&par, params));
    SCL_EXPECT_OK(tr, scl_ml_rf_fit(par, &ds));
    SCL_ML_FLOAT pred_par[12];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(par, &ds, pred_par));

    for (size_t i = 0; i < 12; i++)
        SCL_EXPECT_TRUE(tr, pred_seq[i] == pred_par[i]);

    scl_ml_rf_free(seq); scl_ml_rf_free(par);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_rf_mae_criterion(scl_test_runner_t *tr) {
    scl_test_group("rf_mae_criterion");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 10; i++) { ds.data[i*ds.row_stride]=(SCL_ML_FLOAT)i; ds.targets[i]=(SCL_ML_FLOAT)(i*2); }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));
    scl_ml_rf_params_t params = SCL_ML_RF_PARAMS_DEFAULT();
    params.criterion = SCL_ML_CRITERION_MAE;
    params.n_estimators = 10; params.random_seed = 42;
    scl_ml_rf_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_rf_new(&m, params));
    SCL_EXPECT_OK(tr, scl_ml_rf_fit(m, &ds));
    SCL_ML_FLOAT pred[10];
    SCL_EXPECT_OK(tr, scl_ml_rf_predict(m, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 10);
    SCL_EXPECT_TRUE(tr, r2 > 0.90f);
    scl_ml_rf_free(m);
    scl_ml_dataset_destroy(&ds, a);
}


/* ── Edge Cases ───────────────────────────────────────────────*/


int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_rf_classification(&tr);
    test_rf_regression(&tr);
    test_rf_errors(&tr);
    test_rf_serialization(&tr);
    test_rf_parallel_determinism(&tr);
    test_rf_mae_criterion(&tr);
    test_rf_parallel_determinism(&tr);
    test_rf_mae_criterion(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
