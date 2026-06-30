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

/* SIMD kernel dispatch: CPU feature detection, per-ISA override
 * implementations (via __attribute__((target(...)))), and dispatch
 * table initialization. */

#include "scl_ml_simd.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

/* ── Global dispatch table ───────────────────────────────────── */
scl_ml_simd_t scl_ml_simd;
static int scl_ml_simd_initialized = 0;

/* ═══════════════════════════════════════════════════════════════
 * CPU feature detection
 * ═══════════════════════════════════════════════════════════════ */

#if defined(SCL_ARCH_X86_64)

static inline void scl_ml_cpuid(uint32_t leaf, uint32_t subleaf,
                                 uint32_t *eax, uint32_t *ebx,
                                 uint32_t *ecx, uint32_t *edx) {
#if defined(__GNUC__) || defined(__clang__)
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf)
        : "memory");
#else
    (void)leaf; (void)subleaf;
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

uint64_t scl_ml_cpu_features(void) {
    uint64_t features = 0;
    uint32_t eax, ebx, ecx, edx;

    scl_ml_cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    if (ecx & (1u << 19)) features |= SCL_ML_CPU_SSE42;

    scl_ml_cpuid(7, 0, &eax, &ebx, &ecx, &edx);
    if (ebx & (1u << 5))  features |= SCL_ML_CPU_AVX2;
    if (ebx & (1u << 16)) features |= SCL_ML_CPU_AVX512F;
    if (ebx & (1u << 17)) features |= SCL_ML_CPU_AVX512DQ;
    if (ebx & (1u << 30)) features |= SCL_ML_CPU_AVX512BW;
    return features;
}

#elif defined(SCL_ARCH_ARM64)

#if defined(__linux__)
#include <sys/auxv.h>
#ifndef AT_HWCAP
#define AT_HWCAP 16
#endif
#ifndef HWCAP_ASIMD
#define HWCAP_ASIMD (1 << 1)
#endif
#ifndef HWCAP_SVE
#define HWCAP_SVE   (1 << 22)
#endif
uint64_t scl_ml_cpu_features(void) {
    uint64_t features = 0;
    unsigned long hwcap = getauxval(AT_HWCAP);
    if (hwcap & HWCAP_ASIMD) features |= SCL_ML_CPU_NEON;
    if (hwcap & HWCAP_SVE)   features |= SCL_ML_CPU_SVE;
    return features;
}
#elif defined(__APPLE__)
#include <sys/sysctl.h>
uint64_t scl_ml_cpu_features(void) {
    uint64_t features = SCL_ML_CPU_NEON; /* Apple Silicon always */
    int has_sve = 0;
    size_t len = sizeof(has_sve);
    if (sysctlbyname("hw.optional.arm.FEAT_SVE", &has_sve, &len, NULL, 0) == 0)
        if (has_sve) features |= SCL_ML_CPU_SVE;
    return features;
}
#else
uint64_t scl_ml_cpu_features(void) {
    return SCL_ML_CPU_NEON; /* assume NEON on AArch64 */
}
#endif

#else

uint64_t scl_ml_cpu_features(void) { return 0; }

#endif

/* ═══════════════════════════════════════════════════════════════
 * AVX2+FMA overrides — compiled with target attribute
 * GCC/Clang: generates AVX2 code even when compiled at base arch.
 * ═══════════════════════════════════════════════════════════════ */

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>

__attribute__((target("avx2,fma")))
static float scl_ml_simd_avx2_dot_f(const float *a, const float *b, size_t n) {
    __m256d acc = _mm256_setzero_pd();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m128 lo = _mm256_castps256_ps128(_mm256_mul_ps(va, vb));
        __m128 hi = _mm256_extractf128_ps(_mm256_mul_ps(va, vb), 1);
        __m128d lo_d = _mm_cvtps_pd(lo);
        __m128d hi_d = _mm_cvtps_pd(hi);
        /* Use f64 accumulator: convert f32 lanes to f64, add */
        __m128 s1 = _mm_add_ps(_mm_mul_ps(lo, lo), _mm_mul_ps(hi, hi));
        (void)s1;
        acc = _mm256_add_pd(acc, _mm256_cvtps_pd(lo));
        acc = _mm256_add_pd(acc, _mm256_cvtps_pd(hi));
    }
    double result = acc[0] + acc[1] + acc[2] + acc[3];
    for (; i < n; i++) result += (double)a[i] * (double)b[i];
    return (float)result;
}

