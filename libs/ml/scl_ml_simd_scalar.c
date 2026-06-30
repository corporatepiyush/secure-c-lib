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

/* Scalar (non-SIMD) fallback implementations for all ML kernels.
 * These are always compiled and serve as the baseline. The compiler
 * may auto-vectorize these with -O2/-O3; the hand-coded SIMD overrides
 * in scl_ml_simd_avx2.c etc are used when runtime dispatch selects them. */

#include "scl_ml_simd.h"
#include <string.h>
#include <math.h>
#include <float.h>

/* ═══════════════════════════════════════════════════════════════
 * BLAS Level 1: dot products
 * ═══════════════════════════════════════════════════════════════ */

float scl_ml_simd_scalar_dot_f(const float *SCL_RESTRICT a,
                                 const float *SCL_RESTRICT b,
                                 size_t n) {
    double acc = 0.0;
    for (size_t i = 0; i < n; i++)
        acc += (double)a[i] * (double)b[i];
    return (float)acc;
}

float scl_ml_simd_scalar_dot(const float *SCL_RESTRICT a,
                              const float *SCL_RESTRICT b,
                              size_t n) {
    float acc = 0.0f;
    for (size_t i = 0; i < n; i++)
        acc += a[i] * b[i];
    return acc;
}

float scl_ml_simd_scalar_norm_l2_sq(const float *SCL_RESTRICT x, size_t n) {
    float acc = 0.0f;
    for (size_t i = 0; i < n; i++)
        acc += x[i] * x[i];
    return acc;
}

float scl_ml_simd_scalar_norm_l2(const float *SCL_RESTRICT x, size_t n) {
    return sqrtf(scl_ml_simd_scalar_norm_l2_sq(x, n));
}

float scl_ml_simd_scalar_norm_l1(const float *SCL_RESTRICT x, size_t n) {
    float acc = 0.0f;
    for (size_t i = 0; i < n; i++)
        acc += fabsf(x[i]);
    return acc;
}

void scl_ml_simd_scalar_axpy(float *SCL_RESTRICT y, float alpha,
                               const float *SCL_RESTRICT x, size_t n) {
    for (size_t i = 0; i < n; i++)
        y[i] += alpha * x[i];
}

void scl_ml_simd_scalar_axpby(float *SCL_RESTRICT z, float a,
                                const float *SCL_RESTRICT x, float b,
                                const float *SCL_RESTRICT y, size_t n) {
    for (size_t i = 0; i < n; i++)
        z[i] = a * x[i] + b * y[i];
}

/* ═══════════════════════════════════════════════════════════════
 * Reductions
 * ═══════════════════════════════════════════════════════════════ */

float scl_ml_simd_scalar_sum(const float *SCL_RESTRICT x, size_t n) {
    double acc = 0.0;
    for (size_t i = 0; i < n; i++)
        acc += (double)x[i];
    return (float)acc;
}

float scl_ml_simd_scalar_max(const float *SCL_RESTRICT x, size_t n) {
    float m = -FLT_MAX;
    for (size_t i = 0; i < n; i++)
        if (x[i] > m) m = x[i];
    return m;
}

float scl_ml_simd_scalar_min(const float *SCL_RESTRICT x, size_t n) {
    float m = FLT_MAX;
    for (size_t i = 0; i < n; i++)
        if (x[i] < m) m = x[i];
    return m;
}

size_t scl_ml_simd_scalar_argmax(const float *SCL_RESTRICT x, size_t n) {
    size_t idx = 0;
    float  m   = x[0];
    for (size_t i = 1; i < n; i++) {
        if (x[i] > m) { m = x[i]; idx = i; }
    }
    return idx;
}

size_t scl_ml_simd_scalar_argmin(const float *SCL_RESTRICT x, size_t n) {
    size_t idx = 0;
    float  m   = x[0];
    for (size_t i = 1; i < n; i++) {
        if (x[i] < m) { m = x[i]; idx = i; }
    }
    return idx;
}

