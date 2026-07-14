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

/* Portable SIMD dispatch layer for ML kernels.
 *
 * Architecture:
 *   - Dispatch table (scl_ml_simd_t) holds function pointers for every kernel.
 *   - At init time, scl_ml_cpu_features() detects the best ISA available and
 *     scl_ml_simd_init_impl() installs the matching override.
 *   - Priority (highest first): AVX-512F+BW+DQ > AVX2+FMA > SSE4.2 > SVE > NEON
 * > Scalar
 *
 * Every kernel has a scalar fallback so the table is always fully populated.
 * Per-ISA modules (scl_ml_simd_{avx2,avx512,neon,sve}.c) override the slots
 * they can accelerate; unaccelerated slots retain the scalar implementation.
 *
 * All "float" kernels operate on float arrays; "double" variants use double.
 * "n" is element count, "d" is dimensionality, alignment is 32 bytes.
 */

#ifndef SCL_ML_SIMD_H
#define SCL_ML_SIMD_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_ml.h"
#include "scl_stddef.h"
#include "scl_stdint.h"

/* ══════════════════════════════════════════════════════════════════
 * CPU feature flags
 * ══════════════════════════════════════════════════════════════════ */
typedef enum {
  SCL_ML_CPU_SSE42 = 1u << 0,    /* x86 SSE4.2 */
  SCL_ML_CPU_AVX2 = 1u << 1,     /* x86 AVX2 + FMA */
  SCL_ML_CPU_AVX512F = 1u << 2,  /* x86 AVX-512 Foundation */
  SCL_ML_CPU_AVX512BW = 1u << 3, /* x86 AVX-512 Byte/Word */
  SCL_ML_CPU_AVX512DQ = 1u << 4, /* x86 AVX-512 DoubleWord/QuadWord */
  SCL_ML_CPU_NEON = 1u << 5,     /* ARM64 NEON (ASIMD) */
  SCL_ML_CPU_SVE = 1u << 6,      /* ARM64 SVE / SVE2 */
} scl_ml_cpu_feature_t;

/* Detect available CPU SIMD features at runtime.
 * Uses CPUID on x86-64 (leaf 1/7), getauxval(AT_HWCAP) on ARM64 Linux,
 * sysctl on macOS. Returns bitmask of scl_ml_cpu_feature_t. */
uint64_t scl_ml_cpu_features(void);

/* ══════════════════════════════════════════════════════════════════
 * Dispatch table — one entry per kernel
 * ══════════════════════════════════════════════════════════════════ */