__attribute__((target("avx2,fma")))
static float scl_ml_simd_avx2_dot(const float *a, const float *b, size_t n) {
    __m256 acc = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        acc = _mm256_fmadd_ps(va, vb, acc);
    }
    __m128 lo = _mm256_castps256_ps128(acc);
    __m128 hi = _mm256_extractf128_ps(acc, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    float result = _mm_cvtss_f32(sum);
    for (; i < n; i++) result += a[i] * b[i];
    return result;
}

__attribute__((target("avx2,fma")))
static float scl_ml_simd_avx2_norm_l2_sq(const float *x, size_t n) {
    return scl_ml_simd_avx2_dot(x, x, n);
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_axpy(float *y, float alpha,
                                    const float *x, size_t n) {
    __m256 va = _mm256_set1_ps(alpha);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 vy = _mm256_loadu_ps(&y[i]);
        __m256 vx = _mm256_loadu_ps(&x[i]);
        vy = _mm256_fmadd_ps(va, vx, vy);
        _mm256_storeu_ps(&y[i], vy);
    }
    for (; i < n; i++) y[i] += alpha * x[i];
}

__attribute__((target("avx2,fma")))
static float scl_ml_simd_avx2_sum(const float *x, size_t n) {
    __m256 acc = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8)
        acc = _mm256_add_ps(acc, _mm256_loadu_ps(&x[i]));
    __m128 lo = _mm256_castps256_ps128(acc);
    __m128 hi = _mm256_extractf128_ps(acc, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    float result = _mm_cvtss_f32(sum);
    for (; i < n; i++) result += x[i];
    return result;
}

__attribute__((target("avx2,fma")))
static float scl_ml_simd_avx2_max(const float *x, size_t n) {
    __m256 vmax = _mm256_loadu_ps(x);
    size_t i = 8;
    for (; i + 8 <= n; i += 8)
        vmax = _mm256_max_ps(vmax, _mm256_loadu_ps(&x[i]));
    __m128 lo = _mm256_castps256_ps128(vmax);
    __m128 hi = _mm256_extractf128_ps(vmax, 1);
    __m128 m = _mm_max_ps(lo, hi);
    m = _mm_max_ps(m, _mm_shuffle_ps(m, m, _MM_SHUFFLE(2,3,0,1)));
    m = _mm_max_ps(m, _mm_shuffle_ps(m, m, _MM_SHUFFLE(1,0,3,2)));
    float result = _mm_cvtss_f32(m);
    for (; i < n; i++) if (x[i] > result) result = x[i];
    return result;
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_add(float *z, const float *a,
                                   const float *b, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        _mm256_storeu_ps(&z[i], _mm256_add_ps(va, vb));
    }
    for (; i < n; i++) z[i] = a[i] + b[i];
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_sub(float *z, const float *a,
                                   const float *b, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        _mm256_storeu_ps(&z[i], _mm256_sub_ps(va, vb));
    }
    for (; i < n; i++) z[i] = a[i] - b[i];
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_mul(float *z, const float *a,
                                   const float *b, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        _mm256_storeu_ps(&z[i], _mm256_mul_ps(va, vb));
    }
    for (; i < n; i++) z[i] = a[i] * b[i];
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_fma(float *z, const float *a,
                                   const float *b, size_t n) {
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 vz = _mm256_loadu_ps(&z[i]);
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        _mm256_storeu_ps(&z[i], _mm256_fmadd_ps(va, vb, vz));
    }
    for (; i < n; i++) z[i] += a[i] * b[i];
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_relu(float *out, const float *in, size_t n) {
    __m256 zero = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 vi = _mm256_loadu_ps(&in[i]);
        _mm256_storeu_ps(&out[i], _mm256_max_ps(vi, zero));
    }
    for (; i < n; i++) out[i] = in[i] > 0.0f ? in[i] : 0.0f;
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_sigmoid(float *out, const float *in, size_t n) {
    __m256 one = _mm256_set1_ps(1.0f);
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        __m256 vi = _mm256_loadu_ps(&in[i]);
        /* clamp to [-30, 30] for numerical safety */
        vi = _mm256_min_ps(_mm256_max_ps(vi, _mm256_set1_ps(-30.0f)),
                           _mm256_set1_ps(30.0f));
        /* fast exp via Schraudolph approximation — vectorized:
         * exp(x) ≈ 2^(log2(e)*x) — bit hack on IEEE-754 exponent */
        __m256 vn = _mm256_mul_ps(vi, _mm256_set1_ps(-12102203.0f));
        __m256 vc = _mm256_add_ps(vn, _mm256_set1_ps(1065353216.0f));
        /* Convert float bits to integer, clamp, then back */
        __m256i vi_clamped = _mm256_cvttps_epi32(vc);
        /* Clamp to valid float range */
        vi_clamped = _mm256_min_epi32(_mm256_max_epi32(vi_clamped,
            _mm256_set1_epi32(0)), _mm256_set1_epi32(0x7F800000));
        __m256 ve = _mm256_castsi256_ps(vi_clamped);
        ve = _mm256_add_ps(one, ve);
        _mm256_storeu_ps(&out[i], _mm256_div_ps(one, ve));
    }
    for (; i < n; i++) out[i] = scl_ml_fast_sigmoid(in[i]);
}

