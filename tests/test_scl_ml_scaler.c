/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "preprocessing/scl_ml_scaler.h"
#include "scl_alloc_arena.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── Scaler Tests ── */

static void test_standard_scaler(scl_test_runner_t *tr) {
    scl_test_group("standard_scaler");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 2));
    /* data: col0={1,2,3,4}, col1={2,4,6,8} */
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i + 1);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)((i + 1) * 2);
        ds.targets[i] = 0.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_standard_scaler_t *scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&scaler, a));
    SCL_EXPECT_NOT_NULL(tr, scaler);

    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_fit(scaler, &ds));
    SCL_EXPECT_TRUE(tr, scaler->fitted);

    SCL_ML_NEAR(tr, scaler->mean_[0], 2.5f, 1e-5f);
    SCL_ML_NEAR(tr, scaler->mean_[1], 5.0f, 1e-5f);

    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_transform(scaler, &ds));
    SCL_ML_NEAR(tr, ds.data[0], -1.5f / sqrtf(1.25f), 1e-4f);
    SCL_ML_NEAR(tr, ds.data[3*ds.row_stride+0], 1.5f / sqrtf(1.25f), 1e-4f);

    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_inverse(scaler, &ds));
    SCL_ML_NEAR(tr, ds.data[0], 1.0f, 1e-4f);
    SCL_ML_NEAR(tr, ds.data[3*ds.row_stride+0], 4.0f, 1e-4f);

    scl_ml_standard_scaler_free(scaler);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_minmax_scaler(scl_test_runner_t *tr) {
    scl_test_group("minmax_scaler");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 1));
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = 0.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_minmax_scaler_t *scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_new(&scaler, a));
    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_fit(scaler, &ds));
    SCL_EXPECT_TRUE(tr, scaler->fitted);
    SCL_ML_NEAR(tr, scaler->min_[0], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, scaler->scale_[0], 1.0f / 6.0f, 1e-5f);

    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_transform(scaler, &ds));
    SCL_ML_NEAR(tr, ds.data[0], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, ds.data[ds.row_stride], 2.0f/6.0f, 1e-4f);
    SCL_ML_NEAR(tr, ds.data[3*ds.row_stride], 1.0f, 1e-5f);

    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_inverse(scaler, &ds));
    SCL_ML_NEAR(tr, ds.data[0], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, ds.data[3*ds.row_stride], 6.0f, 1e-4f);

    scl_ml_minmax_scaler_free(scaler);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_scaler_errors(scl_test_runner_t *tr) {
    scl_test_group("scaler_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_standard_scaler_t *ss = NULL;
    scl_ml_minmax_scaler_t *ms = NULL;

    SCL_EXPECT_ERROR(tr, scl_ml_standard_scaler_new(NULL, a), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_minmax_scaler_new(NULL, a), SCL_ERR_NULL_PTR);

    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&ss, a));
    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_new(&ms, a));

    SCL_EXPECT_ERROR(tr, scl_ml_standard_scaler_fit(ss, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_minmax_scaler_fit(ms, NULL), SCL_ERR_NULL_PTR);

    scl_ml_standard_scaler_free(ss);
    scl_ml_minmax_scaler_free(ms);
    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_standard_scaler(&tr);
    test_minmax_scaler(&tr);
    test_scaler_errors(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
