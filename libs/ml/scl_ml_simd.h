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

/* Portable SIMD dispatch table for ML kernels.
 *
 * Each kernel operation has a function pointer in scl_ml_simd_t.
 * At init time (scl_ml_simd_init), CPU features are detected and
 * the best available implementation is selected:
 *   AVX-512 > AVX2+FMA > SSE4.2 > NEON > Scalar fallback
 *
 * All kernels operate on float (32-bit) arrays unless noted.
 * "n" is element count, "d" is dimensionality.
 * Alignment: 32 bytes (AVX2 requirement).
 */

#ifndef SCL_ML_SIMD_H
#define SCL_ML_SIMD_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"
#include <stdint.h>
#include <stddef.h>

/* ── CPU feature detection ───────────────────────────────────── */
typedef enum {
    SCL_ML_CPU_SSE42    = 1u << 0,
    SCL_ML_CPU_AVX2     = 1u << 1,
    SCL_ML_CPU_AVX512F  = 1u << 2,
    SCL_ML_CPU_AVX512BW = 1u << 3,
    SCL_ML_CPU_AVX512DQ = 1u << 4,
    SCL_ML_CPU_NEON     = 1u << 5,
    SCL_ML_CPU_SVE      = 1u << 6
} scl_ml_cpu_feature_t;

/* Detect available CPU SIMD features at runtime.
 * Uses CPUID on x86-64 (leaf 1, leaf 7), getauxval(AT_HWCAP) on ARM64,
 * sysctl on macOS. Returns bitmask of scl_ml_cpu_feature_t. */
uint64_t scl_ml_cpu_features(void);

/* ── Kernel dispatch table ─────────────────────────────────────
 * All functions process float arrays. 32-byte aligned pointers assumed. */
typedef struct scl_ml_simd {
    /* ── BLAS Level 1: vector-vector ─────────────────────────── */
    /* dot product with f64 accumulator (numerical stability for large N) */
    float   (*dot_f)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b, size_t n);
    float   (*dot)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b, size_t n);
    float   (*norm_l2_sq)(const float *SCL_RESTRICT x, size_t n);
    float   (*norm_l2)(const float *SCL_RESTRICT x, size_t n);
    float   (*norm_l1)(const float *SCL_RESTRICT x, size_t n);
    void    (*axpy)(float *SCL_RESTRICT y, float alpha,
                    const float *SCL_RESTRICT x, size_t n);
    void    (*axpby)(float *SCL_RESTRICT z, float a,
                     const float *SCL_RESTRICT x, float b,
                     const float *SCL_RESTRICT y, size_t n);

    /* ── Reductions ──────────────────────────────────────────── */
    float   (*sum)(const float *SCL_RESTRICT x, size_t n);
    float   (*max)(const float *SCL_RESTRICT x, size_t n);
    float   (*min)(const float *SCL_RESTRICT x, size_t n);
    size_t  (*argmax)(const float *SCL_RESTRICT x, size_t n);
    size_t  (*argmin)(const float *SCL_RESTRICT x, size_t n);

    /* ── Element-wise: vector-vector → vector ────────────────── */
    void    (*add)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                   const float *SCL_RESTRICT b, size_t n);
    void    (*sub)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                   const float *SCL_RESTRICT b, size_t n);
    void    (*mul)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                   const float *SCL_RESTRICT b, size_t n);
    void    (*div)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                   const float *SCL_RESTRICT b, size_t n);
    /* fused multiply-add: z[i] = a[i] * b[i] + z[i] */
    void    (*fma)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
                   const float *SCL_RESTRICT b, size_t n);

    /* ── Element-wise: scalar-vector → vector ────────────────── */
    void    (*add_s)(float *SCL_RESTRICT z, const float *SCL_RESTRICT x,
                     float s, size_t n);
    void    (*mul_s)(float *SCL_RESTRICT z, const float *SCL_RESTRICT x,
                     float s, size_t n);

    /* ── Activations (in-place safe: out may alias in) ───────── */
    /* fast sigmoid via polynomial approximation of exp(x) */
    void    (*sigmoid)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                       size_t n);
    void    (*relu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                    size_t n);
    void    (*relu6)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                     size_t n);
    /* fast tanh via min-max rational approximation */
    void    (*tanh_fast)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                         size_t n);
    void    (*gelu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                    size_t n);
    /* numerically stable softmax: out[i] = exp(in[i] - max) / sum(exp(in - max)) */
    void    (*softmax)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                       size_t n);
    void    (*log_softmax)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                           size_t n);
    /* fast exp (Schraudolph approximation, ~1e-4 rel error) */
    void    (*vexp)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                    size_t n);
    /* natural log */
    void    (*vlog)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                    size_t n);
    /* sqrt */
    void    (*vsqrt)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                     size_t n);

    /* ── Distance: vector-vector → scalar ────────────────────── */
    float   (*dist_l2_sq)(const float *SCL_RESTRICT a,
                          const float *SCL_RESTRICT b, size_t d);
    float   (*dist_l1)(const float *SCL_RESTRICT a,
                       const float *SCL_RESTRICT b, size_t d);
    float   (*dist_cos)(const float *SCL_RESTRICT a,
                        const float *SCL_RESTRICT b, size_t d);
    float   (*dist_cheb)(const float *SCL_RESTRICT a,
                         const float *SCL_RESTRICT b, size_t d);

    /* ── Distance matrix: batch computation ────────────────────
     * out[i*m + j] = dist(a[i*d .. i*d+d], b[j*d .. j*d+d], d)
     * All pointers 32-byte aligned. */
    void    (*dist_matrix_l2_sq)(float *SCL_RESTRICT out,
                                  const float *SCL_RESTRICT a,
                                  const float *SCL_RESTRICT b,
                                  size_t n, size_t m, size_t d);
    void    (*dist_matrix_cos)(float *SCL_RESTRICT out,
                                const float *SCL_RESTRICT a,
                                const float *SCL_RESTRICT b,
                                size_t n, size_t m, size_t d);
    void    (*dist_matrix_l1)(float *SCL_RESTRICT out,
                               const float *SCL_RESTRICT a,
                               const float *SCL_RESTRICT b,
                               size_t n, size_t m, size_t d);

    /* ── BLAS Level 2: matrix-vector ───────────────────────────
     * Row-major matrices: A is [m x n] stored row-contiguous.
     * y = beta * y + A @ x   (gemv)
     * y = beta * y + A^T @ x (gemv_t) */
    void    (*gemv)(float *SCL_RESTRICT y, const float *SCL_RESTRICT a,
                    const float *SCL_RESTRICT x,
                    size_t m, size_t n, float beta);
    void    (*gemv_t)(float *SCL_RESTRICT y, const float *SCL_RESTRICT a,
                      const float *SCL_RESTRICT x,
                      size_t m, size_t n, float beta);

    /* ── BLAS Level 3: matrix-matrix ───────────────────────────
     * C = alpha * A @ B + beta * C
     * A is [m x k], B is [k x n], C is [m x n], all row-major */
    void    (*gemm)(float *SCL_RESTRICT c, const float *SCL_RESTRICT a,
                    const float *SCL_RESTRICT b,
                    size_t m, size_t n, size_t k,
                    float alpha, float beta);

    /* ── Comparison / Selection ──────────────────────────────── */
    /* out[i] = in[i] > t ? 1.0f : 0.0f */
    void    (*threshold)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                         float t, size_t n);
    /* argmin + argmax combined (returns argmin, sets *argmax_out) */
    size_t  (*argminmax)(const float *SCL_RESTRICT x, size_t n,
                          size_t *argmax_out);
    /* Hamming distance between two uint32 bit arrays */
    float   (*hamming)(const uint32_t *SCL_RESTRICT a,
                       const uint32_t *SCL_RESTRICT b, size_t n_words);

    /* ── Scalar fast math (used in SMO inner loop) ───────────── */
    float   (*sigmoid_f)(float x);
    float   (*tanh_f)(float x);
    float   (*exp_f)(float x);

    /* ── Top-K selection ───────────────────────────────────────
     * Find k largest values in vals, write their indices to indices[0..k-1].
     * Uses partial heap sort. */
    void    (*topk_indices)(const float *SCL_RESTRICT vals,
                            uint32_t *SCL_RESTRICT indices,
                            size_t n, size_t k);
} scl_ml_simd_t;