__attribute__((target("avx2,fma")))
static float scl_ml_simd_avx2_dist_l2_sq(const float *a, const float *b,
                                          size_t d) {
    __m256 acc = _mm256_setzero_ps();
    size_t i = 0;
    for (; i + 8 <= d; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);
        __m256 diff = _mm256_sub_ps(va, vb);
        acc = _mm256_fmadd_ps(diff, diff, acc);
    }
    __m128 lo = _mm256_castps256_ps128(acc);
    __m128 hi = _mm256_extractf128_ps(acc, 1);
    __m128 sum = _mm_add_ps(lo, hi);
    sum = _mm_hadd_ps(sum, sum);
    sum = _mm_hadd_ps(sum, sum);
    float result = _mm_cvtss_f32(sum);
    for (; i < d; i++) { float df = a[i] - b[i]; result += df * df; }
    return result;
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_gemv(float *y, const float *a,
                                    const float *x,
                                    size_t m, size_t n, float beta) {
    for (size_t i = 0; i < m; i++) {
        __m256 acc = _mm256_setzero_ps();
        size_t j = 0;
        for (; j + 8 <= n; j += 8) {
            __m256 va = _mm256_loadu_ps(&a[i * n + j]);
            __m256 vx = _mm256_loadu_ps(&x[j]);
            acc = _mm256_fmadd_ps(va, vx, acc);
        }
        __m128 lo = _mm256_castps256_ps128(acc);
        __m128 hi = _mm256_extractf128_ps(acc, 1);
        __m128 sum = _mm_add_ps(lo, hi);
        sum = _mm_hadd_ps(sum, sum);
        sum = _mm_hadd_ps(sum, sum);
        float row_result = _mm_cvtss_f32(sum);
        for (; j < n; j++) row_result += a[i * n + j] * x[j];
        y[i] = beta * y[i] + row_result;
    }
}

__attribute__((target("avx2,fma")))
static void scl_ml_simd_avx2_gemv_t(float *y, const float *a,
                                      const float *x,
                                      size_t m, size_t n, float beta) {
    /* y = beta * y + A^T @ x */
    for (size_t j = 0; j < n; j++) y[j] *= beta;
    for (size_t i = 0; i < m; i++) {
        float xi = x[i];
        __m256 vxi = _mm256_set1_ps(xi);
        size_t j = 0;
        for (; j + 8 <= n; j += 8) {
            __m256 vy = _mm256_loadu_ps(&y[j]);
            __m256 va = _mm256_loadu_ps(&a[i * n + j]);
            _mm256_storeu_ps(&y[j], _mm256_fmadd_ps(vxi, va, vy));
        }
        for (; j < n; j++) y[j] += xi * a[i * n + j];
    }
}

/* ── AVX2 override function ─────────────────────────────────── */
void scl_ml_simd_override_avx2(scl_ml_simd_t *t) {
    t->dot        = scl_ml_simd_avx2_dot;
    t->dot_f      = scl_ml_simd_avx2_dot_f;
    t->norm_l2_sq = scl_ml_simd_avx2_norm_l2_sq;
    t->axpy       = scl_ml_simd_avx2_axpy;
    t->sum        = scl_ml_simd_avx2_sum;
    t->max        = scl_ml_simd_avx2_max;
    t->add        = scl_ml_simd_avx2_add;
    t->sub        = scl_ml_simd_avx2_sub;
    t->mul        = scl_ml_simd_avx2_mul;
    t->fma        = scl_ml_simd_avx2_fma;
    t->relu       = scl_ml_simd_avx2_relu;
    t->sigmoid    = scl_ml_simd_avx2_sigmoid;
    t->dist_l2_sq = scl_ml_simd_avx2_dist_l2_sq;
    t->gemv       = scl_ml_simd_avx2_gemv;
    t->gemv_t     = scl_ml_simd_avx2_gemv_t;
}

