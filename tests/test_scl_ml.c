#include "scl_test.h"
#include "scl_ml.h"
#include "scl_ml_simd.h"
#include "linear_model/scl_ml_linear.h"
#include "preprocessing/scl_ml_scaler.h"
#include "preprocessing/scl_ml_metrics.h"
#include "cluster/scl_ml_dbscan.h"
#include "cluster/scl_ml_gmm.h"
#include "cluster/scl_ml_kmeans.h"
#include "decomposition/scl_ml_pca.h"
#include "linear_model/scl_ml_logistic.h"
#include "naive_bayes/scl_ml_nb.h"
#include "neighbors/scl_ml_knn.h"
#include "svm/scl_ml_svm.h"
#include "tree/scl_ml_gbdt.h"
#include "tree/scl_ml_rf.h"
#include "tree/scl_ml_tree.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)



/* ═══════════════════════════════════════════════════════════════
 * Dataset Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_dataset_init(scl_test_runner_t *tr) {
    scl_test_group("dataset_init");
    scl_ml_dataset_t ds;
    memset(&ds, 0, sizeof(ds));
    scl_allocator_t *a = scl_allocator_default();
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 3));
    SCL_EXPECT_EQ_SZ(tr, ds.n_rows, 10);
    SCL_EXPECT_EQ_SZ(tr, ds.n_cols, 3);
    SCL_EXPECT_NOT_NULL(tr, ds.data);
    SCL_EXPECT_NOT_NULL(tr, ds.targets);
    SCL_EXPECT_TRUE(tr, ds.owns_data);
    SCL_EXPECT_TRUE(tr, ds.owns_targets);
    scl_ml_dataset_destroy(&ds, a);
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
    scl_allocator_t *a = scl_allocator_default();
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
}

static void test_dataset_errors(scl_test_runner_t *tr) {
    scl_test_group("dataset_errors");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;

    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(NULL, a, 1, 1), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(&ds, NULL, 1, 1), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(&ds, a, 0, 1), SCL_ERR_INVALID_ARG);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_init(&ds, a, 1, 0), SCL_ERR_INVALID_ARG);

    SCL_EXPECT_ERROR(tr, scl_ml_dataset_wrap(NULL, NULL, NULL, 0, 0), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_dataset_wrap(&ds, NULL, NULL, 1, 1), SCL_ERR_INVALID_ARG);
}

/* ═══════════════════════════════════════════════════════════════
 * SIMD Kernel Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_simd_dot(scl_test_runner_t *tr) {
    scl_test_group("simd_dot");
    scl_ml_simd_init();
    float a[64], b[64];
    for (size_t i = 0; i < 64; i++) { a[i] = (float)i; b[i] = (float)(63 - i); }

    float dot = scl_ml_simd.dot(a, b, 64);
    float dot_f = scl_ml_simd.dot_f(a, b, 64);
    float expected = 0.0f;
    for (size_t i = 0; i < 64; i++) expected += a[i] * b[i];
    SCL_ML_NEAR(tr, dot, expected, 1e-4f);
    SCL_ML_NEAR(tr, dot_f, expected, 1e-4f);
}

static void test_simd_norm(scl_test_runner_t *tr) {
    scl_test_group("simd_norm");
    scl_ml_simd_init();
    float x[8] = {3.0f, 4.0f, 0.0f, -1.0f, 2.0f, -3.0f, 1.0f, 0.5f};
    float n2 = scl_ml_simd.norm_l2_sq(x, 8);
    float n1 = scl_ml_simd.norm_l1(x, 8);
    float n2_expected = 3*3+4*4+0+1+4+9+1+0.25f;
    float n1_expected = 3+4+0+1+2+3+1+0.5f;
    SCL_ML_NEAR(tr, n2, n2_expected, 1e-4f);
    SCL_ML_NEAR(tr, n1, n1_expected, 1e-4f);
}

static void test_simd_axpy(scl_test_runner_t *tr) {
    scl_test_group("simd_axpy");
    scl_ml_simd_init();
    float y[8] = {1,2,3,4,5,6,7,8};
    float x[8] = {8,7,6,5,4,3,2,1};
    scl_ml_simd.axpy(y, 2.0f, x, 8);
    for (size_t i = 0; i < 8; i++)
        SCL_ML_NEAR(tr, y[i], (float)((i+1) + 2*(8-i)), 1e-5f);
}

static void test_simd_reductions(scl_test_runner_t *tr) {
    scl_test_group("simd_reductions");
    scl_ml_simd_init();
    float x[8] = {-3.0f, 5.0f, -1.0f, 7.0f, 0.0f, -2.0f, 4.0f, 6.0f};
    SCL_ML_NEAR(tr, scl_ml_simd.sum(x, 8), 16.0f, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_simd.max(x, 8), 7.0f, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_simd.min(x, 8), -3.0f, 1e-5f);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_simd.argmax(x, 8), 3);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_simd.argmin(x, 8), 0);
}

static void test_simd_elementwise(scl_test_runner_t *tr) {
    scl_test_group("simd_elementwise");
    scl_ml_simd_init();
    float a[4] = {1,2,3,4}, b[4] = {5,6,7,8}, z[4];
    scl_ml_simd.add(z, a, b, 4);
    for (size_t i = 0; i < 4; i++) SCL_ML_NEAR(tr, z[i], a[i]+b[i], 1e-5f);
    scl_ml_simd.sub(z, a, b, 4);
    for (size_t i = 0; i < 4; i++) SCL_ML_NEAR(tr, z[i], a[i]-b[i], 1e-5f);
    scl_ml_simd.mul(z, a, b, 4);
    for (size_t i = 0; i < 4; i++) SCL_ML_NEAR(tr, z[i], a[i]*b[i], 1e-5f);
    scl_ml_simd.mul_s(z, a, 2.0f, 4);
    for (size_t i = 0; i < 4; i++) SCL_ML_NEAR(tr, z[i], a[i]*2, 1e-5f);
    scl_ml_simd.add_s(z, a, 10.0f, 4);
    for (size_t i = 0; i < 4; i++) SCL_ML_NEAR(tr, z[i], a[i]+10, 1e-5f);
}

static void test_simd_activations(scl_test_runner_t *tr) {
    scl_test_group("simd_activations");
    scl_ml_simd_init();
    float in[8] = {-3,-2,-1,0,1,2,3,4}, out[8];

    scl_ml_simd.relu(out, in, 8);
    for (size_t i = 0; i < 8; i++) SCL_ML_NEAR(tr, out[i], in[i] > 0 ? in[i] : 0, 1e-5f);

    scl_ml_simd.sigmoid(out, in, 8);
    for (size_t i = 0; i < 8; i++) {
        float e = 1.0f / (1.0f + expf(-in[i]));
        float diff = fabsf(out[i] - e);
        scl_expect_true(tr, diff < 2e-2f, __FILE__, __LINE__, "sigmoid near");
    }

    scl_ml_simd.softmax(out, in, 8);
    float sum = 0;
    for (size_t i = 0; i < 8; i++) sum += out[i];
    SCL_ML_NEAR(tr, sum, 1.0f, 1e-5f);

    scl_ml_simd.vexp(out, in, 8);
    for (size_t i = 0; i < 8; i++) {
        float diff = fabsf(out[i] - expf(in[i]));
        float rel = diff / fmaxf(1e-6f, expf(in[i]));
        scl_expect_true(tr, diff < 1e-2f || rel < 3e-1f, __FILE__, __LINE__, "vexp near");
    }
}

static void test_simd_distances(scl_test_runner_t *tr) {
    scl_test_group("simd_distances");
    scl_ml_simd_init();
    float a[4] = {1,2,3,4}, b[4] = {4,3,2,1};
    SCL_ML_NEAR(tr, scl_ml_simd.dist_l2_sq(a, b, 4), 20.0f, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_simd.dist_l1(a, b, 4), 8.0f, 1e-5f);

    float cos = scl_ml_simd.dist_cos(a, b, 4);
    float dot_ab = 1*4+2*3+3*2+4*1, na=sqrtf(1+4+9+16), nb=sqrtf(16+9+4+1);
    SCL_ML_NEAR(tr, cos, 1.0f - dot_ab/(na*nb), 1e-5f);
}

static void test_simd_distance_matrix(scl_test_runner_t *tr) {
    scl_test_group("simd_dist_matrix");
    scl_ml_simd_init();
    float a[6] = {0,0, 1,1, 2,2};
    float b[4] = {0,0, 1,1};
    float out[8];
    memset(out, 0, sizeof(out));
    scl_ml_simd.dist_matrix_l2_sq(out, a, b, 3, 2, 2);
    SCL_ML_NEAR(tr, out[0], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[1], 2.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[2], 2.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[3], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[4], 8.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[5], 2.0f, 1e-5f);
}

static void test_simd_fast_math(scl_test_runner_t *tr) {
    scl_test_group("simd_fast_math");
    scl_ml_simd_init();

    SCL_ML_NEAR(tr, scl_ml_simd.sigmoid_f(0.0f), 0.5f, 1e-3f);
    SCL_ML_NEAR(tr, scl_ml_simd.sigmoid_f(10.0f), 1.0f, 1e-3f);
    SCL_ML_NEAR(tr, scl_ml_simd.sigmoid_f(-10.0f), 0.0f, 1e-3f);

    SCL_ML_NEAR(tr, scl_ml_simd.tanh_f(0.0f), 0.0f, 1e-3f);
    SCL_ML_NEAR(tr, scl_ml_simd.tanh_f(5.0f), 1.0f, 1e-3f);
    SCL_ML_NEAR(tr, scl_ml_simd.tanh_f(-5.0f), -1.0f, 1e-3f);

    float ex = scl_ml_simd.exp_f(0.0f);
    SCL_ML_NEAR(tr, ex, 1.0f, 1e-3f);
}

static void test_simd_gemv(scl_test_runner_t *tr) {
    scl_test_group("simd_gemv");
    scl_ml_simd_init();
    float A[6] = {1,2, 3,4, 5,6};
    float x[2] = {2,3};
    float y[3] = {0,0,0};
    scl_ml_simd.gemv(y, A, x, 3, 2, 0.0f);
    SCL_ML_NEAR(tr, y[0], 1*2+2*3, 1e-5f);
    SCL_ML_NEAR(tr, y[1], 3*2+4*3, 1e-5f);
    SCL_ML_NEAR(tr, y[2], 5*2+6*3, 1e-5f);
}

static void test_simd_threshold(scl_test_runner_t *tr) {
    scl_test_group("simd_threshold");
    scl_ml_simd_init();
    float in[6] = {-1, 0, 0.5f, 1, 2, 3}, out[6];
    scl_ml_simd.threshold(out, in, 0.5f, 6);
    float expected[6] = {0,0,0,1,1,1};
    for (size_t i = 0; i < 6; i++) SCL_ML_NEAR(tr, out[i], expected[i], 1e-5f);
}

static void test_simd_argminmax(scl_test_runner_t *tr) {
    scl_test_group("simd_argminmax");
    scl_ml_simd_init();
    float x[6] = {5, 2, 9, 1, 7, 3};
    size_t amax = 99;
    size_t amin = scl_ml_simd.argminmax(x, 6, &amax);
    SCL_EXPECT_EQ_SZ(tr, amin, 3);
    SCL_EXPECT_EQ_SZ(tr, amax, 2);
}

/* ═══════════════════════════════════════════════════════════════
 * Scaler Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_standard_scaler(scl_test_runner_t *tr) {
    scl_test_group("standard_scaler");
    scl_allocator_t *a = scl_allocator_default();
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
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&scaler));
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
}

static void test_minmax_scaler(scl_test_runner_t *tr) {
    scl_test_group("minmax_scaler");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 1));
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = 0.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_minmax_scaler_t *scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_new(&scaler));
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
}

static void test_scaler_errors(scl_test_runner_t *tr) {
    scl_test_group("scaler_errors");
    scl_ml_standard_scaler_t *ss = NULL;
    scl_ml_minmax_scaler_t *ms = NULL;

    SCL_EXPECT_ERROR(tr, scl_ml_standard_scaler_new(NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_minmax_scaler_new(NULL), SCL_ERR_NULL_PTR);

    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&ss));
    SCL_EXPECT_OK(tr, scl_ml_minmax_scaler_new(&ms));

    SCL_EXPECT_ERROR(tr, scl_ml_standard_scaler_fit(ss, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_minmax_scaler_fit(ms, NULL), SCL_ERR_NULL_PTR);

    scl_ml_standard_scaler_free(ss);
    scl_ml_minmax_scaler_free(ms);
}

/* ═══════════════════════════════════════════════════════════════
 * Metrics Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_metrics_scoring(scl_test_runner_t *tr) {
    scl_test_group("metrics_scoring");
    float yt[5] = {1,2,3,4,5};
    float yp[5] = {1.1f, 1.9f, 3.2f, 3.8f, 5.1f};
    float mse = scl_ml_mean_squared_error(yt, yp, 5);
    float mae = scl_ml_mean_absolute_error(yt, yp, 5);
    SCL_ML_NEAR(tr, mse, (0.01f+0.01f+0.04f+0.04f+0.01f)/5.0f, 1e-5f);
    SCL_ML_NEAR(tr, mae, (0.1f+0.1f+0.2f+0.2f+0.1f)/5.0f, 1e-5f);

    float r2 = scl_ml_r2_score(yt, yp, 5);
    float var_y = (4+1+0+1+4)/5.0f;
    float mse_v = 0.11f/5.0f;
    SCL_ML_NEAR(tr, r2, 1.0f - mse_v/var_y, 1e-4f);

    float acc = scl_ml_accuracy(yt, yp, 5);
    SCL_ML_NEAR(tr, acc, 0.0f, 1e-5f);

    float yc[3] = {0,1,0}, ypc[3] = {0,1,0};
    SCL_ML_NEAR(tr, scl_ml_accuracy(yc, ypc, 3), 1.0f, 1e-5f);
}

static void test_metrics_logloss(scl_test_runner_t *tr) {
    scl_test_group("metrics_logloss");
    float yt[4] = {0,1,0,1};
    float yp[4] = {0.1f, 0.9f, 0.2f, 0.8f};
    float ll = scl_ml_log_loss(yt, yp, 4);
    float expected = 0.0f;
    for (size_t i = 0; i < 4; i++) {
        float p = yp[i];
        float t = yt[i];
        float eps = 1e-15f;
        p = fminf(fmaxf(p, eps), 1.0f - eps);
        expected -= (t * logf(p) + (1-t) * logf(1-p));
    }
    expected /= 4.0f;
    SCL_ML_NEAR(tr, ll, expected, 1e-4f);
}

static void test_metrics_distance_matrix(scl_test_runner_t *tr) {
    scl_test_group("metrics_distance_matrix");
    float a[6] = {0,0, 1,1, 2,2};
    float b[4] = {0,0, 1,1};
    float out[8];
    SCL_EXPECT_OK(tr, scl_ml_distance_matrix(out, a, b, 3, 2, 2, SCL_ML_DISTANCE_L2));
    /* L2 squared distance */
    SCL_ML_NEAR(tr, out[0], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[1], 2.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[2], 2.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[3], 0.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[4], 8.0f, 1e-5f);
    SCL_ML_NEAR(tr, out[5], 2.0f, 1e-5f);
}

