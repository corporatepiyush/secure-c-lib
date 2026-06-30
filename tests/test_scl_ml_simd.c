/* Auto-split from test_scl_ml.c */

#include "scl_test.h"
#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>

/* ── Helpers ─────────────────────────────────────────────────── */
#define SCL_ML_NEAR(tr, a, b, eps) do { \
    float _d = (float)fabs((double)(a) - (double)(b)); \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__, "near check: " #a " ~ " #b); \
} while(0)

/* ── SIMD Kernel Tests ── */

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

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

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

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