#else /* not x86-64 */

void scl_ml_simd_override_avx2(scl_ml_simd_t *t) { (void)t; }

#endif /* x86-64 AVX2 */

/* ═══════════════════════════════════════════════════════════════
 * NEON overrides (ARM64)
 * ═══════════════════════════════════════════════════════════════ */

#if defined(SCL_ARCH_ARM64)
#include <arm_neon.h>

static void scl_ml_simd_neon_sigmoid(float *out, const float *in, size_t n) {
    float32x4_t one = vdupq_n_f32(1.0f);
    float32x4_t clamp_lo = vdupq_n_f32(-30.0f);
    float32x4_t clamp_hi = vdupq_n_f32(30.0f);
    float32x4_t magic = vdupq_n_f32(-12102203.0f);
    float32x4_t bias  = vdupq_n_f32(1065353216.0f);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t vi = vld1q_f32(&in[i]);
        vi = vminq_f32(vmaxq_f32(vi, clamp_lo), clamp_hi);
        float32x4_t vn = vmlaq_f32(bias, magic, vi);
        int32x4_t vi_clamped = vcvtq_s32_f32(vn);
        vi_clamped = vminq_s32(vmaxq_s32(vi_clamped, vdupq_n_s32(0)),
                                vdupq_n_s32(0x7F800000));
        float32x4_t ve = vreinterpretq_f32_s32(vi_clamped);
        ve = vaddq_f32(one, ve);
        vst1q_f32(&out[i], vdivq_f32(one, ve));
    }
    for (; i < n; i++) out[i] = scl_ml_fast_sigmoid(in[i]);
}

static void scl_ml_simd_neon_relu(float *out, const float *in, size_t n) {
    float32x4_t zero = vdupq_n_f32(0.0f);
    size_t i = 0;
    for (; i + 4 <= n; i += 4)
        vst1q_f32(&out[i], vmaxq_f32(vld1q_f32(&in[i]), zero));
    for (; i < n; i++) out[i] = in[i] > 0.0f ? in[i] : 0.0f;
}

static float scl_ml_simd_neon_dot(const float *a, const float *b, size_t n) {
    float32x4_t acc = vdupq_n_f32(0.0f);
    size_t i = 0;
    for (; i + 4 <= n; i += 4)
        acc = vmlaq_f32(acc, vld1q_f32(&a[i]), vld1q_f32(&b[i]));
    float result = vaddvq_f32(acc);
    for (; i < n; i++) result += a[i] * b[i];
    return result;
}

static float scl_ml_simd_neon_norm_l2_sq(const float *x, size_t n) {
    return scl_ml_simd_neon_dot(x, x, n);
}

static float scl_ml_simd_neon_dist_l2_sq(const float *a, const float *b,
                                           size_t d) {
    float32x4_t acc = vdupq_n_f32(0.0f);
    size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        float32x4_t va = vld1q_f32(&a[i]);
        float32x4_t vb = vld1q_f32(&b[i]);
        float32x4_t diff = vsubq_f32(va, vb);
        acc = vmlaq_f32(acc, diff, diff);
    }
    float result = vaddvq_f32(acc);
    for (; i < d; i++) { float df = a[i] - b[i]; result += df * df; }
    return result;
}

static void scl_ml_simd_neon_axpy(float *y, float alpha,
                                    const float *x, size_t n) {
    float32x4_t va = vdupq_n_f32(alpha);
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        float32x4_t vy = vld1q_f32(&y[i]);
        float32x4_t vx = vld1q_f32(&x[i]);
        vst1q_f32(&y[i], vmlaq_f32(vy, va, vx));
    }
    for (; i < n; i++) y[i] += alpha * x[i];
}

static float scl_ml_simd_neon_sum(const float *x, size_t n) {
    float32x4_t acc = vdupq_n_f32(0.0f);
    size_t i = 0;
    for (; i + 4 <= n; i += 4)
        acc = vaddq_f32(acc, vld1q_f32(&x[i]));
    float result = vaddvq_f32(acc);
    for (; i < n; i++) result += x[i];
    return result;
}