/* ═══════════════════════════════════════════════════════════════
 * Element-wise: vector-vector
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_add(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                              const float *SCL_RESTRICT b, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] = a[i] + b[i];
}

void scl_ml_simd_scalar_sub(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                              const float *SCL_RESTRICT b, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] = a[i] - b[i];
}

void scl_ml_simd_scalar_mul(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                              const float *SCL_RESTRICT b, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] = a[i] * b[i];
}

void scl_ml_simd_scalar_div(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                              const float *SCL_RESTRICT b, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] = a[i] / b[i];
}

void scl_ml_simd_scalar_fma(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                              const float *SCL_RESTRICT b, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] += a[i] * b[i];
}

/* ═══════════════════════════════════════════════════════════════
 * Element-wise: scalar-vector
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_add_s(float *SCL_RESTRICT z, const float *SCL_RESTRICT x,
                                float s, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] = x[i] + s;
}

void scl_ml_simd_scalar_mul_s(float *SCL_RESTRICT z, const float *SCL_RESTRICT x,
                                float s, size_t n) {
    for (size_t i = 0; i < n; i++) z[i] = x[i] * s;
}

/* ═══════════════════════════════════════════════════════════════
 * Activations
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_sigmoid(float *SCL_RESTRICT out,
                                  const float *SCL_RESTRICT in,
                                  size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = scl_ml_fast_sigmoid(in[i]);
}

void scl_ml_simd_scalar_relu(float *SCL_RESTRICT out,
                               const float *SCL_RESTRICT in,
                               size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = in[i] > 0.0f ? in[i] : 0.0f;
}

void scl_ml_simd_scalar_relu6(float *SCL_RESTRICT out,
                                const float *SCL_RESTRICT in,
                                size_t n) {
    for (size_t i = 0; i < n; i++) {
        float v = in[i];
        out[i] = v < 0.0f ? 0.0f : (v > 6.0f ? 6.0f : v);
    }
}

void scl_ml_simd_scalar_tanh_fast(float *SCL_RESTRICT out,
                                    const float *SCL_RESTRICT in,
                                    size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = scl_ml_fast_tanh(in[i]);
}

void scl_ml_simd_scalar_gelu(float *SCL_RESTRICT out,
                               const float *SCL_RESTRICT in,
                               size_t n) {
    /* GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3))) */
    const float sqrt_2_over_pi = 0.7978845608028654f;
    const float c = 0.044715f;
    for (size_t i = 0; i < n; i++) {
        float x = in[i];
        float x3 = x * x * x;
        float inner = tanhf(sqrt_2_over_pi * (x + c * x3));
        out[i] = 0.5f * x * (1.0f + inner);
    }
}

static void scl_ml_scalar_softmax_impl(float *SCL_RESTRICT out,
                                        const float *SCL_RESTRICT in,
                                        size_t n, int do_log) {
    /* max-subtraction trick for numerical stability */
    float maxv = in[0];
    for (size_t i = 1; i < n; i++)
        if (in[i] > maxv) maxv = in[i];

    double sum = 0.0;
    for (size_t i = 0; i < n; i++) {
        float e = scl_ml_fast_exp(in[i] - maxv);
        out[i] = e;
        sum += (double)e;
    }

    float inv_sum = (float)(1.0 / sum);
    if (do_log) {
        float log_inv_sum = logf(inv_sum);
        for (size_t i = 0; i < n; i++)
            out[i] = logf(out[i]) + log_inv_sum;
    } else {
        for (size_t i = 0; i < n; i++)
            out[i] *= inv_sum;
    }
}

void scl_ml_simd_scalar_softmax(float *SCL_RESTRICT out,
                                  const float *SCL_RESTRICT in,
                                  size_t n) {
    scl_ml_scalar_softmax_impl(out, in, n, 0);
}

void scl_ml_simd_scalar_log_softmax(float *SCL_RESTRICT out,
                                      const float *SCL_RESTRICT in,
                                      size_t n) {
    scl_ml_scalar_softmax_impl(out, in, n, 1);
}

void scl_ml_simd_scalar_vexp(float *SCL_RESTRICT out,
                               const float *SCL_RESTRICT in,
                               size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = scl_ml_fast_exp(in[i]);
}

void scl_ml_simd_scalar_vlog(float *SCL_RESTRICT out,
                               const float *SCL_RESTRICT in,
                               size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = in[i] > 0.0f ? logf(in[i]) : -FLT_MAX;
}