/* ── Global dispatch table ───────────────────────────────────── */
extern scl_ml_simd_t scl_ml_simd;

/* Initialize dispatch table with best available CPU implementation.
 * Safe to call multiple times (checks if already initialized).
 * Selects highest available ISA: AVX-512 > AVX2+FMA > SSE4.2 > NEON > Scalar */
void scl_ml_simd_init(void);

/* ── Override helpers for target-specific compilation units ────
 * Each scl_ml_simd_<isa>.c defines an override function that selectively
 * replaces entries in the table. Un-overridden entries keep scalar fallback. */
void scl_ml_simd_override_scalar(scl_ml_simd_t *t);
void scl_ml_simd_override_avx2(scl_ml_simd_t *t);
void scl_ml_simd_override_avx512(scl_ml_simd_t *t);
void scl_ml_simd_override_sse42(scl_ml_simd_t *t);
void scl_ml_simd_override_neon(scl_ml_simd_t *t);

/* ── Fast exponent approximation (inline, header-only) ────────
 * Schraudolph's method: e^x ≈ 2^(log2(e) * x) using IEEE-754 bit hack.
 * ~20x faster than math.h exp, ~1e-4 relative error for x in [-88, 88]. */
static inline float
scl_ml_fast_exp(float x) {
    if (scl_unlikely(x < -88.0f)) return 0.0f;
    if (scl_unlikely(x >  88.0f)) return INFINITY;
    union { float f; int32_t i; } u;
    u.i = (int32_t)(12102203.0f * x + 1065353216.0f);
    return u.f;
}

/* Fast sigmoid using fast_exp */
static inline float
scl_ml_fast_sigmoid(float x) {
    if (scl_unlikely(x < -30.0f)) return 0.0f;
    if (scl_unlikely(x >  30.0f)) return 1.0f;
    float ex = scl_ml_fast_exp(-x);
    return 1.0f / (1.0f + ex);
}

/* Fast tanh using sigmoid: tanh(x) = 2*sigmoid(2x) - 1 */
static inline float
scl_ml_fast_tanh(float x) {
    if (scl_unlikely(x < -10.0f)) return -1.0f;
    if (scl_unlikely(x >  10.0f)) return  1.0f;
    return 2.0f * scl_ml_fast_sigmoid(2.0f * x) - 1.0f;
}

/* Check if a value is a power of 2 */
static inline bool
scl_ml_is_pow2(size_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

/* Align size up to alignment (must be power of 2) */
static inline size_t
scl_ml_align_up(size_t v, size_t align) {
    return (v + align - 1) & ~(align - 1);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_SIMD_H */
