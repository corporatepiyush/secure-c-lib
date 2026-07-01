/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "neighbors/scl_ml_knn.h"
#include "preprocessing/scl_ml_metrics.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── KNN Tests ── */

static void test_knn_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("knn_fit_predict");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 2));
    for (size_t i = 0; i < 5; i++) {
        ds.data[i * ds.row_stride + 0] = 0.0f;
        ds.data[i * ds.row_stride + 1] = 0.0f;
        ds.targets[i] = 0.0f;
    }
    for (size_t i = 5; i < 10; i++) {
        ds.data[i * ds.row_stride + 0] = 10.0f;
        ds.data[i * ds.row_stride + 1] = 10.0f;
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_knn_params_t params = SCL_ML_KNN_PARAMS_DEFAULT();
    params.k = 3;
    params.alloc = a;
    scl_ml_knn_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_knn_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_knn_get_n_samples(model), 10);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_knn_get_n_features(model), 2);

    SCL_ML_FLOAT pred[10];
    SCL_EXPECT_OK(tr, scl_ml_knn_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 10);
    SCL_EXPECT_TRUE(tr, acc > 0.99f);

    scl_ml_knn_free(model);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_knn_distance_weighted(scl_test_runner_t *tr) {
    scl_test_group("knn_distance_weighted");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    ds.data[0] = 0.0f; ds.targets[0] = 0.0f;
    ds.data[ds.row_stride] = 0.0f; ds.targets[1] = 0.0f;
    ds.data[2*ds.row_stride] = 1.0f; ds.targets[2] = 0.0f;
    ds.data[3*ds.row_stride] = 10.0f; ds.targets[3] = 1.0f;
    ds.data[4*ds.row_stride] = 10.0f; ds.targets[4] = 1.0f;
    ds.data[5*ds.row_stride] = 10.0f; ds.targets[5] = 1.0f;
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_knn_params_t params = SCL_ML_KNN_PARAMS_DEFAULT();
    params.k = 5;
    params.weights = 1; /* distance-weighted */
    params.alloc = a;
    scl_ml_knn_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_knn_fit(model, &ds));

    /* Predict on training */
    SCL_ML_FLOAT pred[6];
    SCL_EXPECT_OK(tr, scl_ml_knn_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 6);
    SCL_EXPECT_TRUE(tr, acc > 0.8f);

    scl_ml_knn_free(model);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_knn_errors(scl_test_runner_t *tr) {
    scl_test_group("knn_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_knn_params_t kep = SCL_ML_KNN_PARAMS_DEFAULT();
    kep.alloc = a;
    SCL_EXPECT_ERROR(tr, scl_ml_knn_new(NULL, kep), SCL_ERR_NULL_PTR);
    scl_ml_knn_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_new(&m, kep));
    SCL_EXPECT_ERROR(tr, scl_ml_knn_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_knn_free(m);
    scl_alloc_arena_destroy(a);
}
static void test_knn_serialization(scl_test_runner_t *tr) {
    scl_test_group("knn_serialization");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_knn_params_t kp = SCL_ML_KNN_PARAMS_DEFAULT();
    kp.alloc = a;
    scl_ml_knn_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_new(&model, kp));
    SCL_EXPECT_OK(tr, scl_ml_knn_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[6];
    SCL_EXPECT_OK(tr, scl_ml_knn_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_knn_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_knn_free(model);

    scl_ml_knn_params_t kp2 = SCL_ML_KNN_PARAMS_DEFAULT();
    kp2.alloc = a;
    scl_ml_knn_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_load(&loaded, buf, len, kp2));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_knn_get_n_samples(loaded), 6);

    SCL_ML_FLOAT pred_load[6];
    SCL_EXPECT_OK(tr, scl_ml_knn_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 6; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_knn_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_knn_fit_predict(&tr);
    test_knn_distance_weighted(&tr);
    test_knn_errors(&tr);
    test_knn_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