static void test_metrics_kernel(scl_test_runner_t *tr) {
    scl_test_group("metrics_kernel");
    float x[3] = {1,0,1};
    float y[3] = {0,1,1};
    float k = scl_ml_kernel(x, y, 3, SCL_ML_KERNEL_RBF, 1.0f, 0, 3);
    /* K(x,y) = exp(-||x-y||^2) = exp(-(1+1+0)) = exp(-2) */
    SCL_ML_NEAR(tr, k, expf(-2.0f), 1e-4f);
}

static void test_metrics_errors(scl_test_runner_t *tr) {
    scl_test_group("metrics_errors");
    SCL_EXPECT_ERROR(tr, scl_ml_distance_matrix(NULL, NULL, NULL, 0, 0, 0, 0), SCL_ERR_NULL_PTR);
    float a[1]={0}, b[1]={0}, o[1];
    SCL_EXPECT_ERROR(tr, scl_ml_distance_matrix(o, NULL, b, 1, 1, 1, 0), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_distance_matrix(o, a, NULL, 1, 1, 1, 0), SCL_ERR_NULL_PTR);
}

/* ═══════════════════════════════════════════════════════════════
 * Linear Model Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_linear_ols_normal_eq(scl_test_runner_t *tr) {
    scl_test_group("linear_ols_normal_eq");
    /* y = 3*x0 - 2*x1 + 5, generate data with noise */
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 50, 2));
    srand(42);
    for (size_t i = 0; i < 50; i++) {
        float x0 = (float)((double)rand()/(double)RAND_MAX * 10.0);
        float x1 = (float)((double)rand()/(double)RAND_MAX * 10.0);
        float noise = (float)((double)rand()/(double)RAND_MAX * 0.1 - 0.05);
        ds.data[i * ds.row_stride + 0] = x0;
        ds.data[i * ds.row_stride + 1] = x1;
        ds.targets[i] = 3.0f * x0 - 2.0f * x1 + 5.0f + noise;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
    params.solver = SCL_ML_SOLVER_NORMAL_EQ;
    params.penalty = SCL_ML_PENALTY_NONE;

    scl_ml_linear_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);

    SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

    /* Check weights */
    const SCL_ML_FLOAT *w = scl_ml_linear_get_weights(model);
    float intercept = scl_ml_linear_get_intercept(model);
    SCL_EXPECT_NOT_NULL(tr, w);
    SCL_ML_NEAR(tr, w[0], 3.0f, 0.5f);
    SCL_ML_NEAR(tr, w[1], -2.0f, 0.5f);
    SCL_ML_NEAR(tr, intercept, 5.0f, 0.5f);

    /* Predict and check R2 near 1 */
    float *pred = (float *)calloc(50, sizeof(float));
    SCL_EXPECT_OK(tr, scl_ml_linear_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 50);
    SCL_EXPECT_TRUE(tr, r2 > 0.99f);
    free(pred);

    scl_ml_linear_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_linear_ridge_sgd(scl_test_runner_t *tr) {
    scl_test_group("linear_ridge_sgd");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 30, 1));
    for (size_t i = 0; i < 30; i++) {
        float x = (float)i;
        ds.data[i * ds.row_stride] = x;
        ds.targets[i] = 2.0f * x - 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
    params.solver = SCL_ML_SOLVER_SGD;
    params.penalty = SCL_ML_PENALTY_L2;
    params.alpha = 0.001;
    params.learning_rate = 0.01;
    params.max_iter = 3000;
    params.batch_size = 0;

    scl_ml_linear_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

    float *pred = (float *)calloc(30, sizeof(float));
    SCL_EXPECT_OK(tr, scl_ml_linear_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 30);
    SCL_EXPECT_TRUE(tr, r2 > 0.95f);
    free(pred);

    scl_ml_linear_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_linear_lasso_cd(scl_test_runner_t *tr) {
    scl_test_group("linear_lasso_cd");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 3));
    for (size_t i = 0; i < 20; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 2);
        ds.data[i * ds.row_stride + 2] = (SCL_ML_FLOAT)(i * 3);
        ds.targets[i] = (SCL_ML_FLOAT)(i);
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
    params.solver = SCL_ML_SOLVER_CD;
    params.penalty = SCL_ML_PENALTY_L1;
    params.alpha = 0.01;
    params.max_iter = 1000;

    scl_ml_linear_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

    float *pred = (float *)calloc(20, sizeof(float));
    SCL_EXPECT_OK(tr, scl_ml_linear_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 20);
    SCL_EXPECT_TRUE(tr, r2 > 0.90f);
    free(pred);

    scl_ml_linear_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_linear_model_accessors(scl_test_runner_t *tr) {
    scl_test_group("linear_model_accessors");
    scl_ml_linear_params_t ap = SCL_ML_LINEAR_PARAMS_DEFAULT();
    scl_ml_linear_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_linear_new(&m, ap));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_linear_get_n_features(m), 0);
    SCL_ML_NEAR(tr, scl_ml_linear_get_intercept(m), 0.0f, 1e-5f);
    SCL_EXPECT_NULL(tr, scl_ml_linear_get_weights(m));
    scl_ml_linear_free(m);
}