void scl_ml_simd_scalar_vsqrt(float *SCL_RESTRICT out,
                                const float *SCL_RESTRICT in,
                                size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = in[i] >= 0.0f ? sqrtf(in[i]) : 0.0f;
}

/* ═══════════════════════════════════════════════════════════════
 * Distance: vector-vector → scalar
 * ═══════════════════════════════════════════════════════════════ */

float scl_ml_simd_scalar_dist_l2_sq(const float *SCL_RESTRICT a,
                                      const float *SCL_RESTRICT b,
                                      size_t d) {
    float acc = 0.0f;
    for (size_t i = 0; i < d; i++) {
        float diff = a[i] - b[i];
        acc += diff * diff;
    }
    return acc;
}

float scl_ml_simd_scalar_dist_l1(const float *SCL_RESTRICT a,
                                   const float *SCL_RESTRICT b,
                                   size_t d) {
    float acc = 0.0f;
    for (size_t i = 0; i < d; i++)
        acc += fabsf(a[i] - b[i]);
    return acc;
}

float scl_ml_simd_scalar_dist_cos(const float *SCL_RESTRICT a,
                                    const float *SCL_RESTRICT b,
                                    size_t d) {
    double dot = 0.0, na = 0.0, nb = 0.0;
    for (size_t i = 0; i < d; i++) {
        dot += (double)a[i] * (double)b[i];
        na  += (double)a[i] * (double)a[i];
        nb  += (double)b[i] * (double)b[i];
    }
    double denom = sqrt(na * nb);
    if (denom < FLT_MIN) return 1.0f;
    return (float)(1.0 - dot / denom);
}

float scl_ml_simd_scalar_dist_cheb(const float *SCL_RESTRICT a,
                                     const float *SCL_RESTRICT b,
                                     size_t d) {
    float maxv = 0.0f;
    for (size_t i = 0; i < d; i++) {
        float diff = fabsf(a[i] - b[i]);
        if (diff > maxv) maxv = diff;
    }
    return maxv;
}

/* ═══════════════════════════════════════════════════════════════
 * Distance matrix: batch
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_dist_matrix_l2_sq(float *SCL_RESTRICT out,
                                            const float *SCL_RESTRICT a,
                                            const float *SCL_RESTRICT b,
                                            size_t n, size_t m, size_t d) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < m; j++) {
            float acc = 0.0f;
            for (size_t k = 0; k < d; k++) {
                float diff = a[i * d + k] - b[j * d + k];
                acc += diff * diff;
            }
            out[i * m + j] = acc;
        }
    }
}

void scl_ml_simd_scalar_dist_matrix_cos(float *SCL_RESTRICT out,
                                          const float *SCL_RESTRICT a,
                                          const float *SCL_RESTRICT b,
                                          size_t n, size_t m, size_t d) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < m; j++) {
            out[i * m + j] = scl_ml_simd_scalar_dist_cos(&a[i * d], &b[j * d], d);
        }
    }
}

void scl_ml_simd_scalar_dist_matrix_l1(float *SCL_RESTRICT out,
                                         const float *SCL_RESTRICT a,
                                         const float *SCL_RESTRICT b,
                                         size_t n, size_t m, size_t d) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < m; j++) {
            float acc = 0.0f;
            for (size_t k = 0; k < d; k++)
                acc += fabsf(a[i * d + k] - b[j * d + k]);
            out[i * m + j] = acc;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * BLAS Level 2: matrix-vector
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_gemv(float *SCL_RESTRICT y,
                               const float *SCL_RESTRICT a,
                               const float *SCL_RESTRICT x,
                               size_t m, size_t n, float beta) {
    for (size_t i = 0; i < m; i++) {
        double acc = 0.0;
        for (size_t j = 0; j < n; j++)
            acc += (double)a[i * n + j] * (double)x[j];
        y[i] = beta * y[i] + (float)acc;
    }
}

void scl_ml_simd_scalar_gemv_t(float *SCL_RESTRICT y,
                                 const float *SCL_RESTRICT a,
                                 const float *SCL_RESTRICT x,
                                 size_t m, size_t n, float beta) {
    /* y = beta * y + A^T @ x
     * A is [m x n] row-major, so A^T columns are scattered.
     * This is the strided access pattern. We accumulate each y[j] separately. */
    for (size_t j = 0; j < n; j++)
        y[j] = beta * y[j];

    for (size_t i = 0; i < m; i++) {
        float xi = x[i];
        const float *row = &a[i * n];
        for (size_t j = 0; j < n; j++)
            y[j] += xi * row[j];
    }
}