typedef struct scl_ml_simd {
  /* ── BLAS Level 1: vector–vector ───────────────────────────── */
  float (*dot_f)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
                 size_t n);
  float (*dot)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
               size_t n);
  float (*norm_l2_sq)(const float *SCL_RESTRICT x, size_t n);
  float (*norm_l2)(const float *SCL_RESTRICT x, size_t n);
  float (*norm_l1)(const float *SCL_RESTRICT x, size_t n);
  void (*axpy)(float *SCL_RESTRICT y, float alpha, const float *SCL_RESTRICT x,
               size_t n);
  void (*axpby)(float *SCL_RESTRICT z, float a, const float *SCL_RESTRICT x,
                float b, const float *SCL_RESTRICT y, size_t n);

  /* ── Reductions ───────────────────────────────────────────── */
  float (*sum)(const float *SCL_RESTRICT x, size_t n);
  float (*max)(const float *SCL_RESTRICT x, size_t n);
  float (*min)(const float *SCL_RESTRICT x, size_t n);
  size_t (*argmax)(const float *SCL_RESTRICT x, size_t n);
  size_t (*argmin)(const float *SCL_RESTRICT x, size_t n);
  void (*argminmax)(const float *SCL_RESTRICT x, size_t n, size_t *argmin_out,
                    size_t *argmax_out);

  /* ── Element-wise: vector–vector → vector ────────────────── */
  void (*add)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
              const float *SCL_RESTRICT b, size_t n);
  void (*sub)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
              const float *SCL_RESTRICT b, size_t n);
  void (*mul)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
              const float *SCL_RESTRICT b, size_t n);
  void (*div)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
              const float *SCL_RESTRICT b, size_t n);
  void (*abs)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);
  void (*fma)(float *SCL_RESTRICT z, const float *SCL_RESTRICT a,
              const float *SCL_RESTRICT b, size_t n);

  /* ── Element-wise: scalar–vector → vector ────────────────── */
  void (*add_s)(float *SCL_RESTRICT z, const float *SCL_RESTRICT x, float s,
                size_t n);
  void (*mul_s)(float *SCL_RESTRICT z, const float *SCL_RESTRICT x, float s,
                size_t n);
  void (*scale_add_s)(float *SCL_RESTRICT z, float alpha,
                      const float *SCL_RESTRICT x, float beta, size_t n);

  /* ── Activations (in-place safe: out may alias in) ───────── */
  void (*sigmoid)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                  size_t n);
  void (*relu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);
  void (*relu6)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                size_t n);
  void (*leaky_relu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                     float slope, size_t n);
  void (*elu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
              float alpha, size_t n);
  void (*tanh_fast)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                    size_t n);
  void (*gelu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);
  void (*silu)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);

  /* ── Softmax family ──────────────────────────────────────── */
  void (*softmax)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                  size_t n);
  void (*log_softmax)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                      size_t n);

  /* ── Element-wise unary math ──────────────────────────────── */
  void (*vexp)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);
  void (*vlog)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);
  void (*vsqrt)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                size_t n);
  void (*vrsqrt)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                 size_t n);
  void (*vinv)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, size_t n);

  /* ── Distance: vector–vector → scalar ────────────────────── */
  float (*dist_l2_sq)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
                      size_t d);
  float (*dist_l1)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
                   size_t d);
  float (*dist_cos)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
                    size_t d);
  float (*dist_cheb)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
                     size_t d);
  float (*dist_l2)(const float *SCL_RESTRICT a, const float *SCL_RESTRICT b,
                   size_t d);

  /* ── Distance matrix: batch computation ──────────────────── */
  void (*dist_matrix_l2_sq)(float *SCL_RESTRICT out,
                            const float *SCL_RESTRICT a,
                            const float *SCL_RESTRICT b, size_t n, size_t m,
                            size_t d);
  void (*dist_matrix_cos)(float *SCL_RESTRICT out, const float *SCL_RESTRICT a,
                          const float *SCL_RESTRICT b, size_t n, size_t m,
                          size_t d);
  void (*dist_matrix_l1)(float *SCL_RESTRICT out, const float *SCL_RESTRICT a,
                         const float *SCL_RESTRICT b, size_t n, size_t m,
                         size_t d);

  /* ── BLAS Level 2: matrix–vector ─────────────────────────── */
  void (*gemv)(float *SCL_RESTRICT y, const float *SCL_RESTRICT a,
               const float *SCL_RESTRICT x, size_t m, size_t n, float beta);
  void (*gemv_t)(float *SCL_RESTRICT y, const float *SCL_RESTRICT a,
                 const float *SCL_RESTRICT x, size_t m, size_t n, float beta);

  /* ── BLAS Level 3: matrix–matrix ─────────────────────────── */
  void (*gemm)(float *SCL_RESTRICT c, const float *SCL_RESTRICT a,
               const float *SCL_RESTRICT b, size_t m, size_t n, size_t k,
               float alpha, float beta);

  /* ── Comparison / Selection ──────────────────────────────── */
  void (*threshold)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                    float t, size_t n);
  void (*threshold_sign)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in,
                         float t, size_t n);

  /* ── Hamming ─────────────────────────────────────────────── */
  float (*hamming)(const uint32_t *SCL_RESTRICT a,
                   const uint32_t *SCL_RESTRICT b, size_t n_words);

  /* ── Scalar fast math (for inner loops) ──────────────────── */
  float (*sigmoid_f)(float x);
  float (*tanh_f)(float x);
  float (*exp_f)(float x);

  /* ── Top-K selection ─────────────────────────────────────── */
  void (*topk_indices)(const float *SCL_RESTRICT vals,
                       uint32_t *SCL_RESTRICT indices, size_t n, size_t k);

  /* ── Clamp ───────────────────────────────────────────────── */
  void (*clamp)(float *SCL_RESTRICT out, const float *SCL_RESTRICT in, float lo,
                float hi, size_t n);

} scl_ml_simd_t;