static void test_linear_errors(scl_test_runner_t *tr) {
    scl_test_group("linear_errors");
    scl_ml_linear_params_t ep = SCL_ML_LINEAR_PARAMS_DEFAULT();
    scl_ml_linear_t *m = NULL;

    SCL_EXPECT_ERROR(tr, scl_ml_linear_new(NULL, ep), SCL_ERR_NULL_PTR);

    scl_ml_linear_params_t params = ep;
    SCL_EXPECT_OK(tr, scl_ml_linear_new(&m, params));

    SCL_EXPECT_ERROR(tr, scl_ml_linear_fit(m, NULL), SCL_ERR_NULL_PTR);

    scl_ml_linear_free(m);
}

/* ═══════════════════════════════════════════════════════════════
 * Serialization Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_linear_serialization(scl_test_runner_t *tr) {
    scl_test_group("linear_serialization");
    scl_allocator_t *a = scl_allocator_default();

    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 2));
    srand(42);
    for (size_t i = 0; i < 10; i++) {
        float x0 = (float)(i);
        float x1 = (float)(rand() % 100);
        ds.data[i * ds.row_stride + 0] = x0;
        ds.data[i * ds.row_stride + 1] = x1;
        ds.targets[i] = 3.0f * x0 + 2.0f * x1 + 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_linear_params_t params = SCL_ML_LINEAR_PARAMS_DEFAULT();
    params.solver = SCL_ML_SOLVER_NORMAL_EQ;

    scl_ml_linear_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_linear_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_linear_fit(model, &ds));

    /* Save */
    uint8_t *buf = NULL;
    size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_linear_save(model, &buf, &len, a));
    SCL_EXPECT_NOT_NULL(tr, buf);
    SCL_EXPECT_TRUE(tr, len > sizeof(scl_ml_serial_header_t));

    const SCL_ML_FLOAT *w_orig = scl_ml_linear_get_weights(model);
    float w_saved[2] = { w_orig[0], w_orig[1] };
    float b_saved = scl_ml_linear_get_intercept(model);
    scl_ml_linear_free(model);

    /* Load */
    scl_ml_linear_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_linear_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);

    const SCL_ML_FLOAT *w_loaded = scl_ml_linear_get_weights(loaded);
    SCL_EXPECT_NOT_NULL(tr, w_loaded);
    SCL_ML_NEAR(tr, scl_ml_linear_get_intercept(loaded), b_saved, 1e-5f);
    for (size_t i = 0; i < 2; i++)
        SCL_ML_NEAR(tr, w_loaded[i], w_saved[i], 1e-5f);

    float *pred = (float *)calloc(10, sizeof(float));
    SCL_EXPECT_OK(tr, scl_ml_linear_predict(loaded, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 10);
    SCL_EXPECT_TRUE(tr, r2 > 0.99f);
    free(pred);

    scl_ml_linear_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * PCA Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_pca_fit_transform(scl_test_runner_t *tr) {
    scl_test_group("pca_fit_transform");
    scl_allocator_t *a = scl_allocator_default();
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
}

static void test_pca_auto_components(scl_test_runner_t *tr) {
    scl_test_group("pca_auto_components");
    scl_allocator_t *a = scl_allocator_default();
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
    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, params));
    SCL_EXPECT_OK(tr, scl_ml_pca_fit(pca, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_pca_get_n_components(pca), 2);

    SCL_ML_FLOAT proj[10];
    SCL_EXPECT_OK(tr, scl_ml_pca_fit_transform(pca, &ds, proj));

    scl_ml_pca_free(pca);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_pca_getters(scl_test_runner_t *tr) {
    scl_test_group("pca_getters");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 2));
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(3 - i);
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_pca_params_t params = SCL_ML_PCA_PARAMS_DEFAULT();
    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, params));
    SCL_EXPECT_OK(tr, scl_ml_pca_fit(pca, &ds));

    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_components(pca));
    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_mean(pca));
    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_explained_variance(pca));
    SCL_EXPECT_NOT_NULL(tr, scl_ml_pca_get_explained_variance_ratio(pca));

    scl_ml_pca_free(pca);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_pca_errors(scl_test_runner_t *tr) {
    scl_test_group("pca_errors");
    scl_ml_pca_params_t ep = SCL_ML_PCA_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_pca_new(NULL, ep), SCL_ERR_NULL_PTR);

    scl_ml_pca_t *pca = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_new(&pca, ep));

    SCL_EXPECT_ERROR(tr, scl_ml_pca_fit(pca, NULL), SCL_ERR_NULL_PTR);

    scl_ml_pca_free(pca);
}

