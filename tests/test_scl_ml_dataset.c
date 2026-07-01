/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml.h"
#include "scl_alloc_arena.h"
#include <string.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── Dataset Tests ── */

static void test_dataset_init(scl_test_runner_t *tr) {
    scl_test_group("dataset_init");
    scl_ml_dataset_t ds;
    memset(&ds, 0, sizeof(ds));
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 3));
    SCL_EXPECT_EQ_SZ(tr, ds.n_rows, 10);
    SCL_EXPECT_EQ_SZ(tr, ds.n_cols, 3);
    SCL_EXPECT_NOT_NULL(tr, ds.data);
    SCL_EXPECT_NOT_NULL(tr, ds.targets);
    SCL_EXPECT_TRUE(tr, ds.owns_data);
    SCL_EXPECT_TRUE(tr, ds.owns_targets);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_dataset_wrap(scl_test_runner_t *tr) {
    scl_test_group("dataset_wrap");
    SCL_ML_FLOAT data[6] = {1,2,3,4,5,6};
    SCL_ML_FLOAT tgt[2] = {0,1};
    scl_ml_dataset_t ds;
    memset(&ds, 0, sizeof(ds));
    SCL_EXPECT_OK(tr, scl_ml_dataset_wrap(&ds, data, tgt, 2, 3));
    SCL_EXPECT_EQ_SZ(tr, ds.n_rows, 2);
    SCL_EXPECT_EQ_SZ(tr, ds.n_cols, 3);
    SCL_EXPECT_EQ_PTR(tr, ds.data, data);
    SCL_EXPECT_EQ_PTR(tr, ds.targets, tgt);
    SCL_EXPECT_FALSE(tr, ds.owns_data);
    SCL_EXPECT_FALSE(tr, ds.owns_targets);
    scl_ml_dataset_destroy(&ds, scl_allocator_default());
}
static void test_dataset_prepare(scl_test_runner_t *tr) {
    scl_test_group("dataset_prepare");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 5, 2));
    for (size_t i = 0; i < 5; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i;
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = (SCL_ML_FLOAT)(i + 1);
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));
    SCL_EXPECT_NOT_NULL(tr, ds.data_col);
    scl_ml_dataset_destroy(&ds, a);
    scl_alloc_arena_destroy(a);
}
static void test_dataset_errors(scl_test_runner_t *tr) {
    scl_test_group("dataset_errors");
    scl_allocator_t *a = scl_alloc_arena_create(scl_allocator_default(), 1 << 20, 0);
    scl_ml_dataset_t ds;

    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(NULL, a, 1, 1), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(&ds, NULL, 1, 1), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(&ds, a, 0, 1), SCL_ERR_INVALID_ARG);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(&ds, a, 1, 0), SCL_ERR_INVALID_ARG);

    SCL_EXPECT_ERROR(tr, scl_ml_dataset_wrap(NULL, NULL, NULL, 0, 0), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_wrap(&ds, NULL, NULL, 1, 1), SCL_ERR_INVALID_ARG);

    scl_alloc_arena_destroy(a);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    test_dataset_init(&tr);
    test_dataset_wrap(&tr);
    test_dataset_prepare(&tr);
    test_dataset_errors(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