/* ═══════════════════════════════════════════════════════════════
 * BLAS Level 3: matrix-matrix (simple tiled)
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_gemm(float *SCL_RESTRICT c,
                               const float *SCL_RESTRICT a,
                               const float *SCL_RESTRICT b,
                               size_t m, size_t n, size_t k,
                               float alpha, float beta) {
    /* C = alpha * A @ B + beta * C
     * A [m x k], B [k x n], C [m x n] all row-major.
     * Cache-tiled with tile size 32. */
    const size_t T = 32;

    for (size_t i = 0; i < m; i++) {
        for (size_t j = 0; j < n; j++)
            c[i * n + j] *= beta;
    }

    for (size_t i0 = 0; i0 < m; i0 += T) {
        size_t imax = i0 + T < m ? i0 + T : m;
        for (size_t j0 = 0; j0 < n; j0 += T) {
            size_t jmax = j0 + T < n ? j0 + T : n;
            for (size_t k0 = 0; k0 < k; k0 += T) {
                size_t kmax = k0 + T < k ? k0 + T : k;
                for (size_t i = i0; i < imax; i++) {
                    for (size_t j = j0; j < jmax; j++) {
                        double acc = 0.0;
                        for (size_t kk = k0; kk < kmax; kk++)
                            acc += (double)a[i * k + kk] * (double)b[kk * n + j];
                        c[i * n + j] += alpha * (float)acc;
                    }
                }
            }
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Comparison / Selection
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_scalar_threshold(float *SCL_RESTRICT out,
                                    const float *SCL_RESTRICT in,
                                    float t, size_t n) {
    for (size_t i = 0; i < n; i++)
        out[i] = in[i] > t ? 1.0f : 0.0f;
}

size_t scl_ml_simd_scalar_argminmax(const float *SCL_RESTRICT x, size_t n,
                                     size_t *argmax_out) {
    size_t imin = 0, imax = 0;
    float  vmin = x[0], vmax = x[0];
    for (size_t i = 1; i < n; i++) {
        if (x[i] < vmin) { vmin = x[i]; imin = i; }
        if (x[i] > vmax) { vmax = x[i]; imax = i; }
    }
    *argmax_out = imax;
    return imin;
}

float scl_ml_simd_scalar_hamming(const uint32_t *SCL_RESTRICT a,
                                   const uint32_t *SCL_RESTRICT b,
                                   size_t n_words) {
    uint32_t diff = 0;
    for (size_t i = 0; i < n_words; i++)
        diff += (uint32_t)__builtin_popcount(a[i] ^ b[i]);
    return (float)diff;
}

/* ═══════════════════════════════════════════════════════════════
 * Scalar fast math
 * ═══════════════════════════════════════════════════════════════ */

float scl_ml_simd_scalar_sigmoid_f(float x) {
    return scl_ml_fast_sigmoid(x);
}

float scl_ml_simd_scalar_tanh_f(float x) {
    return scl_ml_fast_tanh(x);
}

float scl_ml_simd_scalar_exp_f(float x) {
    return scl_ml_fast_exp(x);
}

/* ═══════════════════════════════════════════════════════════════
 * Top-K selection via heap
 * ═══════════════════════════════════════════════════════════════ */

static inline void scl_heap_sift_down(uint32_t *heap, const float *vals,
                                       size_t k, size_t idx) {
    float key = vals[heap[idx]];
    while (1) {
        size_t left = 2 * idx + 1;
        if (left >= k) break;
        size_t right = left + 1;
        size_t larger = (right < k && vals[heap[right]] > vals[heap[left]])
                        ? right : left;
        if (key >= vals[heap[larger]]) break;
        uint32_t tmp = heap[idx];
        heap[idx] = heap[larger];
        heap[larger] = tmp;
        idx = larger;
    }
}

void scl_ml_simd_scalar_topk_indices(const float *SCL_RESTRICT vals,
                                       uint32_t *SCL_RESTRICT indices,
                                       size_t n, size_t k) {
    if (k == 0 || n == 0) return;
    if (k > n) k = n;

    /* Build max-heap of first k elements */
    for (size_t i = 0; i < k; i++)
        indices[i] = (uint32_t)i;
    for (size_t i = k / 2; i > 0; i--)
        scl_heap_sift_down(indices, vals, k, i - 1);

    /* Process remaining elements */
    for (size_t i = k; i < n; i++) {
        if (vals[i] < vals[indices[0]]) {
            indices[0] = (uint32_t)i;
            scl_heap_sift_down(indices, vals, k, 0);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════
 * Override table (fill with scalar implementations)
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_override_scalar(scl_ml_simd_t *t) {
    t->dot_f          = scl_ml_simd_scalar_dot_f;
    t->dot            = scl_ml_simd_scalar_dot;
    t->norm_l2_sq     = scl_ml_simd_scalar_norm_l2_sq;
    t->norm_l2        = scl_ml_simd_scalar_norm_l2;
    t->norm_l1        = scl_ml_simd_scalar_norm_l1;
    t->axpy           = scl_ml_simd_scalar_axpy;
    t->axpby          = scl_ml_simd_scalar_axpby;
    t->sum            = scl_ml_simd_scalar_sum;
    t->max            = scl_ml_simd_scalar_max;
    t->min            = scl_ml_simd_scalar_min;
    t->argmax         = scl_ml_simd_scalar_argmax;
    t->argmin         = scl_ml_simd_scalar_argmin;
    t->add            = scl_ml_simd_scalar_add;
    t->sub            = scl_ml_simd_scalar_sub;
    t->mul            = scl_ml_simd_scalar_mul;
    t->div            = scl_ml_simd_scalar_div;
    t->fma            = scl_ml_simd_scalar_fma;
    t->add_s          = scl_ml_simd_scalar_add_s;
    t->mul_s          = scl_ml_simd_scalar_mul_s;
    t->sigmoid        = scl_ml_simd_scalar_sigmoid;
    t->relu           = scl_ml_simd_scalar_relu;
    t->relu6          = scl_ml_simd_scalar_relu6;
    t->tanh_fast      = scl_ml_simd_scalar_tanh_fast;
    t->gelu           = scl_ml_simd_scalar_gelu;
    t->softmax        = scl_ml_simd_scalar_softmax;
    t->log_softmax    = scl_ml_simd_scalar_log_softmax;
    t->vexp           = scl_ml_simd_scalar_vexp;
    t->vlog           = scl_ml_simd_scalar_vlog;
    t->vsqrt          = scl_ml_simd_scalar_vsqrt;
    t->dist_l2_sq     = scl_ml_simd_scalar_dist_l2_sq;
    t->dist_l1        = scl_ml_simd_scalar_dist_l1;
    t->dist_cos       = scl_ml_simd_scalar_dist_cos;
    t->dist_cheb      = scl_ml_simd_scalar_dist_cheb;
    t->dist_matrix_l2_sq = scl_ml_simd_scalar_dist_matrix_l2_sq;
    t->dist_matrix_cos   = scl_ml_simd_scalar_dist_matrix_cos;
    t->dist_matrix_l1    = scl_ml_simd_scalar_dist_matrix_l1;
    t->gemv           = scl_ml_simd_scalar_gemv;
    t->gemv_t         = scl_ml_simd_scalar_gemv_t;
    t->gemm           = scl_ml_simd_scalar_gemm;
    t->threshold      = scl_ml_simd_scalar_threshold;
    t->argminmax      = scl_ml_simd_scalar_argminmax;
    t->hamming        = scl_ml_simd_scalar_hamming;
    t->sigmoid_f      = scl_ml_simd_scalar_sigmoid_f;
    t->tanh_f         = scl_ml_simd_scalar_tanh_f;
    t->exp_f          = scl_ml_simd_scalar_exp_f;
    t->topk_indices   = scl_ml_simd_scalar_topk_indices;
}