static void test_pca_serialization(scl_test_runner_t *tr) {
    scl_test_group("pca_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 4, 2));
    for (size_t i = 0; i < 4; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i * 2);
        ds.targets[i] = 0;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_pca_params_t params = SCL_ML_PCA_PARAMS_DEFAULT();
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

    scl_ml_pca_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_pca_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_pca_get_n_components(loaded), 2);
    SCL_ML_NEAR(tr, scl_ml_pca_get_components(loaded)[0], c0, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_pca_get_components(loaded)[1], c1, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_pca_get_mean(loaded)[0], m0, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_pca_get_explained_variance(loaded)[0], ev, 1e-5f);

    scl_ml_pca_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * GMM Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_gmm_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("gmm_fit_predict");
    scl_allocator_t *a = scl_allocator_default();

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
}

static void test_gmm_predict_proba(scl_test_runner_t *tr) {
    scl_test_group("gmm_predict_proba");
    scl_allocator_t *a = scl_allocator_default();

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
}

static void test_gmm_errors(scl_test_runner_t *tr) {
    scl_test_group("gmm_errors");
    scl_ml_gmm_params_t ep = SCL_ML_GMM_PARAMS_DEFAULT();

    SCL_EXPECT_ERROR(tr, scl_ml_gmm_new(NULL, ep), SCL_ERR_NULL_PTR);

    scl_ml_gmm_t *gmm = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gmm_new(&gmm, ep));

    SCL_EXPECT_ERROR(tr, scl_ml_gmm_fit(gmm, NULL), SCL_ERR_NULL_PTR);
    SCL_EXPECT_ERROR(tr, scl_ml_gmm_predict(gmm, NULL, NULL), SCL_ERR_NULL_PTR);

    scl_ml_gmm_free(gmm);
}