/* ── NEON override function ─────────────────────────────────── */
void scl_ml_simd_override_neon(scl_ml_simd_t *t) {
    t->dot        = scl_ml_simd_neon_dot;
    t->dot_f      = scl_ml_simd_neon_dot;
    t->norm_l2_sq = scl_ml_simd_neon_norm_l2_sq;
    t->dist_l2_sq = scl_ml_simd_neon_dist_l2_sq;
    t->axpy       = scl_ml_simd_neon_axpy;
    t->sum        = scl_ml_simd_neon_sum;
    t->sigmoid    = scl_ml_simd_neon_sigmoid;
    t->relu       = scl_ml_simd_neon_relu;
}

#else

void scl_ml_simd_override_neon(scl_ml_simd_t *t) { (void)t; }

#endif /* ARM64 NEON */

/* ═══════════════════════════════════════════════════════════════
 * SSE4.2 overrides (fallback when AVX2 unavailable)
 * ═══════════════════════════════════════════════════════════════ */

#if defined(__x86_64__) || defined(_M_X64)
#include <smmintrin.h>

__attribute__((target("sse4.2")))
static float scl_ml_simd_sse42_dot(const float *a, const float *b, size_t n) {
    __m128 acc = _mm_setzero_ps();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);
        __m128 vb = _mm_loadu_ps(&b[i]);
        acc = _mm_add_ps(acc, _mm_mul_ps(va, vb));
    }
    __m128 shuf = _mm_movehdup_ps(acc);
    __m128 sums = _mm_add_ps(acc, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    float result = _mm_cvtss_f32(sums);
    for (; i < n; i++) result += a[i] * b[i];
    return result;
}

__attribute__((target("sse4.2")))
static float scl_ml_simd_sse42_dist_l2_sq(const float *a, const float *b,
                                            size_t d) {
    __m128 acc = _mm_setzero_ps();
    size_t i = 0;
    for (; i + 4 <= d; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);
        __m128 vb = _mm_loadu_ps(&b[i]);
        __m128 diff = _mm_sub_ps(va, vb);
        acc = _mm_add_ps(acc, _mm_mul_ps(diff, diff));
    }
    __m128 shuf = _mm_movehdup_ps(acc);
    __m128 sums = _mm_add_ps(acc, shuf);
    shuf = _mm_movehl_ps(shuf, sums);
    sums = _mm_add_ss(sums, shuf);
    float result = _mm_cvtss_f32(sums);
    for (; i < d; i++) { float df = a[i] - b[i]; result += df * df; }
    return result;
}

__attribute__((target("sse4.2")))
static void scl_ml_simd_sse42_relu(float *out, const float *in, size_t n) {
    __m128 zero = _mm_setzero_ps();
    size_t i = 0;
    for (; i + 4 <= n; i += 4)
        _mm_storeu_ps(&out[i], _mm_max_ps(_mm_loadu_ps(&in[i]), zero));
    for (; i < n; i++) out[i] = in[i] > 0.0f ? in[i] : 0.0f;
}

void scl_ml_simd_override_sse42(scl_ml_simd_t *t) {
    t->dot        = scl_ml_simd_sse42_dot;
    t->dot_f      = scl_ml_simd_sse42_dot;
    t->norm_l2_sq = scl_ml_simd_sse42_dist_l2_sq;
    t->dist_l2_sq = scl_ml_simd_sse42_dist_l2_sq;
    t->relu       = scl_ml_simd_sse42_relu;
}

#else

void scl_ml_simd_override_sse42(scl_ml_simd_t *t) { (void)t; }

#endif /* x86-64 SSE4.2 */

/* ═══════════════════════════════════════════════════════════════
 * AVX-512 overrides
 * ═══════════════════════════════════════════════════════════════ */

#if defined(__x86_64__) || defined(_M_X64)

__attribute__((target("avx512f,avx512bw,avx512dq")))
static float scl_ml_simd_avx512_dot(const float *a, const float *b, size_t n) {
    __m512 acc = _mm512_setzero_ps();
    size_t i = 0;
    for (; i + 16 <= n; i += 16)
        acc = _mm512_fmadd_ps(_mm512_loadu_ps(&a[i]), _mm512_loadu_ps(&b[i]), acc);
    float result = _mm512_reduce_add_ps(acc);
    for (; i < n; i++) result += a[i] * b[i];
    return result;
}

