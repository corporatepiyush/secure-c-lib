/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "cluster/scl_ml_kmeans.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── K-Means Tests ── */

static void test_kmeans_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("kmeans_fit_predict");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    /* Two well-separated clusters in 2D */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 2));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride + 0] = 0.0f;
        ds.data[i * ds.row_stride + 1] = 0.0f;
        ds.targets[i] = 0.0f;
    }
    for (size_t i = 10; i < 20; i++) {
        ds.data[i * ds.row_stride + 0] = 100.0f;
        ds.data[i * ds.row_stride + 1] = 100.0f;
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_kmeans_params_t params = SCL_ML_KMEANS_PARAMS_DEFAULT();
    params.n_clusters = 2;
    params.n_init = 3;
    params.random_seed = 42;
    scl_ml_kmeans_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_kmeans_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_kmeans_get_n_clusters(model), 2);

    SCL_EXPECT_TRUE(tr, scl_ml_kmeans_get_inertia(model) >= 0.0f);

    int labels[20];
    SCL_EXPECT_OK(tr, scl_ml_kmeans_predict(model, &ds, labels));
    int l0 = labels[0], l1 = labels[10];
    SCL_EXPECT_TRUE(tr, l0 != l1);
    for (size_t i = 0; i < 10; i++) SCL_EXPECT_EQ_SZ(tr, (size_t)labels[i], (size_t)l0);
    for (size_t i = 10; i < 20; i++) SCL_EXPECT_EQ_SZ(tr, (size_t)labels[i], (size_t)l1);

    const int *stored = scl_ml_kmeans_get_labels(model);
    SCL_EXPECT_NOT_NULL(tr, stored);

    scl_ml_kmeans_free(model);
    scl_ml_dataset_destroy(&ds, a);
}
static void test_kmeans_errors(scl_test_runner_t *tr) {
    scl_test_group("kmeans_errors");
    scl_ml_kmeans_params_t mep = SCL_ML_KMEANS_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_kmeans_new(NULL, mep), SCL_ERR_NULL_PTR);
    scl_ml_kmeans_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_new(&m, mep));
    SCL_EXPECT_ERROR(tr, scl_ml_kmeans_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_kmeans_free(m);
}
static void test_kmeans_serialization(scl_test_runner_t *tr) {
    scl_test_group("kmeans_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 100.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_kmeans_t *model = NULL;
    scl_ml_kmeans_params_t params = SCL_ML_KMEANS_PARAMS_DEFAULT();
    params.n_clusters = 2;
    params.n_init = 1;
    params.random_seed = 42;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_kmeans_fit(model, &ds));

    int labels_orig[10];
    SCL_EXPECT_OK(tr, scl_ml_kmeans_predict(model, &ds, labels_orig));
    SCL_ML_FLOAT inertia = scl_ml_kmeans_get_inertia(model);

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_kmeans_free(model);

    scl_ml_kmeans_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_kmeans_get_n_clusters(loaded), 2);

    int labels_load[10];
    SCL_EXPECT_OK(tr, scl_ml_kmeans_predict(loaded, &ds, labels_load));
    for (size_t i = 0; i < 10; i++) SCL_EXPECT_EQ_SZ(tr, (size_t)labels_load[i], (size_t)labels_orig[i]);

    SCL_ML_NEAR(tr, scl_ml_kmeans_get_inertia(loaded), inertia, 1e-4f);

    scl_ml_kmeans_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ── Edge Cases ───────────────────────────────────────────────*/
static void test_kmeans_k_equals_n(scl_test_runner_t *tr) {
    scl_test_group("kmeans_k_equals_n");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 5, 2));
    for (size_t i = 0; i < 5; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i * 10);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 10);
        ds.targets[i] = 0.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_kmeans_params_t params = SCL_ML_KMEANS_PARAMS_DEFAULT();
    params.n_clusters = 5; /* k == n */
    params.n_init = 2;
    params.random_seed = 42;
    scl_ml_kmeans_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_kmeans_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_kmeans_get_n_clusters(model), 5);
    SCL_EXPECT_TRUE(tr, scl_ml_kmeans_get_inertia(model) >= 0.0f);
    scl_ml_kmeans_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_kmeans_single_point(scl_test_runner_t *tr) {
    scl_test_group("kmeans_single_point");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 1, 2));
    ds.data[0] = 5.0f; ds.data[1] = 7.0f; ds.targets[0] = 0.0f;
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_kmeans_params_t params = SCL_ML_KMEANS_PARAMS_DEFAULT();
    params.n_clusters = 1;
    params.random_seed = 42;
    scl_ml_kmeans_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_kmeans_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_kmeans_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_kmeans_get_n_clusters(model), 1);
    int labels[1];
    SCL_EXPECT_OK(tr, scl_ml_kmeans_predict(model, &ds, labels));
    SCL_EXPECT_EQ_SZ(tr, (size_t)labels[0], 0);
    scl_ml_kmeans_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_kmeans_fit_predict(&tr);
    test_kmeans_errors(&tr);
    test_kmeans_serialization(&tr);
    test_kmeans_k_equals_n(&tr);
    test_kmeans_single_point(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