static void test_gmm_serialization(scl_test_runner_t *tr) {
    scl_test_group("gmm_serialization");
    scl_allocator_t *a = scl_allocator_default();

    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) ds.data[i * ds.row_stride] = 0.0f;
    for (size_t i = 5; i < 10; i++) ds.data[i * ds.row_stride] = 5.0f;
    for (size_t i = 0; i < 10; i++) ds.targets[i] = 0;
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gmm_params_t params = SCL_ML_GMM_PARAMS_DEFAULT();
    params.n_components = 2;
    params.random_seed = 42;

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
}

/* ═══════════════════════════════════════════════════════════════
 * Logistic Regression Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_logistic_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("logistic_fit_predict");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    /* Binary classification: x<5 -> 0, x>=5 -> 1 */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 2));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i * 0.5f;
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i);
        ds.targets[i] = 0.0f;
    }
    for (size_t i = 10; i < 20; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(5.0f + i * 0.5f);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i + 10);
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_standard_scaler_t *log_scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&log_scaler));
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_fit_transform(log_scaler, &ds));

    scl_ml_logistic_params_t params = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    params.max_iter = 5000;
    params.learning_rate = 0.5f;
    params.alpha = 1e-6f;
    params.random_seed = 42;
    scl_ml_logistic_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_logistic_fit(model, &ds));

    SCL_ML_FLOAT pred[20];
    SCL_EXPECT_OK(tr, scl_ml_logistic_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 20);
    SCL_EXPECT_TRUE(tr, acc > 0.70f);

    scl_ml_logistic_free(model);
    scl_ml_standard_scaler_free(log_scaler);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_logistic_predict_proba(scl_test_runner_t *tr) {
    scl_test_group("logistic_predict_proba");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_standard_scaler_t *log_scaler = NULL;
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_new(&log_scaler));
    SCL_EXPECT_OK(tr, scl_ml_standard_scaler_fit_transform(log_scaler, &ds));

    scl_ml_logistic_params_t params = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    params.max_iter = 5000;
    params.learning_rate = 0.5f;
    params.alpha = 1e-6f;
    params.random_seed = 42;
    scl_ml_logistic_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_logistic_fit(model, &ds));

    SCL_ML_FLOAT proba[10];
    SCL_EXPECT_OK(tr, scl_ml_logistic_predict_proba(model, &ds, proba));
    for (size_t i = 0; i < 10; i++)
        SCL_EXPECT_TRUE(tr, proba[i] >= 0.0f && proba[i] <= 1.0f);

    scl_ml_logistic_free(model);
    scl_ml_standard_scaler_free(log_scaler);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_logistic_getters(scl_test_runner_t *tr) {
    scl_test_group("logistic_getters");
    scl_ml_logistic_params_t p = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    scl_ml_logistic_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&m, p));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_logistic_get_n_features(m), 0);
    SCL_ML_NEAR(tr, scl_ml_logistic_get_intercept(m), 0.0f, 1e-5f);
    SCL_EXPECT_NULL(tr, scl_ml_logistic_get_coef(m));
    scl_ml_logistic_free(m);
}

