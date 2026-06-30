/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "tree/scl_ml_tree.h"
#include "preprocessing/scl_ml_metrics.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── Decision Tree Tests ── */

static void test_tree_classification(scl_test_runner_t *tr) {
    scl_test_group("tree_classification");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 12, 2));
    /* Class 0: bottom-left, Class 1: top-right */
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

    scl_ml_tree_params_t params = SCL_ML_TREE_PARAMS_DEFAULT();
    params.max_depth = 5;
    params.random_seed = 42;
    scl_ml_tree_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_tree_fit(model, &ds));
    SCL_EXPECT_TRUE(tr, scl_ml_tree_get_n_nodes(model) > 0);
    SCL_EXPECT_TRUE(tr, scl_ml_tree_get_n_leaves(model) > 0);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_tree_get_n_features(model), 2);

    SCL_ML_FLOAT pred[12];
    SCL_EXPECT_OK(tr, scl_ml_tree_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 12);
    SCL_EXPECT_TRUE(tr, acc > 0.99f);

    scl_ml_tree_free(model);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_tree_regression(scl_test_runner_t *tr) {
    scl_test_group("tree_regression");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)i;
        ds.targets[i] = 2.0f * (SCL_ML_FLOAT)i + 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_tree_params_t params = SCL_ML_TREE_PARAMS_DEFAULT();
    params.criterion = SCL_ML_CRITERION_MSE;
    params.max_depth = 4;
    scl_ml_tree_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_tree_fit(model, &ds));

    SCL_ML_FLOAT pred[10];
    SCL_EXPECT_OK(tr, scl_ml_tree_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 10);
    SCL_EXPECT_TRUE(tr, r2 > 0.90f);

    scl_ml_tree_free(model);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_tree_errors(scl_test_runner_t *tr) {
    scl_test_group("tree_errors");
    scl_ml_tree_params_t tep = SCL_ML_TREE_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_tree_new(NULL, tep), SCL_ERR_NULL_PTR);
    scl_ml_tree_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_new(&m, tep));
    SCL_EXPECT_ERROR(tr, scl_ml_tree_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_tree_free(m);
}
static void test_tree_serialization(scl_test_runner_t *tr) {
    scl_test_group("tree_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_tree_params_t tp = SCL_ML_TREE_PARAMS_DEFAULT();
    scl_ml_tree_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_new(&model, tp));
    SCL_EXPECT_OK(tr, scl_ml_tree_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[10];
    SCL_EXPECT_OK(tr, scl_ml_tree_predict(model, &ds, pred_orig));
    size_t nn = scl_ml_tree_get_n_nodes(model);

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_tree_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_tree_free(model);

    scl_ml_tree_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_load(&loaded, buf, len, tp));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_tree_get_n_nodes(loaded), nn);

    SCL_ML_FLOAT pred_load[10];
    SCL_EXPECT_OK(tr, scl_ml_tree_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 10; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_tree_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ── Edge Cases ───────────────────────────────────────────────*/
static void test_tree_entropy_criterion(scl_test_runner_t *tr) {
    scl_test_group("tree_entropy");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 12, 2));
    for (size_t i = 0; i < 6; i++) { ds.data[i*ds.row_stride+0]=(SCL_ML_FLOAT)i; ds.data[i*ds.row_stride+1]=(SCL_ML_FLOAT)i; ds.targets[i]=0.0f; }
    for (size_t i = 6; i < 12; i++) { ds.data[i*ds.row_stride+0]=(SCL_ML_FLOAT)(50+i); ds.data[i*ds.row_stride+1]=(SCL_ML_FLOAT)(50+i); ds.targets[i]=1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));
    scl_ml_tree_params_t params = SCL_ML_TREE_PARAMS_DEFAULT();
    params.criterion = SCL_ML_CRITERION_ENTROPY;
    params.max_depth = 5; params.random_seed = 42;
    scl_ml_tree_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_new(&m, params));
    SCL_EXPECT_OK(tr, scl_ml_tree_fit(m, &ds));
    SCL_EXPECT_TRUE(tr, scl_ml_tree_get_n_nodes(m) > 0);
    SCL_ML_FLOAT pred[12];
    SCL_EXPECT_OK(tr, scl_ml_tree_predict(m, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 12);
    SCL_EXPECT_TRUE(tr, acc > 0.99f);
    scl_ml_tree_free(m);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_tree_max_depth_zero(scl_test_runner_t *tr) {
    scl_test_group("tree_max_depth_zero");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 2));
    for (size_t i = 0; i < 5; i++) { ds.data[i*ds.row_stride+0]=0.0f; ds.data[i*ds.row_stride+1]=0.0f; ds.targets[i]=0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i*ds.row_stride+0]=10.0f; ds.data[i*ds.row_stride+1]=10.0f; ds.targets[i]=1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));
    scl_ml_tree_params_t params = SCL_ML_TREE_PARAMS_DEFAULT();
    params.max_depth = 0; /* stump — single node should be a leaf */
    scl_ml_tree_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_tree_new(&m, params));
    SCL_EXPECT_OK(tr, scl_ml_tree_fit(m, &ds));
    SCL_EXPECT_TRUE(tr, scl_ml_tree_get_n_nodes(m) > 0);
    SCL_EXPECT_TRUE(tr, scl_ml_tree_get_n_leaves(m) > 0);
    scl_ml_tree_free(m);
    scl_ml_dataset_destroy(&ds, a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_tree_classification(&tr);
    test_tree_regression(&tr);
    test_tree_errors(&tr);
    test_tree_serialization(&tr);
    test_tree_entropy_criterion(&tr);
    test_tree_max_depth_zero(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
