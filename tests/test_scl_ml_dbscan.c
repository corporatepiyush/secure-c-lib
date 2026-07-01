/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "cluster/scl_ml_dbscan.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── DBSCAN Tests ── */

static void test_dbscan_fit(scl_test_runner_t *tr) {
    scl_test_group("dbscan_fit");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    /* Two dense clusters + 1 noise point */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 9, 2));
    /* Cluster 0: points at (0,0) */
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride + 0] = 0.0f;
        ds.data[i * ds.row_stride + 1] = 0.0f;
        ds.targets[i] = 0.0f;
    }
    /* Cluster 1: points at (10,10) */
    for (size_t i = 4; i < 8; i++) {
        ds.data[i * ds.row_stride + 0] = 10.0f;
        ds.data[i * ds.row_stride + 1] = 10.0f;
        ds.targets[i] = 0.0f;
    }
    /* Noise at (100,100) */
    ds.data[8 * ds.row_stride + 0] = 100.0f;
    ds.data[8 * ds.row_stride + 1] = 100.0f;
    ds.targets[8] = 0.0f;
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_dbscan_params_t params = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    params.eps = 5.0f;
    params.min_pts = 3;
    params.alloc = a;
    scl_ml_dbscan_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_dbscan_fit(model, &ds));
    SCL_EXPECT_TRUE(tr, scl_ml_dbscan_get_n_clusters(model) >= 2);

    const int *labels = scl_ml_dbscan_get_labels(model);
    SCL_EXPECT_NOT_NULL(tr, labels);

    /* First 4 should have same label, next 4 the other, noise != both */
    SCL_EXPECT_TRUE(tr, labels[0] >= 0);
    for (size_t i = 0; i < 4; i++) SCL_EXPECT_EQ_SZ(tr, (size_t)labels[i], (size_t)labels[0]);
    for (size_t i = 4; i < 8; i++) SCL_EXPECT_EQ_SZ(tr, (size_t)labels[i], (size_t)labels[4]);
    SCL_EXPECT_TRUE(tr, labels[0] != labels[4]);
    /* Noise point should be -1 */
    SCL_EXPECT_EQ_SZ(tr, (size_t)labels[8], (size_t)-1);

    scl_ml_dbscan_free(model);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_dbscan_predict(scl_test_runner_t *tr) {
    scl_test_group("dbscan_predict");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 0; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_dbscan_t *model = NULL;
    scl_ml_dbscan_params_t params = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    params.eps = 3.0f;
    params.min_pts = 2;
    params.alloc = a;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_dbscan_fit(model, &ds));

    int out[6];
    SCL_EXPECT_OK(tr, scl_ml_dbscan_predict(model, &ds, out));
    SCL_EXPECT_EQ_SZ(tr, (size_t)out[0], (size_t)out[1]);
    SCL_EXPECT_EQ_SZ(tr, (size_t)out[3], (size_t)out[4]);
    SCL_EXPECT_TRUE(tr, out[0] != out[3]);

    scl_ml_dbscan_free(model);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_dbscan_errors(scl_test_runner_t *tr) {
    scl_test_group("dbscan_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dbscan_params_t dep = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    dep.alloc = a;
    SCL_EXPECT_ERROR(tr, scl_ml_dbscan_new(NULL, dep), SCL_ERR_NULL_PTR);
    scl_ml_dbscan_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_new(&m, dep));
    SCL_EXPECT_ERROR(tr, scl_ml_dbscan_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_dbscan_free(m);
    scl_alloc_arena_destroy(a);
}
static void test_dbscan_serialization(scl_test_runner_t *tr) {
    scl_test_group("dbscan_serialization");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 0; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_dbscan_t *model = NULL;
    scl_ml_dbscan_params_t params = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    params.eps = 3.0f; params.min_pts = 2;
    params.alloc = a;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_dbscan_fit(model, &ds));

    int labels_orig[6];
    SCL_EXPECT_OK(tr, scl_ml_dbscan_predict(model, &ds, labels_orig));
    size_t nc = scl_ml_dbscan_get_n_clusters(model);

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_dbscan_free(model);

    scl_ml_dbscan_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_dbscan_get_n_clusters(loaded), nc);

    int labels_load[6];
    SCL_EXPECT_OK(tr, scl_ml_dbscan_predict(loaded, &ds, labels_load));
    for (size_t i = 0; i < 6; i++) SCL_EXPECT_EQ_SZ(tr, (size_t)labels_load[i], (size_t)labels_orig[i]);

    scl_ml_dbscan_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_dbscan_fit(&tr);
    test_dbscan_predict(&tr);
    test_dbscan_errors(&tr);
    test_dbscan_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