static void test_logistic_errors(scl_test_runner_t *tr) {
    scl_test_group("logistic_errors");
    scl_ml_logistic_params_t ep = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_logistic_new(NULL, ep), SCL_ERR_NULL_PTR);
    scl_ml_logistic_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&m, ep));
    SCL_EXPECT_ERROR(tr, scl_ml_logistic_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_logistic_free(m);
}

static void test_logistic_serialization(scl_test_runner_t *tr) {
    scl_test_group("logistic_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_logistic_params_t params = SCL_ML_LOGISTIC_PARAMS_DEFAULT();
    params.max_iter = 500;
    params.random_seed = 42;
    scl_ml_logistic_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_logistic_fit(model, &ds));

    SCL_ML_FLOAT coef_orig = scl_ml_logistic_get_coef(model)[0];
    SCL_ML_FLOAT int_orig = scl_ml_logistic_get_intercept(model);

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_logistic_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_logistic_free(model);

    scl_ml_logistic_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_logistic_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_ML_NEAR(tr, scl_ml_logistic_get_coef(loaded)[0], coef_orig, 1e-5f);
    SCL_ML_NEAR(tr, scl_ml_logistic_get_intercept(loaded), int_orig, 1e-5f);

    scl_ml_logistic_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * Naive Bayes Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_nb_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("nb_fit_predict");
    scl_allocator_t *a = scl_allocator_default();
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
}

static void test_nb_predict_proba(scl_test_runner_t *tr) {
    scl_test_group("nb_predict_proba");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 2; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 2; i < 4; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    for (size_t i = 4; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 2.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_nb_params_t params = SCL_ML_NB_PARAMS_DEFAULT();
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
}

static void test_nb_errors(scl_test_runner_t *tr) {
    scl_test_group("nb_errors");
    scl_ml_nb_params_t nep = SCL_ML_NB_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_nb_new(NULL, nep), SCL_ERR_NULL_PTR);
    scl_ml_nb_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_new(&m, nep));
    SCL_EXPECT_ERROR(tr, scl_ml_nb_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_nb_free(m);
}

