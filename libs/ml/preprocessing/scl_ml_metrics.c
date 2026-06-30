/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "scl_ml_metrics.h"
#include "scl_ml_simd.h"
#include <math.h>
#include <float.h>

/* ── Distance matrix dispatch ───────────────────────────────── */
SCL_WARN_UNUSED scl_error_t
scl_ml_distance_matrix(SCL_ML_FLOAT *out,
                        const SCL_ML_FLOAT *a, const SCL_ML_FLOAT *b,
                        size_t n, size_t m, size_t d,
                        scl_ml_distance_t metric) {
    if (scl_unlikely(!out || !a || !b)) return SCL_ERR_NULL_PTR;
    scl_ml_simd_init();

    switch (metric) {
    case SCL_ML_DISTANCE_L2:
        scl_ml_simd.dist_matrix_l2_sq(out, a, b, n, m, d);
        break;
    case SCL_ML_DISTANCE_L1:
        scl_ml_simd.dist_matrix_l1(out, a, b, n, m, d);
        break;
    case SCL_ML_DISTANCE_COSINE:
        scl_ml_simd.dist_matrix_cos(out, a, b, n, m, d);
        break;
    default:
        return SCL_ERR_NOT_IMPLEMENTED;
    }
    return SCL_OK;
}

/* ── Kernel functions ────────────────────────────────────────── */

float
scl_ml_kernel(const SCL_ML_FLOAT *x, const SCL_ML_FLOAT *y,
              size_t d, scl_ml_kernel_t type,
              float gamma, float coef0, int degree) {
    scl_ml_simd_init();

    switch (type) {
    case SCL_ML_KERNEL_LINEAR:
        return scl_ml_simd.dot_f(x, y, d);

    case SCL_ML_KERNEL_RBF: {
        float dist_sq = scl_ml_simd.dist_l2_sq(x, y, d);
        return expf(-gamma * dist_sq);
    }
    case SCL_ML_KERNEL_POLY: {
        float dot = scl_ml_simd.dot(x, y, d);
        float val = gamma * dot + coef0;
        /* Fast power for small integer degree */
        if (degree == 2) return val * val;
        if (degree == 3) return val * val * val;
        return powf(val, (float)degree);
    }
    case SCL_ML_KERNEL_SIGMOID: {
        float dot = scl_ml_simd.dot(x, y, d);
        return tanhf(gamma * dot + coef0);
    }
    default:
        return 0.0f;
    }
}

SCL_WARN_UNUSED scl_error_t
scl_ml_kernel_matrix(SCL_ML_FLOAT *k_out,
                      const SCL_ML_FLOAT *a, const SCL_ML_FLOAT *b,
                      size_t n, size_t m, size_t d,
                      scl_ml_kernel_t type,
                      float gamma, float coef0, int degree) {
    if (scl_unlikely(!k_out || !a || !b)) return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < m; j++) {
            k_out[i * m + j] = scl_ml_kernel(&a[i * d], &b[j * d],
                                              d, type, gamma, coef0, degree);
        }
    }
    return SCL_OK;
}

/* ── Scoring functions ───────────────────────────────────────── */

float
scl_ml_mean_squared_error(const SCL_ML_FLOAT *y_true,
                           const SCL_ML_FLOAT *y_pred, size_t n) {
    if (scl_unlikely(!y_true || !y_pred || n == 0)) return -1.0f;
    scl_ml_simd_init();

    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff = (double)y_true[i] - (double)y_pred[i];
        sum += diff * diff;
    }
    return (float)(sum / (double)n);
}

float
scl_ml_mean_absolute_error(const SCL_ML_FLOAT *y_true,
                            const SCL_ML_FLOAT *y_pred, size_t n) {
    if (scl_unlikely(!y_true || !y_pred || n == 0)) return -1.0f;
    double sum = 0.0;
    for (size_t i = 0; i < n; i++)
        sum += (double)fabsf(y_true[i] - y_pred[i]);
    return (float)(sum / (double)n);
}

float
scl_ml_r2_score(const SCL_ML_FLOAT *y_true,
                 const SCL_ML_FLOAT *y_pred, size_t n) {
    if (scl_unlikely(!y_true || !y_pred || n == 0)) return -1.0f;

    double mean = 0.0;
    for (size_t i = 0; i < n; i++) mean += (double)y_true[i];
    mean /= (double)n;

    double ss_res = 0.0, ss_tot = 0.0;
    for (size_t i = 0; i < n; i++) {
        double diff_res = (double)y_true[i] - (double)y_pred[i];
        double diff_tot = (double)y_true[i] - mean;
        ss_res += diff_res * diff_res;
        ss_tot += diff_tot * diff_tot;
    }
    if (ss_tot < FLT_MIN) return 1.0f;
    return (float)(1.0 - ss_res / ss_tot);
}

float
scl_ml_log_loss(const SCL_ML_FLOAT *y_true,
                 const SCL_ML_FLOAT *y_pred_proba, size_t n) {
    if (scl_unlikely(!y_true || !y_pred_proba || n == 0)) return -1.0f;

    double loss = 0.0;
    for (size_t i = 0; i < n; i++) {
        float p = y_pred_proba[i];
        if (p < FLT_MIN) p = FLT_MIN;
        if (p > 1.0f) p = 1.0f;
        loss += (double)y_true[i] * logf(p) + (double)(1.0f - y_true[i]) * logf(1.0f - p);
    }
    return (float)(-loss / (double)n);
}

float
scl_ml_accuracy(const float *y_true, const float *y_pred, size_t n) {
    if (scl_unlikely(!y_true || !y_pred || n == 0)) return 0.0f;
    size_t correct = 0;
    for (size_t i = 0; i < n; i++)
        if (y_true[i] == y_pred[i]) correct++;
    return (float)correct / (float)n;
}