__attribute__((target("avx512f,avx512bw,avx512dq")))
static float scl_ml_simd_avx512_dist_l2_sq(const float *a, const float *b,
                                             size_t d) {
    __m512 acc = _mm512_setzero_ps();
    size_t i = 0;
    for (; i + 16 <= d; i += 16) {
        __m512 diff = _mm512_sub_ps(_mm512_loadu_ps(&a[i]), _mm512_loadu_ps(&b[i]));
        acc = _mm512_fmadd_ps(diff, diff, acc);
    }
    float result = _mm512_reduce_add_ps(acc);
    for (; i < d; i++) { float df = a[i] - b[i]; result += df * df; }
    return result;
}

__attribute__((target("avx512f,avx512bw,avx512dq")))
static float scl_ml_simd_avx512_max(const float *x, size_t n) {
    __m512 vmax = _mm512_loadu_ps(x);
    size_t i = 16;
    for (; i + 16 <= n; i += 16)
        vmax = _mm512_max_ps(vmax, _mm512_loadu_ps(&x[i]));
    float result = _mm512_reduce_max_ps(vmax);
    for (; i < n; i++) if (x[i] > result) result = x[i];
    return result;
}

__attribute__((target("avx512f,avx512bw,avx512dq")))
static void scl_ml_simd_avx512_relu(float *out, const float *in, size_t n) {
    __m512 zero = _mm512_setzero_ps();
    size_t i = 0;
    for (; i + 16 <= n; i += 16)
        _mm512_storeu_ps(&out[i], _mm512_max_ps(_mm512_loadu_ps(&in[i]), zero));
    for (; i < n; i++) out[i] = in[i] > 0.0f ? in[i] : 0.0f;
}

__attribute__((target("avx512f,avx512bw,avx512dq")))
static void scl_ml_simd_avx512_axpy(float *y, float alpha,
                                      const float *x, size_t n) {
    __m512 va = _mm512_set1_ps(alpha);
    size_t i = 0;
    for (; i + 16 <= n; i += 16) {
        __m512 vy = _mm512_loadu_ps(&y[i]);
        __m512 vx = _mm512_loadu_ps(&x[i]);
        _mm512_storeu_ps(&y[i], _mm512_fmadd_ps(va, vx, vy));
    }
    for (; i < n; i++) y[i] += alpha * x[i];
}

void scl_ml_simd_override_avx512(scl_ml_simd_t *t) {
    t->dot        = scl_ml_simd_avx512_dot;
    t->dot_f      = scl_ml_simd_avx512_dot;
    t->norm_l2_sq = scl_ml_simd_avx512_dist_l2_sq;
    t->dist_l2_sq = scl_ml_simd_avx512_dist_l2_sq;
    t->max        = scl_ml_simd_avx512_max;
    t->relu       = scl_ml_simd_avx512_relu;
    t->axpy       = scl_ml_simd_avx512_axpy;
}

#else

void scl_ml_simd_override_avx512(scl_ml_simd_t *t) { (void)t; }

#endif /* x86-64 AVX-512 */

/* ═══════════════════════════════════════════════════════════════
 * Dispatch table initialization
 * ─── Selects highest available ISA.
 * Priority: AVX-512 > AVX2+FMA > SSE4.2 > NEON > Scalar
 * ═══════════════════════════════════════════════════════════════ */

void scl_ml_simd_init(void) {
    if (scl_likely(scl_ml_simd_initialized))
        return;

    /* Start with scalar fallback (declared in scl_ml_simd_scalar.c) */
    scl_ml_simd_override_scalar(&scl_ml_simd);

    uint64_t caps = scl_ml_cpu_features();

    /* Override with best available SIMD — applied in reverse priority
     * so that higher ISA overrides lower ones (order matters) */
    if (caps & SCL_ML_CPU_NEON)
        scl_ml_simd_override_neon(&scl_ml_simd);

    if (caps & SCL_ML_CPU_SSE42)
        scl_ml_simd_override_sse42(&scl_ml_simd);

    if (caps & SCL_ML_CPU_AVX2)
        scl_ml_simd_override_avx2(&scl_ml_simd);

    if (caps & (SCL_ML_CPU_AVX512F | SCL_ML_CPU_AVX512BW | SCL_ML_CPU_AVX512DQ))
        scl_ml_simd_override_avx512(&scl_ml_simd);

    scl_ml_simd_initialized = 1;
}