static void test_nb_serialization(scl_test_runner_t *tr) {
    scl_test_group("nb_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 5.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_nb_params_t np = SCL_ML_NB_PARAMS_DEFAULT();
    scl_ml_nb_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_new(&model, np));
    SCL_EXPECT_OK(tr, scl_ml_nb_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[6];
    SCL_EXPECT_OK(tr, scl_ml_nb_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_nb_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_nb_free(model);

    scl_ml_nb_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_nb_load(&loaded, buf, len, np));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_nb_get_n_classes(loaded), 2);

    SCL_ML_FLOAT pred_load[6];
    SCL_EXPECT_OK(tr, scl_ml_nb_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 6; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_nb_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * KNN Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_knn_fit_predict(scl_test_runner_t *tr) {
    scl_test_group("knn_fit_predict");
    scl_allocator_t *a = scl_allocator_default();
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
}

static void test_knn_distance_weighted(scl_test_runner_t *tr) {
    scl_test_group("knn_distance_weighted");
    scl_allocator_t *a = scl_allocator_default();
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
}

static void test_knn_errors(scl_test_runner_t *tr) {
    scl_test_group("knn_errors");
    scl_ml_knn_params_t kep = SCL_ML_KNN_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_knn_new(NULL, kep), SCL_ERR_NULL_PTR);
    scl_ml_knn_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_new(&m, kep));
    SCL_EXPECT_ERROR(tr, scl_ml_knn_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_knn_free(m);
}

static void test_knn_serialization(scl_test_runner_t *tr) {
    scl_test_group("knn_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0.0f; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_knn_params_t kp = SCL_ML_KNN_PARAMS_DEFAULT();
    scl_ml_knn_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_new(&model, kp));
    SCL_EXPECT_OK(tr, scl_ml_knn_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[6];
    SCL_EXPECT_OK(tr, scl_ml_knn_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_knn_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_knn_free(model);

    scl_ml_knn_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_knn_load(&loaded, buf, len, kp));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_knn_get_n_samples(loaded), 6);

    SCL_ML_FLOAT pred_load[6];
    SCL_EXPECT_OK(tr, scl_ml_knn_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 6; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_knn_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * K-Means Tests
 * ═══════════════════════════════════════════════════════════════ */
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

/* ═══════════════════════════════════════════════════════════════
 * DBSCAN Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_dbscan_fit(scl_test_runner_t *tr) {
    scl_test_group("dbscan_fit");
    scl_allocator_t *a = scl_allocator_default();
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
}

static void test_dbscan_predict(scl_test_runner_t *tr) {
    scl_test_group("dbscan_predict");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 0; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_dbscan_t *model = NULL;
    scl_ml_dbscan_params_t params = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    params.eps = 3.0f;
    params.min_pts = 2;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_dbscan_fit(model, &ds));

    int out[6];
    SCL_EXPECT_OK(tr, scl_ml_dbscan_predict(model, &ds, out));
    SCL_EXPECT_EQ_SZ(tr, (size_t)out[0], (size_t)out[1]);
    SCL_EXPECT_EQ_SZ(tr, (size_t)out[3], (size_t)out[4]);
    SCL_EXPECT_TRUE(tr, out[0] != out[3]);

    scl_ml_dbscan_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_dbscan_errors(scl_test_runner_t *tr) {
    scl_test_group("dbscan_errors");
    scl_ml_dbscan_params_t dep = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_dbscan_new(NULL, dep), SCL_ERR_NULL_PTR);
    scl_ml_dbscan_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_dbscan_new(&m, dep));
    SCL_EXPECT_ERROR(tr, scl_ml_dbscan_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_dbscan_free(m);
}

static void test_dbscan_serialization(scl_test_runner_t *tr) {
    scl_test_group("dbscan_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 6, 1));
    for (size_t i = 0; i < 3; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = 0; }
    for (size_t i = 3; i < 6; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 0; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_dbscan_t *model = NULL;
    scl_ml_dbscan_params_t params = SCL_ML_DBSCAN_PARAMS_DEFAULT();
    params.eps = 3.0f; params.min_pts = 2;
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
}

/* ═══════════════════════════════════════════════════════════════
 * Decision Tree Tests
 * ═══════════════════════════════════════════════════════════════ */
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

/* ═══════════════════════════════════════════════════════════════
 * Random Forest Tests
 * ═══════════════════════════════════════════════════════════════ */
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

/* ═══════════════════════════════════════════════════════════════
 * GBDT Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_gbdt_regression(scl_test_runner_t *tr) {
    scl_test_group("gbdt_regression");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 20, 1));
    for (size_t i = 0; i < 20; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)i;
        ds.targets[i] = 2.0f * (SCL_ML_FLOAT)i + 3.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gbdt_params_t params = SCL_ML_GBDT_PARAMS_DEFAULT();
    params.n_estimators = 20;
    params.learning_rate = 0.5f;
    params.max_depth = 3;
    params.random_seed = 42;
    scl_ml_gbdt_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_gbdt_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_gbdt_get_n_features(model), 1);

    SCL_ML_FLOAT pred[20];
    SCL_EXPECT_OK(tr, scl_ml_gbdt_predict(model, &ds, pred));
    float r2 = scl_ml_r2_score(ds.targets, pred, 20);
    SCL_EXPECT_TRUE(tr, r2 > 0.90f);

    scl_ml_gbdt_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_gbdt_errors(scl_test_runner_t *tr) {
    scl_test_group("gbdt_errors");
    scl_ml_gbdt_params_t gep = SCL_ML_GBDT_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_gbdt_new(NULL, gep), SCL_ERR_NULL_PTR);
    scl_ml_gbdt_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_new(&m, gep));
    SCL_EXPECT_ERROR(tr, scl_ml_gbdt_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_gbdt_free(m);
}

static void test_gbdt_serialization(scl_test_runner_t *tr) {
    scl_test_group("gbdt_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 10; i++) {
        ds.data[i * ds.row_stride] = (SCL_ML_FLOAT)i;
        ds.targets[i] = (SCL_ML_FLOAT)(2 * i);
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_gbdt_t *model = NULL;
    scl_ml_gbdt_params_t params = SCL_ML_GBDT_PARAMS_DEFAULT();
    params.n_estimators = 10;
    params.max_depth = 2;
    params.random_seed = 42;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_gbdt_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[10];
    SCL_EXPECT_OK(tr, scl_ml_gbdt_predict(model, &ds, pred_orig));

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_gbdt_free(model);

    scl_ml_gbdt_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_gbdt_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);

    SCL_ML_FLOAT pred_load[10];
    SCL_EXPECT_OK(tr, scl_ml_gbdt_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 10; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-4f);

    scl_ml_gbdt_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * SVM Tests
 * ═══════════════════════════════════════════════════════════════ */
static void test_svm_linear(scl_test_runner_t *tr) {
    scl_test_group("svm_linear");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 2));
    for (size_t i = 0; i < 5; i++) {
        ds.data[i * ds.row_stride + 0] = 0.0f;
        ds.data[i * ds.row_stride + 1] = 0.0f;
        ds.targets[i] = -1.0f;
    }
    for (size_t i = 5; i < 10; i++) {
        ds.data[i * ds.row_stride + 0] = 10.0f;
        ds.data[i * ds.row_stride + 1] = 10.0f;
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_svm_params_t params = SCL_ML_SVM_PARAMS_DEFAULT();
    params.kernel = SCL_ML_KERNEL_LINEAR;
    params.C = 1.0f;
    params.max_iter = 500;
    params.random_seed = 42;
    scl_ml_svm_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_svm_new(&model, params));
    SCL_EXPECT_NOT_NULL(tr, model);
    SCL_EXPECT_OK(tr, scl_ml_svm_fit(model, &ds));
    SCL_EXPECT_EQ_SZ(tr, scl_ml_svm_get_n_features(model), 2);
    SCL_EXPECT_TRUE(tr, scl_ml_svm_get_n_support(model) > 0);

    SCL_ML_FLOAT pred[10];
    SCL_EXPECT_OK(tr, scl_ml_svm_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 10);
    SCL_EXPECT_TRUE(tr, acc > 0.99f);

    scl_ml_svm_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_svm_rbf(scl_test_runner_t *tr) {
    scl_test_group("svm_rbf");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    /* Non-linear separable pattern */
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 12, 2));
    for (size_t i = 0; i < 6; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)i;
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)i;
        ds.targets[i] = -1.0f;
    }
    for (size_t i = 6; i < 12; i++) {
        ds.data[i * ds.row_stride + 0] = (SCL_ML_FLOAT)(i + 20);
        ds.data[i * ds.row_stride + 1] = (SCL_ML_FLOAT)(i + 20);
        ds.targets[i] = 1.0f;
    }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_svm_params_t params = SCL_ML_SVM_PARAMS_DEFAULT();
    params.kernel = SCL_ML_KERNEL_RBF;
    params.C = 10.0f;
    params.gamma = 0.01f;
    params.tol = 1e-3f;
    params.max_iter = 1000;
    params.random_seed = 42;
    scl_ml_svm_t *model = NULL;
    SCL_EXPECT_OK(tr, scl_ml_svm_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_svm_fit(model, &ds));

    SCL_ML_FLOAT pred[12];
    SCL_EXPECT_OK(tr, scl_ml_svm_predict(model, &ds, pred));
    float acc = scl_ml_accuracy(ds.targets, pred, 12);
    SCL_EXPECT_TRUE(tr, acc > 0.90f);

    scl_ml_svm_free(model);
    scl_ml_dataset_destroy(&ds, a);
}

static void test_svm_errors(scl_test_runner_t *tr) {
    scl_test_group("svm_errors");
    scl_ml_svm_params_t sep = SCL_ML_SVM_PARAMS_DEFAULT();
    SCL_EXPECT_ERROR(tr, scl_ml_svm_new(NULL, sep), SCL_ERR_NULL_PTR);
    scl_ml_svm_t *m = NULL;
    SCL_EXPECT_OK(tr, scl_ml_svm_new(&m, sep));
    SCL_EXPECT_ERROR(tr, scl_ml_svm_fit(m, NULL), SCL_ERR_NULL_PTR);
    scl_ml_svm_free(m);
}

