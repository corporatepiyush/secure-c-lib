/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "naive_bayes/scl_ml_nb.h"
#include "preprocessing/scl_ml_metrics.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── Naive Bayes Tests ── */

static void test_nb_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("nb_fit_predict");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    /* 3 classes, 2 features: each class cluster at (c*10, c*10) */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 15, 2));
    for (size_t c = 0; c < 3; c++) {
        for (size_t r = 0; r < 5; r++) {
            size_t i = c * 5 + r;
            ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(c * 10);
            ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(c * 10);
            ds.targets[i] = (SCL_ML_FLOAT)c;
        }
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_nb_params_t params = SCL_ML_NB_PARAMS_DEFAULT();
    params.alloc = a;
    scl_ml_nb_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_nb_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_nb_get_n_classes(model), 3);

    SCL_ML_FLOAT pred[15];
    SCL_EXPECT_OK(tr, scl_ml_nb_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 15);
    SCL_EXPECT_TRUE(tr, acc > 0.99f);

    scl_ml_nb_free(model);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_nb_predict_proba(scl_test_runner_t *tr) {
    scl_test_group("nb_predict_proba");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 2; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 2; i < 4; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    for (size_t i = 4; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 2.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_nb_params_t params = SCL_ML_NB_PARAMS_DEFAULT();
    params.alloc = a;
    scl_ml_nb_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_nb_fit(model, &ds));

    SCL_ML_FLOAT proba[18];
    SCL_EXPECT_OK(tr, scl_ml_nb_predict_proba(model, &ds, proba));
    for (size_t i = 0; i < 6; i++) {
        float sum = proba[i*3] + proba[i*3+1] + proba[i*3+2];
        SCL_ML_NEAR(tr, sum, 1.0f, 1e-4f);
    }

    scl_ml_nb_free(model);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_nb_errors(scl_test_runner_t *tr) {
    scl_test_group("nb_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_nb_params_t nep = SCL_ML_NB_PARAMS_DEFAULT();
    nep.alloc = a;
    SCL_EXPECT_ERROR(tr, scl_ml_nb_new(NULL, nep), SCL_ERR_NULL_PTR);
    scl_ml_nb_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_new(&m, nep));
    SCL_EXPECT_ERROR(tr, scl_ml_nb_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_nb_free(m);
    scl_alloc_arena_destroy(a);
}
static void test_nb_serialization(scl_test_runner_t *tr) {
    scl_test_group("nb_serialization");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_nb_params_t np = SCL_ML_NB_PARAMS_DEFAULT();
    np.alloc = a;
    scl_ml_nb_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_new(&model, np));
    SCL_EXPECT_OK(tr, scl_ml_nb_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[6];
    SCL_EXPECT_OK(tr, scl_ml_nb_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_nb_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_nb_free(model);

    scl_ml_nb_params_t np2 = SCL_ML_NB_PARAMS_DEFAULT();
    np2.alloc = a;
    scl_ml_nb_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_load(&loaded, buf, len, np2));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_nb_get_n_classes(loaded), 2);

    SCL_ML_FLOAT pred_load[6];
    SCL_EXPECT_OK(tr, scl_ml_nb_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 6; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_nb_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_nb_fit_predict(&tr);
    test_nb_predict_proba(&tr);
    test_nb_errors(&tr);
    test_nb_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
