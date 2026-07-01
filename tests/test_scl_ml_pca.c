/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "decomposition/scl_ml_pca.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── PCA Tests ── */

static void test_pca_fit_transform(scl_test_runner_t *tr) {
    scl_test_group("pca_fit_transform");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    /* 4 points in 3D, actually live on a 2D plane (z = x+y) */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 3));
    ds.data[0] = 1; ds.data[1] = 0; ds.data[2] = 1;
    ds.data[ds.row_stride+0] = 0; ds.data[ds.row_stride+1] = 1; ds.data[ds.row_stride+2] = 1;
    ds.data[2*ds.row_stride+0] = 2; ds.data[2*ds.row_stride+1] = 1; ds.data[2*ds.row_stride+2] = 3;
    ds.data[3*ds.row_stride+0] = -1; ds.data[3*ds.row_stride+1] = 0; ds.data[3*ds.row_stride+2] = -1;
    for (size_t i = 0; i < 4; i++) ds.targets[i] = 0;
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_pca_params_t params = SCL_ML_PCA_PARAMS_DEFAULT();
    params.n_components = 2;
    params.alloc = a;
    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, params));
    SCL_EXPECT_NOT_NULL(tr, pca);

    SCL_EXPECT_OK(tr, scl_ml_pca_fit(pca, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_pca_get_n_components(pca), 2);

    /* Transform */
    SCL_ML_FLOAT proj[8];
    SCL_EXPECT_OK(tr, scl_ml_pca_transform(pca, &ds, proj));

    /* Inverse transform should reconstruct approximately */
    SCL_ML_FLOAT recon[12];
    SCL_EXPECT_OK(tr, scl_ml_pca_inverse_transform(pca, proj, 4, recon));
    for (size_t i = 0; i < 4; i++) {
        for (size_t j = 0; j < 3; j++) {
            float orig = ds.data[i * ds.row_stride + j];
            float rec  = recon[i * 3 + j];
            SCL_ML_NEAR(tr, rec, orig, 1e-4f);
        }
    }

    scl_ml_pca_free(pca);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_pca_auto_components(scl_test_runner_t *tr) {
    scl_test_group("pca_auto_components");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 5, 2));
    for (size_t i = 0; i < 5; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i;
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_pca_params_t params = SCL_ML_PCA_PARAMS_DEFAULT();
    params.n_components = 0; /* auto */
    params.alloc = a;
    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, params));
    SCL_EXPECT_OK(tr, scl_ml_pca_fit(pca, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_pca_get_n_components(pca), 2);

    SCL_ML_FLOAT proj[10];
    SCL_EXPECT_OK(tr, scl_ml_pca_fit_transform(pca, &ds, proj));

    scl_ml_pca_free(pca);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_pca_getters(scl_test_runner_t *tr) {
    scl_test_group("pca_getters");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 2));
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(3 - i);
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_pca_params_t params = SCL_ML_PCA_PARAMS_DEFAULT();
    params.alloc = a;
    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, params));
    SCL_EXPECT_OK(tr, scl_ml_pca_fit(pca, &ds));

    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_components(pca));
    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_mean(pca));
    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_explained_variance(pca));
    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_explained_variance_ratio(pca));

    scl_ml_pca_free(pca);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_pca_errors(scl_test_runner_t *tr) {
    scl_test_group("pca_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_pca_params_t ep = SCL_ML_PCA_PARAMS_DEFAULT();
    ep.alloc = a;
    SCL_EXPECT_ERROR(tr, scl_ml_pca_new(NULL, ep), SCL_ERR_NULL_PTR);

    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, ep));

    SCL_EXPECT_ERROR(tr, scl_ml_pca_fit(pca, NULL), SCL_ERR_NULL_PTR);

    scl_ml_pca_free(pca);
    scl_alloc_arena_destroy(a);
}
static void test_pca_serialization(scl_test_runner_t *tr) {
    scl_test_group("pca_serialization");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 2));
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_pca_params_t params = SCL_ML_PCA_PARAMS_DEFAULT();
    params.alloc = a;
    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, params));
    SCL_EXPECT_OK(tr, scl_ml_pca_fit(pca, &ds));

    uint8_t *buf = NULL;
    size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_pca_save(pca, &buf, &len, a));
    SCL_EXPECT_NOT_NULL(tr, buf);
    SCL_EXPECT_TRUE(tr, len > 0);

    SCL_ML_FLOAT c0 = scl_ml_pca_get_components(pca)[0];
    SCL_ML_FLOAT c1 = scl_ml_pca_get_components(pca)[1];
    SCL_ML_FLOAT m0 = scl_ml_pca_get_mean(pca)[0];
    SCL_ML_FLOAT ev = scl_ml_pca_get_explained_variance(pca)[0];
    SCL_EXPECT_EQ_SZ(tr, scl_ml_pca_get_n_components(pca), 2);
    scl_ml_pca_free(pca);

    scl_ml_pca_params_t params2 = SCL_ML_PCA_PARAMS_DEFAULT();
    params2.alloc = a;
    scl_ml_pca_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_load(&loaded, buf, len, params2));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_pca_get_n_components(loaded), 2);
    SCL_ML_NEAR(tr, scl_ml_pca_get_components(loaded)[0], c0, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_pca_get_components(loaded)[1], c1, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_pca_get_mean(loaded)[0], m0, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_pca_get_explained_variance(loaded)[0], ev, 1e-5f);

    scl_ml_pca_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_pca_fit_transform(&tr);
    test_pca_auto_components(&tr);
    test_pca_getters(&tr);
    test_pca_errors(&tr);
    test_pca_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