static void test_svm_serialization(scl_test_runner_t *tr) {
    scl_test_group("svm_serialization");
    scl_allocator_t *a = scl_allocator_default();
    scl_ml_dataset_t ds;
    SCL_EXPECT_OK(tr, scl_ml_dataset_init(&ds, a, 10, 1));
    for (size_t i = 0; i < 5; i++) { ds.data[i * ds.row_stride] = 0.0f; ds.targets[i] = -1.0f; }
    for (size_t i = 5; i < 10; i++) { ds.data[i * ds.row_stride] = 10.0f; ds.targets[i] = 1.0f; }
    SCL_EXPECT_OK(tr, scl_ml_dataset_prepare(&ds, a));

    scl_ml_svm_t *model = NULL;
    scl_ml_svm_params_t params = SCL_ML_SVM_PARAMS_DEFAULT();
    params.kernel = SCL_ML_KERNEL_LINEAR;
    params.C = 1.0f;
    params.max_iter = 200;
    params.random_seed = 42;
    SCL_EXPECT_OK(tr, scl_ml_svm_new(&model, params));
    SCL_EXPECT_OK(tr, scl_ml_svm_fit(model, &ds));

    SCL_ML_FLOAT pred_orig[10];
    SCL_EXPECT_OK(tr, scl_ml_svm_predict(model, &ds, pred_orig));
    size_t ns = scl_ml_svm_get_n_support(model);

    uint8_t *buf = NULL; size_t len = 0;
    SCL_EXPECT_OK(tr, scl_ml_svm_save(model, &buf, &len, a));
    SCL_EXPECT_TRUE(tr, len > 0);
    scl_ml_svm_free(model);

    scl_ml_svm_t *loaded = NULL;
    SCL_EXPECT_OK(tr, scl_ml_svm_load(&loaded, buf, len, params));
    SCL_EXPECT_NOT_NULL(tr, loaded);
    SCL_EXPECT_EQ_SZ(tr, scl_ml_svm_get_n_support(loaded), ns);

    SCL_ML_FLOAT pred_load[10];
    SCL_EXPECT_OK(tr, scl_ml_svm_predict(loaded, &ds, pred_load));
    for (size_t i = 0; i < 10; i++) SCL_ML_NEAR(tr, pred_load[i], pred_orig[i], 1e-5f);

    scl_ml_svm_free(loaded);
    scl_free(a, buf);
    scl_ml_dataset_destroy(&ds, a);
}

/* ═══════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════ */
int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    /* Dataset */
    test_dataset_init(&tr);
    test_dataset_wrap(&tr);
    test_dataset_prepare(&tr);
    test_dataset_errors(&tr);

    /* SIMD kernels */
    test_simd_dot(&tr);
    test_simd_norm(&tr);
    test_simd_axpy(&tr);
    test_simd_reductions(&tr);
    test_simd_elementwise(&tr);
    test_simd_activations(&tr);
    test_simd_distances(&tr);
    test_simd_distance_matrix(&tr);
    test_simd_fast_math(&tr);
    test_simd_gemv(&tr);
    test_simd_threshold(&tr);
    test_simd_argminmax(&tr);

    /* Scalers */
    test_standard_scaler(&tr);
    test_minmax_scaler(&tr);
    test_scaler_errors(&tr);

    /* Metrics */
    test_metrics_scoring(&tr);
    test_metrics_logloss(&tr);
    test_metrics_distance_matrix(&tr);
    test_metrics_kernel(&tr);
    test_metrics_errors(&tr);

    /* Linear model */
    test_linear_ols_normal_eq(&tr);
    test_linear_ridge_sgd(&tr);
    test_linear_lasso_cd(&tr);
    test_linear_model_accessors(&tr);
    test_linear_errors(&tr);
    test_linear_serialization(&tr);

    /* PCA */
    test_pca_fit_transform(&tr);
    test_pca_auto_components(&tr);
    test_pca_getters(&tr);
    test_pca_errors(&tr);
    test_pca_serialization(&tr);

    /* GMM */
    test_gmm_fit_predict(&tr);
    test_gmm_predict_proba(&tr);
    test_gmm_errors(&tr);
    test_gmm_serialization(&tr);

    /* Logistic Regression */
    test_logistic_fit_predict(&tr);
    test_logistic_predict_proba(&tr);
    test_logistic_getters(&tr);
    test_logistic_errors(&tr);
    test_logistic_serialization(&tr);

    /* Naive Bayes */
    test_nb_fit_predict(&tr);
    test_nb_predict_proba(&tr);
    test_nb_errors(&tr);
    test_nb_serialization(&tr);

    /* KNN */
    test_knn_fit_predict(&tr);
    test_knn_distance_weighted(&tr);
    test_knn_errors(&tr);
    test_knn_serialization(&tr);

    /* K-Means */
    test_kmeans_fit_predict(&tr);
    test_kmeans_errors(&tr);
    test_kmeans_serialization(&tr);

    /* DBSCAN */
    test_dbscan_fit(&tr);
    test_dbscan_predict(&tr);
    test_dbscan_errors(&tr);
    test_dbscan_serialization(&tr);

    /* Decision Tree */
    test_tree_classification(&tr);
    test_tree_regression(&tr);
    test_tree_errors(&tr);
    test_tree_serialization(&tr);

    /* Random Forest */
    test_rf_classification(&tr);
    test_rf_regression(&tr);
    test_rf_errors(&tr);
    test_rf_serialization(&tr);

    /* GBDT */
    test_gbdt_regression(&tr);
    test_gbdt_errors(&tr);
    test_gbdt_serialization(&tr);

    /* SVM */
    test_svm_linear(&tr);
    test_svm_rbf(&tr);
    test_svm_errors(&tr);
    test_svm_serialization(&tr);

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
