/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "cluster/scl_ml_gmm.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── GMM Tests ── */

static void test_gmm_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("gmm_fit_predict");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);

    /* Two well-separated clusters in 2D */
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 2));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride + 0] = 0.0f;
        ds.data[i * ds.row_stride + 1] = 0.0f;
        ds.targets[i] = 0;
    }
    for (size_t i = 10; i < 20; i++) {
        ds.data[i * ds.row_stride + 0] = 10.0f;
        ds.data[i * ds.row_stride + 1] = 10.0f;
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gmm_params_t params = SCL_ML_GMM_PARAMS_DEFAULT();
    params.n_components = 2;
    params.random_seed = 42;
    params.alloc = a;

    scl_ml_gmm_t *gmm = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gmm_new(&gmm, params));
    SCL_EXPECT_NOT_NULL(tr, gmm);

    SCL_EXPECT_OK(tr, scl_ml_gmm_fit(gmm, &ds));

    size_t labels[20];
    SCL_EXPECT_OK(tr, scl_ml_gmm_predict(gmm, &ds, labels));

    /* First 10 points should all have the same label, last 10 the other */
    size_t lbl0 = labels[0];
    for (size_t i = 0; i < 10; i++)
        SCL_EXPECT_EQ_SZ(tr, labels[i], lbl0);
    size_t lbl1 = labels[10];
    for (size_t i = 10; i < 20; i++)
        SCL_EXPECT_EQ_SZ(tr, labels[i], lbl1);
    SCL_EXPECT_TRUE(tr, lbl0 != lbl1);

    scl_ml_gmm_free(gmm);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_gmm_predict_proba(scl_test_runner_t *tr) {
    scl_test_group("gmm_predict_proba");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);

    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 6; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gmm_params_t params = SCL_ML_GMM_PARAMS_DEFAULT();
    params.n_components = 2;
    params.random_seed = 7;
    params.alloc = a;

    scl_ml_gmm_t *gmm = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gmm_new(&gmm, params));
    SCL_EXPECT_OK(tr, scl_ml_gmm_fit(gmm, &ds));

    SCL_ML_FLOAT proba[12];
    SCL_EXPECT_OK(tr, scl_ml_gmm_predict_proba(gmm, &ds, proba));
    for (size_t i = 0; i < 6; i++) {
        float sum = proba[i * 2] + proba[i * 2 + 1];
        SCL_ML_NEAR(tr, sum, 1.0f, 1e-4f);
    }

    scl_ml_gmm_free(gmm);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_gmm_errors(scl_test_runner_t *tr) {
    scl_test_group("gmm_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_gmm_params_t ep = SCL_ML_GMM_PARAMS_DEFAULT();
    ep.alloc = a;

    SCL_EXPECT_ERROR(tr, scl_ml_gmm_new(NULL, ep), SCL_ERR_NULL_PTR);

    scl_ml_gmm_t *gmm = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gmm_new(&gmm, ep));

    SCL_EXPECT_ERROR(tr, scl_ml_gmm_fit(gmm, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_gmm_predict(gmm, NULL, NULL), SCL_ERR_NULL_PTR);

    scl_ml_gmm_free(gmm);
    scl_alloc_arena_destroy(a);
}
static void test_gmm_serialization(scl_test_runner_t *tr) {
    scl_test_group("gmm_serialization");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);

    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) ds.data[i * ds.row_stride] = 0.0f;
    for (size_t i = 5; i < 10; i++) ds.data[i * ds.row_stride] = 5.0f;
    for (size_t i = 0; i < 10; i++) ds.targets[i] = 0;
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gmm_params_t params = SCL_ML_GMM_PARAMS_DEFAULT();
    params.n_components = 2;
    params.random_seed = 42;
    params.alloc = a;

    scl_ml_gmm_t *gmm = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gmm_new(&gmm, params));
    SCL_EXPECT_OK(tr, scl_ml_gmm_fit(gmm, &ds));

    uint8_t *buf = NULL;
    size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_gmm_save(gmm, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);

    size_t orig_labels[10];
    SCL_EXPECT_OK(tr, scl_ml_gmm_predict(gmm, &ds, orig_labels));
    scl_ml_gmm_free(gmm);

    scl_ml_gmm_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gmm_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);

    size_t loaded_labels[10];
    SCL_EXPECT_OK(tr, scl_ml_gmm_predict(loaded, &ds, loaded_labels));
    for (size_t i = 0; i < 10; i++)
        SCL_EXPECT_EQ_SZ(tr, loaded_labels[i], orig_labels[i]);

    scl_ml_gmm_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_gmm_fit_predict(&tr);
    test_gmm_predict_proba(&tr);
    test_gmm_errors(&tr);
    test_gmm_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