/* Global dispatch table — safe to read from any thread after init. */
extern scl_ml_simd_t scl_ml_simd;

/* Initialize the dispatch table. Idempotent / thread-safe (uses scl_once). */
void scl_ml_simd_init(void);

/* ══════════════════════════════════════════════════════════════════
 * Shared SIMD helpers (used across ISA backends)
 * ══════════════════════════════════════════════════════════════════ */

/* Horizontal sum of a float array using pairwise addition. */
static inline float scl_simd_hsum_f32(const float *x, size_t n) {
  float acc = 0.0f;
  size_t i = 0;
  for (; i + 8 <= n; i += 8)
    acc += x[i] + x[i + 1] + x[i + 2] + x[i + 3] + x[i + 4] + x[i + 5] +
           x[i + 6] + x[i + 7];
  for (; i < n; i++)
    acc += x[i];
  return acc;
}

/* ══════════════════════════════════════════════════════════════════
 * Per-ISA override functions
 * ══════════════════════════════════════════════════════════════════ */
void scl_ml_simd_override_scalar(scl_ml_simd_t *t);
void scl_ml_simd_override_sse42(scl_ml_simd_t *t);
void scl_ml_simd_override_avx2(scl_ml_simd_t *t);
void scl_ml_simd_override_avx512(scl_ml_simd_t *t);
void scl_ml_simd_override_neon(scl_ml_simd_t *t);
void scl_ml_simd_override_sve(scl_ml_simd_t *t);

/* Shared helper: scalar top-k used by NEON and other ISAs. */
void scl_ml_simd_scalar_topk_indices(const float *SCL_RESTRICT vals,
                                     uint32_t *SCL_RESTRICT indices, size_t n,
                                     size_t k);

/* ══════════════════════════════════════════════════════════════════
 * Inline helpers
 * ══════════════════════════════════════════════════════════════════ */

/* Fast exponent approximation: Schraudolph's method.
 * e^x ≈ 2^(log2(e)·x) via IEEE-754 bit hack. ~1e-4 rel error for x∈[-88,88]. */
static inline float scl_ml_fast_exp(float x) {
  if (scl_unlikely(x < -88.0f))
    return 0.0f;
  if (scl_unlikely(x > 88.0f))
    return INFINITY;
  union {
    float f;
    int32_t i;
  } u;
  u.i = (int32_t)(12102203.0f * x + 1065353216.0f);
  return u.f;
}

static inline float scl_ml_fast_sigmoid(float x) {
  if (scl_unlikely(x < -30.0f))
    return 0.0f;
  if (scl_unlikely(x > 30.0f))
    return 1.0f;
  float ex = scl_ml_fast_exp(-x);
  return 1.0f / (1.0f + ex);
}

static inline float scl_ml_fast_tanh(float x) {
  if (scl_unlikely(x < -10.0f))
    return -1.0f;
  if (scl_unlikely(x > 10.0f))
    return 1.0f;
  return 2.0f * scl_ml_fast_sigmoid(2.0f * x) - 1.0f;
}

static inline bool scl_ml_is_pow2(size_t v) { return v && !(v & (v - 1)); }
static inline size_t scl_ml_align_up(size_t v, size_t a) {
  return (v + a - 1) & ~(a - 1);
}

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif /* SCL_ML_SIMD_H */