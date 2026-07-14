/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0; see LICENSE.
 */

/* ARM64 SVE / SVE2 SIMD overrides — scalable vectors (128b–2048b).
 * Uses ARM SVE ACLE (arm_sve.h). The vector length is determined at
 * runtime via svcntw(). These kernels use predicated execution, so
 * they are "tail-agnostic" — the predicate register handles partial
 * vectors automatically without scalar cleanup loops.
 *
 * Only enabled when the CPU reports SVE support via HWCAP or sysctl. */

#include "scl_math.h"
#include "scl_ml_simd.h"
#include <math.h>

#if defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>

/* Helper: number of active 32-bit float lanes */
static inline int scl_sve_f32_cnt(void) { return svcntw(); }

/* ── Dot product (float accumulator) ─────────────────────────── */
static float scl_ml_simd_sve_dot(const float *SCL_RESTRICT a,
                                 const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t acc = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t va = svld1_f32(pg, &a[i]);
    svfloat32_t vb = svld1_f32(pg, &b[i]);
    acc = svmla_f32_z(pg, acc, va, vb);
  }
  float result = svaddv_f32(pg, acc);
  for (; i < n; i++)
    result += a[i] * b[i];
  return result;
}

/* ── Dot product (double accumulator for stability) ──────────── */
static float scl_ml_simd_sve_dot_f(const float *SCL_RESTRICT a,
                                   const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat64_t acc0 = svdup_f64(0.0);
  svfloat64_t acc1 = svdup_f64(0.0);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  int half = cnt / 2;
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t va = svld1_f32(pg, &a[i]);
    svfloat32_t vb = svld1_f32(pg, &b[i]);
    svfloat32_t prod = svmul_f32_z(pg, va, vb);
    svbool_t pgl = svptrue_pat_b32(SV_VL2);
    svbool_t pgh = svlastb_b32(svptrue_b32(), svwhilelt_b32((size_t)half, cnt));
    if (half > 0) {
      acc0 =
          svmla_f32_f64_z(pgl, acc0, svget_low_f32(prod), svget_low_f32(prod));
    }
    if (cnt - half > 0) {
      acc1 = svmla_f32_f64_z(pgh, acc1, svget_high_f32(prod),
                             svget_high_f32(prod));
    }
  }
  double result =
      svaddv_f64(svptrue_b64(), acc0) + svaddv_f64(svptrue_b64(), acc1);
  for (; i < n; i++)
    result += (double)a[i] * b[i];
  return (float)result;
}

/* ── Norm L2 squared ────────────────────────────────────────── */
static float scl_ml_simd_sve_norm_l2_sq(const float *SCL_RESTRICT x, size_t n) {
  return scl_ml_simd_sve_dot(x, x, n);
}

static float scl_ml_simd_sve_norm_l2(const float *SCL_RESTRICT x, size_t n) {
  return sqrtf(scl_ml_simd_sve_norm_l2_sq(x, n));
}

/* ── Norm L1 ─────────────────────────────────────────────────── */
static float scl_ml_simd_sve_norm_l1(const float *SCL_RESTRICT x, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t acc = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vx = svld1_f32(pg, &x[i]);
    acc = svadd_f32_z(pg, acc, svabs_f32(vx));
  }
  float result = svaddv_f32(pg, acc);
  for (; i < n; i++)
    result += fabsf(x[i]);
  return result;
}

/* ── Sum ─────────────────────────────────────────────────────── */
static float scl_ml_simd_sve_sum(const float *SCL_RESTRICT x, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t acc = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt)
    acc = svadd_f32_z(pg, acc, svld1_f32(pg, &x[i]));
  float result = svaddv_f32(pg, acc);
  for (; i < n; i++)
    result += x[i];
  return result;
}

/* ── Max ─────────────────────────────────────────────────────── */
static float scl_ml_simd_sve_max(const float *SCL_RESTRICT x, size_t n) {
  if (n == 0)
    return -FLT_MAX;
  svbool_t pg = svptrue_b32();
  svfloat32_t vmax = svld1_f32(pg, x);
  size_t i = scl_sve_f32_cnt();
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt)
    vmax = svmax_f32_z(pg, vmax, svld1_f32(pg, &x[i]));
  float result = svmaxv_f32(pg, vmax);
  for (; i < n; i++)
    if (x[i] > result)
      result = x[i];
  return result;
}

/* ── Min ─────────────────────────────────────────────────────── */
static float scl_ml_simd_sve_min(const float *SCL_RESTRICT x, size_t n) {
  if (n == 0)
    return FLT_MAX;
  svbool_t pg = svptrue_b32();
  svfloat32_t vmin = svld1_f32(pg, x);
  size_t i = scl_sve_f32_cnt();
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt)
    vmin = svmin_f32_z(pg, vmin, svld1_f32(pg, &x[i]));
  float result = svminv_f32(pg, vmin);
  for (; i < n; i++)
    if (x[i] < result)
      result = x[i];
  return result;
}

/* ── Argmax ──────────────────────────────────────────────────── */
static size_t scl_ml_simd_sve_argmax(const float *SCL_RESTRICT x, size_t n) {
  if (n == 0)
    return 0;
  svbool_t pg = svptrue_b32();
  svfloat32_t vmax = svld1_f32(pg, x);
  svuint32_t vidx_max = svindex_u32(0, 1);
  size_t offset = scl_sve_f32_cnt();
  int cnt = scl_sve_f32_cnt();
  for (size_t i = offset; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &x[i]);
    svuint32_t vidxi = svadd_n_u32_z(pg, svindex_u32(0, 1), (uint32_t)i);
    svbool_t mask = svcmpgt_f32(pg, vi, vmax);
    vmax = svsel_f32(mask, vi, vmax);
    vidx_max = svsel_u32(mask, vidxi, vidx_max);
  }
  float tmp_max = svmaxv_f32(pg, vmax);
  svbool_t best_mask = svcmpeq_f32(pg, vmax, tmp_max);
  uint32_t idx_arr[256];
  svst1_u32(pg, idx_arr, vidx_max);
  size_t best_idx = 0;
  for (int j = 0; j < cnt; j++) {
    if (idx_arr[j] >= (uint32_t)n)
      continue;
    if (x[idx_arr[j]] == tmp_max) {
      best_idx = idx_arr[j];
      break;
    }
  }
  for (size_t i = n - (n % (size_t)cnt); i < n; i++) {
    if (x[i] > tmp_max) {
      tmp_max = x[i];
      best_idx = i;
    }
  }
  return best_idx;
}

/* ── Argmin ──────────────────────────────────────────────────── */
static size_t scl_ml_simd_sve_argmin(const float *SCL_RESTRICT x, size_t n) {
  if (n == 0)
    return 0;
  svbool_t pg = svptrue_b32();
  svfloat32_t vmin = svld1_f32(pg, x);
  svuint32_t vidx_min = svindex_u32(0, 1);
  size_t offset = scl_sve_f32_cnt();
  int cnt = scl_sve_f32_cnt();
  for (size_t i = offset; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &x[i]);
    svuint32_t vidxi = svadd_n_u32_z(pg, svindex_u32(0, 1), (uint32_t)i);
    svbool_t mask = svcmplt_f32(pg, vi, vmin);
    vmin = svsel_f32(mask, vi, vmin);
    vidx_min = svsel_u32(mask, vidxi, vidx_min);
  }
  float tmp_min = svminv_f32(pg, vmin);
  uint32_t idx_arr[256];
  svst1_u32(pg, idx_arr, vidx_min);
  size_t best_idx = 0;
  for (int j = 0; j < cnt; j++) {
    if (idx_arr[j] >= (uint32_t)n)
      continue;
    if (x[idx_arr[j]] == tmp_min) {
      best_idx = idx_arr[j];
      break;
    }
  }
  for (size_t i = n - (n % (size_t)cnt); i < n; i++) {
    if (x[i] < tmp_min) {
      tmp_min = x[i];
      best_idx = i;
    }
  }
  return best_idx;
}

/* ── Argminmax ───────────────────────────────────────────────── */
static void scl_ml_simd_sve_argminmax(const float *SCL_RESTRICT x, size_t n,
                                      size_t *argmin_out, size_t *argmax_out) {
  if (n == 0) {
    *argmin_out = *argmax_out = 0;
    return;
  }
  svbool_t pg = svptrue_b32();
  svfloat32_t vmin = svld1_f32(pg, x);
  svfloat32_t vmax = vmin;
  svuint32_t vidx_min = svindex_u32(0, 1);
  svuint32_t vidx_max = vidx_min;
  size_t offset = scl_sve_f32_cnt();
  int cnt = scl_sve_f32_cnt();
  for (size_t i = offset; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &x[i]);
    svuint32_t vidxi = svadd_n_u32_z(pg, svindex_u32(0, 1), (uint32_t)i);
    svbool_t mask_lt = svcmplt_f32(pg, vi, vmin);
    svbool_t mask_gt = svcmpgt_f32(pg, vi, vmax);
    vmin = svsel_f32(mask_lt, vi, vmin);
    vmax = svsel_f32(mask_gt, vi, vmax);
    vidx_min = svsel_u32(mask_lt, vidxi, vidx_min);
    vidx_max = svsel_u32(mask_gt, vidxi, vidx_max);
  }
  float best_min = svminv_f32(pg, vmin);
  float best_max = svmaxv_f32(pg, vmax);
  uint32_t idx_min_arr[256], idx_max_arr[256];
  svst1_u32(pg, idx_min_arr, vidx_min);
  svst1_u32(pg, idx_max_arr, vidx_max);
  size_t imin = 0, imax = 0;
  for (int j = 0; j < cnt; j++) {
    if (idx_min_arr[j] < (uint32_t)n && x[idx_min_arr[j]] == best_min) {
      imin = idx_min_arr[j];
      break;
    }
  }
  for (int j = 0; j < cnt; j++) {
    if (idx_max_arr[j] < (uint32_t)n && x[idx_max_arr[j]] == best_max) {
      imax = idx_max_arr[j];
      break;
    }
  }
  for (size_t i = n - (n % (size_t)cnt); i < n; i++) {
    if (x[i] < best_min) {
      best_min = x[i];
      imin = i;
    }
    if (x[i] > best_max) {
      best_max = x[i];
      imax = i;
    }
  }
  *argmin_out = imin;
  *argmax_out = imax;
}

/* ── Element-wise: vector–vector ─────────────────────────────── */
static void scl_ml_simd_sve_add(float *SCL_RESTRICT z,
                                const float *SCL_RESTRICT a,
                                const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &z[i],
              svadd_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i])));
  }
  for (; i < n; i++)
    z[i] = a[i] + b[i];
}

static void scl_ml_simd_sve_sub(float *SCL_RESTRICT z,
                                const float *SCL_RESTRICT a,
                                const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &z[i],
              svsub_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i])));
  }
  for (; i < n; i++)
    z[i] = a[i] - b[i];
}

static void scl_ml_simd_sve_mul(float *SCL_RESTRICT z,
                                const float *SCL_RESTRICT a,
                                const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &z[i],
              svmul_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i])));
  }
  for (; i < n; i++)
    z[i] = a[i] * b[i];
}

static void scl_ml_simd_sve_div(float *SCL_RESTRICT z,
                                const float *SCL_RESTRICT a,
                                const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &z[i],
              svdiv_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i])));
  }
  for (; i < n; i++)
    z[i] = a[i] / b[i];
}

static void scl_ml_simd_sve_abs(float *SCL_RESTRICT out,
                                const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &out[i], svabs_f32(svld1_f32(pg, &in[i])));
  }
  for (; i < n; i++)
    out[i] = fabsf(in[i]);
}

static void scl_ml_simd_sve_fma(float *SCL_RESTRICT z,
                                const float *SCL_RESTRICT a,
                                const float *SCL_RESTRICT b, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &z[i],
              svmla_f32_z(pg, svld1_f32(pg, &z[i]), svld1_f32(pg, &a[i]),
                          svld1_f32(pg, &b[i])));
  }
  for (; i < n; i++)
    z[i] += a[i] * b[i];
}

/* ── Element-wise: scalar–vector ──────────────────────────────── */
static void scl_ml_simd_sve_add_s(float *SCL_RESTRICT z,
                                  const float *SCL_RESTRICT x, float s,
                                  size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vs = svdup_f32(s);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt)
    svst1_f32(pg, &z[i], svadd_f32_z(pg, svld1_f32(pg, &x[i]), vs));
  for (; i < n; i++)
    z[i] = x[i] + s;
}

static void scl_ml_simd_sve_mul_s(float *SCL_RESTRICT z,
                                  const float *SCL_RESTRICT x, float s,
                                  size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vs = svdup_f32(s);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt)
    svst1_f32(pg, &z[i], svmul_f32_z(pg, svld1_f32(pg, &x[i]), vs));
  for (; i < n; i++)
    z[i] = x[i] * s;
}

static void scl_ml_simd_sve_scale_add_s(float *SCL_RESTRICT z, float alpha,
                                        const float *SCL_RESTRICT x, float beta,
                                        size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t va = svdup_f32(alpha);
  svfloat32_t vb = svdup_f32(beta);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt)
    svst1_f32(pg, &z[i], svmla_f32_z(pg, vb, va, svld1_f32(pg, &x[i])));
  for (; i < n; i++)
    z[i] = alpha * x[i] + beta;
}

/* ── Activations ──────────────────────────────────────────────── */
static void scl_ml_simd_sve_sigmoid(float *SCL_RESTRICT out,
                                    const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t one = svdup_f32(1.0f);
  svfloat32_t lo = svdup_f32(-30.0f);
  svfloat32_t hi = svdup_f32(30.0f);
  svfloat32_t magic = svdup_f32(-12102203.0f);
  svfloat32_t bias = svdup_f32(1.0f);
  svint32_t zero_i = svdup_s32(0);
  svint32_t top_i = svdup_s32(0x7F800000);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    vi = svmax_f32_z(pg, svmin_f32_z(pg, vi, hi), lo);
    svint32_t bits = svcvt_s32_f32_z(pg, svmul_f32_z(pg, vi, magic));
    bits = svadd_s32_z(pg, bits, vreinterpret_s32_f32(bias));
    bits = svmax_s32_z(pg, bits, zero_i);
    bits = svmin_s32_z(pg, bits, top_i);
    svfloat32_t ve = svadd_f32_z(pg, one, vreinterpret_f32_s32(bits));
    svst1_f32(pg, &out[i], svdiv_f32_z(pg, one, ve));
  }
  for (; i < n; i++)
    out[i] = scl_ml_fast_sigmoid(in[i]);
}

static void scl_ml_simd_sve_tanh_fast(float *SCL_RESTRICT out,
                                      const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t two = svdup_f32(2.0f);
  svfloat32_t one = svdup_f32(1.0f);
  svfloat32_t lo = svdup_f32(-10.0f);
  svfloat32_t hi = svdup_f32(10.0f);
  svfloat32_t magic = svdup_f32(-12102203.0f * 2.0f);
  svfloat32_t bias = svdup_f32(1.0f);
  svint32_t zero_i = svdup_s32(0);
  svint32_t top_i = svdup_s32(0x7F800000);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    vi = svmax_f32_z(pg, svmin_f32_z(pg, vi, hi), lo);
    svint32_t bits = svcvt_s32_f32_z(pg, svmul_f32_z(pg, vi, magic));
    bits = svadd_s32_z(pg, bits, vreinterpret_s32_f32(bias));
    bits = svmax_s32_z(pg, bits, zero_i);
    bits = svmin_s32_z(pg, bits, top_i);
    svfloat32_t ve = svsub_f32_z(
        pg,
        svdiv_f32_z(pg, two, svadd_f32_z(pg, one, vreinterpret_f32_s32(bits))),
        one);
    svst1_f32(pg, &out[i], ve);
  }
  for (; i < n; i++)
    out[i] = scl_ml_fast_tanh(in[i]);
}

static void scl_ml_simd_sve_gelu(float *SCL_RESTRICT out,
                                 const float *SCL_RESTRICT in, size_t n) {
  const float sqrt_2_over_pi = 0.7978845608028654f;
  const float c = 0.044715f;
  svbool_t pg = svptrue_b32();
  svfloat32_t vsop = svdup_f32(sqrt_2_over_pi);
  svfloat32_t vc = svdup_f32(c);
  svfloat32_t half = svdup_f32(0.5f);
  svfloat32_t one = svdup_f32(1.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t x = svld1_f32(pg, &in[i]);
    svfloat32_t x3 = svmul_f32_z(pg, svmul_f32_z(pg, x, x), x);
    svfloat32_t inner = svmul_f32_z(pg, vsop, svmla_f32_z(pg, x, vc, x3));
    svfloat32_t two = svdup_f32(2.0f);
    svfloat32_t mag = svdup_f32(-12102203.0f * 2.0f);
    svfloat32_t bia = svdup_f32(1.0f);
    svint32_t zi = svcvt_s32_f32_z(pg, svmul_f32_z(pg, inner, mag));
    zi = svadd_s32_z(pg, zi, vreinterpret_s32_f32(bia));
    zi = svmax_s32_z(pg, zi, svdup_s32(0));
    zi = svmin_s32_z(pg, zi, svdup_s32(0x7F800000));
    svfloat32_t ve = svsub_f32_z(
        pg,
        svdiv_f32_z(pg, two, svadd_f32_z(pg, one, vreinterpret_f32_s32(zi))),
        one);
    svst1_f32(
        pg, &out[i],
        svmul_f32_z(pg, svmul_f32_z(pg, half, x), svadd_f32_z(pg, one, ve)));
  }
  for (; i < n; i++) {
    float x = in[i];
    float x3 = x * x * x;
    out[i] =
        0.5f * x * (1.0f + tanhf(0.7978845608028654f * (x + 0.044715f * x3)));
  }
}

static void scl_ml_simd_sve_silu(float *SCL_RESTRICT out,
                                 const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t one = svdup_f32(1.0f);
  svfloat32_t magic = svdup_f32(-12102203.0f);
  svfloat32_t bias = svdup_f32(1.0f);
  svint32_t zero_i = svdup_s32(0);
  svint32_t top_i = svdup_s32(0x7F800000);
  svfloat32_t vzero = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t x = svld1_f32(pg, &in[i]);
    svfloat32_t neg_x = svneg_f32_z(pg, x);
    svint32_t bits = svcvt_s32_f32_z(pg, svmul_f32_z(pg, neg_x, magic));
    bits = svadd_s32_z(pg, bits, vreinterpret_s32_f32(bias));
    bits = svmax_s32_z(pg, bits, zero_i);
    bits = svmin_s32_z(pg, bits, top_i);
    svfloat32_t exp_neg_x = svmax_f32_z(pg, vreinterpret_f32_s32(bits), vzero);
    svfloat32_t sigmoid_x =
        svdiv_f32_z(pg, one, svadd_f32_z(pg, one, exp_neg_x));
    svst1_f32(pg, &out[i], svmul_f32_z(pg, x, sigmoid_x));
  }
  for (; i < n; i++) {
    float v = in[i];
    out[i] = v / (1.0f + expf(-v));
  }
}

static void scl_ml_simd_sve_relu(float *SCL_RESTRICT out,
                                 const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t zero = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(pg, &out[i], svmax_f32_z(pg, svld1_f32(pg, &in[i]), zero));
  }
  for (; i < n; i++)
    out[i] = in[i] > 0.0f ? in[i] : 0.0f;
}

static void scl_ml_simd_sve_relu6(float *SCL_RESTRICT out,
                                  const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t zero = svdup_f32(0.0f);
  svfloat32_t six = svdup_f32(6.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svst1_f32(
        pg, &out[i],
        svmin_f32_z(pg, svmax_f32_z(pg, svld1_f32(pg, &in[i]), zero), six));
  }
  for (; i < n; i++) {
    float v = in[i];
    out[i] = v < 0.0f ? 0.0f : (v > 6.0f ? 6.0f : v);
  }
}

static void scl_ml_simd_sve_leaky_relu(float *SCL_RESTRICT out,
                                       const float *SCL_RESTRICT in,
                                       float slope, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vzero = svdup_f32(0.0f);
  svfloat32_t vslope = svdup_f32(slope);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svbool_t mask = svcmpgt_f32(pg, vi, vzero);
    svfloat32_t neg = svmul_f32_z(mask, vi, vslope);
    svst1_f32(pg, &out[i], svsel_f32(mask, vi, neg));
  }
  for (; i < n; i++) {
    float v = in[i];
    out[i] = v > 0.0f ? v : v * slope;
  }
}

static void scl_ml_simd_sve_elu(float *SCL_RESTRICT out,
                                const float *SCL_RESTRICT in, float alpha,
                                size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vzero = svdup_f32(0.0f);
  svfloat32_t valpha = svdup_f32(alpha);
  svfloat32_t vone = svdup_f32(1.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svbool_t mask = svcmpgt_f32(pg, vi, vzero);
    /* Scalar exp for negative branch (SVML not universally available) */
    float tmp[256];
    for (int j = 0; j < cnt; j++)
      tmp[j] = alpha * (expf(svget_f32(vi, j)) - 1.0f);
    svfloat32_t neg = svld1_f32(pg, tmp);
    svst1_f32(pg, &out[i], svsel_f32(mask, vi, neg));
  }
  for (; i < n; i++) {
    float v = in[i];
    out[i] = v > 0.0f ? v : alpha * (expf(v) - 1.0f);
  }
}

static void scl_ml_simd_sve_softmax(float *SCL_RESTRICT out,
                                    const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  int cnt = scl_sve_f32_cnt();
  svfloat32_t vmax = svld1_f32(pg, in);
  size_t i = cnt;
  for (; i + (size_t)cnt <= n; i += cnt)
    vmax = svmax_f32_z(pg, vmax, svld1_f32(pg, &in[i]));
  float maxv = svmaxv_f32(pg, vmax);
  for (; i < n; i++)
    if (in[i] > maxv)
      maxv = in[i];

  svfloat32_t vmaxv = svdup_f32(maxv);
  svfloat32_t vzero = svdup_f32(0.0f);
  i = 0;
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svsub_f32_z(pg, svld1_f32(pg, &in[i]), vmaxv);
    svint32_t bits =
        svcvt_s32_f32_z(pg, svmul_f32_z(pg, vi, svdup_f32(-12102203.0f)));
    bits = svmax_s32_z(pg, bits, svdup_s32(0));
    bits = svmin_s32_z(pg, bits, svdup_s32(0x7F800000));
    svfloat32_t ve = svmax_f32_z(pg, vreinterpret_f32_s32(bits), vzero);
    svst1_f32(pg, &out[i], ve);
  }
  for (; i < n; i++)
    out[i] = scl_ml_fast_exp(in[i] - maxv);

  float sum = svaddv_f32(pg, svld1_f32(pg, out));
  for (i = cnt; i + (size_t)cnt <= n; i += cnt)
    sum += svaddv_f32(pg, svld1_f32(pg, &out[i]));
  for (; i < n; i++)
    sum += out[i];
  svfloat32_t vinv = svdup_f32(1.0f / sum);
  for (i = 0; i + (size_t)cnt <= n; i += cnt)
    svst1_f32(pg, &out[i], svmul_f32_z(pg, svld1_f32(pg, &out[i]), vinv));
  for (; i < n; i++)
    out[i] *= 1.0f / sum;
}

static void scl_ml_simd_sve_log_softmax(float *SCL_RESTRICT out,
                                        const float *SCL_RESTRICT in,
                                        size_t n) {
  svbool_t pg = svptrue_b32();
  int cnt = scl_sve_f32_cnt();
  svfloat32_t vmax = svld1_f32(pg, in);
  size_t i = cnt;
  for (; i + (size_t)cnt <= n; i += cnt)
    vmax = svmax_f32_z(pg, vmax, svld1_f32(pg, &in[i]));
  float maxv = svmaxv_f32(pg, vmax);
  for (; i < n; i++)
    if (in[i] > maxv)
      maxv = in[i];

  svfloat32_t vmaxv = svdup_f32(maxv);
  svfloat32_t vzero = svdup_f32(0.0f);
  i = 0;
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svsub_f32_z(pg, svld1_f32(pg, &in[i]), vmaxv);
    svint32_t bits =
        svcvt_s32_f32_z(pg, svmul_f32_z(pg, vi, svdup_f32(-12102203.0f)));
    bits = svmax_s32_z(pg, bits, svdup_s32(0));
    bits = svmin_s32_z(pg, bits, svdup_s32(0x7F800000));
    svst1_f32(pg, &out[i], svmax_f32_z(pg, vreinterpret_f32_s32(bits), vzero));
  }
  float sum = 0.0f;
  for (; i < n; i++) {
    out[i] = scl_ml_fast_exp(in[i] - maxv);
    sum += out[i];
  }
  float log_s = logf(sum);
  svfloat32_t vlog_s = svdup_f32(log_s);
  for (i = 0; i + (size_t)cnt <= n; i += cnt)
    svst1_f32(
        pg, &out[i],
        svsub_f32_z(pg, svld1_f32(pg, &in[i]), svadd_f32_z(pg, vmaxv, vlog_s)));
  for (; i < n; i++)
    out[i] = in[i] - maxv - log_s;
}

static void scl_ml_simd_sve_vexp(float *SCL_RESTRICT out,
                                 const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t magic = svdup_f32(12102203.0f);
  svfloat32_t bias = svdup_f32(1.0f);
  svfloat32_t vzero = svdup_f32(0.0f);
  svint32_t zero_i = svdup_s32(0);
  svint32_t top_i = svdup_s32(0x7F800000);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svint32_t bits = svcvt_s32_f32_z(pg, svmul_f32_z(pg, vi, magic));
    bits = svadd_s32_z(pg, bits, vreinterpret_s32_f32(bias));
    bits = svmax_s32_z(pg, bits, zero_i);
    bits = svmin_s32_z(pg, bits, top_i);
    svst1_f32(pg, &out[i], svmax_f32_z(pg, vreinterpret_f32_s32(bits), vzero));
  }
  for (; i < n; i++)
    out[i] = scl_ml_fast_exp(in[i]);
}

static void scl_ml_simd_sve_vlog(float *SCL_RESTRICT out,
                                 const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vzero = svdup_f32(0.0f);
  svfloat32_t vneg = svdup_f32(-FLT_MAX);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svbool_t mask = svcmpgt_f32(pg, vi, vzero);
    svfloat32_t vln = svdup_f32(-FLT_MAX);
    for (int j = 0; j < cnt; j++) {
      float vf = svget_f32(vi, j);
      if (vf > 0.0f)
        svset_f32(vln, j, logf(vf));
    }
    svst1_f32(pg, &out[i], svsel_f32(mask, vln, vneg));
  }
  for (; i < n; i++)
    out[i] = in[i] > 0.0f ? logf(in[i]) : -FLT_MAX;
}

static void scl_ml_simd_sve_vsqrt(float *SCL_RESTRICT out,
                                  const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vzero = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svbool_t mask = svcmpge_f32(pg, vi, vzero);
    svst1_f32(pg, &out[i], svsel_f32(mask, svsqrt_f32_z(mask, vi), vzero));
  }
  for (; i < n; i++)
    out[i] = in[i] >= 0.0f ? sqrtf(in[i]) : 0.0f;
}

static void scl_ml_simd_sve_vrsqrt(float *SCL_RESTRICT out,
                                   const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svfloat32_t vs = svrsqrte_f32(vi);
    vs = svmul_f32_z(pg, vs, svrsqrts_f32(svmul_f32_z(pg, vi, vs), vs));
    vs = svmul_f32_z(pg, vs, svrsqrts_f32(svmul_f32_z(pg, vi, vs), vs));
    svst1_f32(pg, &out[i], vs);
  }
  for (; i < n; i++) {
    float v = in[i];
    out[i] = v > 0.0f ? 1.0f / sqrtf(v) : 0.0f;
  }
}

static void scl_ml_simd_sve_vinv(float *SCL_RESTRICT out,
                                 const float *SCL_RESTRICT in, size_t n) {
  svbool_t pg = svptrue_b32();
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svfloat32_t vs = svrecpe_f32(vi);
    vs = svmul_f32_z(pg, vs, svrecps_f32(vi, vs));
    vs = svmul_f32_z(pg, vs, svrecps_f32(vi, vs));
    svst1_f32(pg, &out[i], vs);
  }
  for (; i < n; i++)
    out[i] = in[i] != 0.0f ? 1.0f / in[i] : 0.0f;
}

/* ── Distance: vector–vector ─────────────────────────────────── */
static float scl_ml_simd_sve_dist_l2_sq(const float *SCL_RESTRICT a,
                                        const float *SCL_RESTRICT b, size_t d) {
  svbool_t pg = svptrue_b32();
  svfloat32_t acc = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= d; i += cnt) {
    svfloat32_t diff =
        svsub_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i]));
    acc = svmla_f32_z(pg, acc, diff, diff);
  }
  float result = svaddv_f32(pg, acc);
  for (; i < d; i++) {
    float df = a[i] - b[i];
    result += df * df;
  }
  return result;
}

static float scl_ml_simd_sve_dist_l1(const float *SCL_RESTRICT a,
                                     const float *SCL_RESTRICT b, size_t d) {
  svbool_t pg = svptrue_b32();
  svfloat32_t acc = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= d; i += cnt) {
    svfloat32_t diff =
        svsub_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i]));
    acc = svadd_f32_z(pg, acc, svabs_f32(diff));
  }
  float result = svaddv_f32(pg, acc);
  for (; i < d; i++)
    result += fabsf(a[i] - b[i]);
  return result;
}

static float scl_ml_simd_sve_dist_cos(const float *SCL_RESTRICT a,
                                      const float *SCL_RESTRICT b, size_t d) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vdot = svdup_f32(0.0f);
  svfloat32_t vna = svdup_f32(0.0f);
  svfloat32_t vnb = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= d; i += cnt) {
    svfloat32_t va = svld1_f32(pg, &a[i]);
    svfloat32_t vb = svld1_f32(pg, &b[i]);
    vdot = svmla_f32_z(pg, vdot, va, vb);
    vna = svmla_f32_z(pg, vna, va, va);
    vnb = svmla_f32_z(pg, vnb, vb, vb);
  }
  float dot = svaddv_f32(pg, vdot);
  float na = svaddv_f32(pg, vna);
  float nb = svaddv_f32(pg, vnb);
  for (; i < d; i++) {
    dot += (double)a[i] * b[i];
    na += (double)a[i] * a[i];
    nb += (double)b[i] * b[i];
  }
  double denom = sqrt(na * nb);
  if (denom < FLT_MIN)
    return 1.0f;
  return (float)(1.0 - dot / denom);
}

static float scl_ml_simd_sve_dist_cheb(const float *SCL_RESTRICT a,
                                       const float *SCL_RESTRICT b, size_t d) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vmax = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= d; i += cnt) {
    svfloat32_t adiff =
        svabs_f32(svsub_f32_z(pg, svld1_f32(pg, &a[i]), svld1_f32(pg, &b[i])));
    vmax = svmax_f32_z(pg, vmax, adiff);
  }
  float result = svmaxv_f32(pg, vmax);
  for (; i < d; i++) {
    float df = fabsf(a[i] - b[i]);
    if (df > result)
      result = df;
  }
  return result;
}

static float scl_ml_simd_sve_dist_l2(const float *SCL_RESTRICT a,
                                     const float *SCL_RESTRICT b, size_t d) {
  return sqrtf(scl_ml_simd_sve_dist_l2_sq(a, b, d));
}

/* ── Distance matrix ──────────────────────────────────────────── */
static void scl_ml_simd_sve_dist_matrix_l2_sq(float *SCL_RESTRICT out,
                                              const float *SCL_RESTRICT a,
                                              const float *SCL_RESTRICT b,
                                              size_t n, size_t m, size_t d) {
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < m; j++)
      out[i * m + j] = scl_ml_simd_sve_dist_l2_sq(&a[i * d], &b[j * d], d);
}

static void scl_ml_simd_sve_dist_matrix_l1(float *SCL_RESTRICT out,
                                           const float *SCL_RESTRICT a,
                                           const float *SCL_RESTRICT b,
                                           size_t n, size_t m, size_t d) {
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < m; j++)
      out[i * m + j] = scl_ml_simd_sve_dist_l1(&a[i * d], &b[j * d], d);
}

static void scl_ml_simd_sve_dist_matrix_cos(float *SCL_RESTRICT out,
                                            const float *SCL_RESTRICT a,
                                            const float *SCL_RESTRICT b,
                                            size_t n, size_t m, size_t d) {
  for (size_t i = 0; i < n; i++)
    for (size_t j = 0; j < m; j++)
      out[i * m + j] = scl_ml_simd_sve_dist_cos(&a[i * d], &b[j * d], d);
}

/* ── BLAS Level 2: gemv ───────────────────────────────────────── */
static void scl_ml_simd_sve_gemv(float *SCL_RESTRICT y,
                                 const float *SCL_RESTRICT a,
                                 const float *SCL_RESTRICT x, size_t m,
                                 size_t n, float beta) {
  svbool_t pg = svptrue_b32();
  int cnt = scl_sve_f32_cnt();
  for (size_t i = 0; i < m; i++) {
    svfloat32_t acc = svdup_f32(0.0f);
    size_t j = 0;
    for (; j + (size_t)cnt <= n; j += cnt) {
      svfloat32_t va = svld1_f32(pg, &a[i * n + j]);
      svfloat32_t vx = svld1_f32(pg, &x[j]);
      acc = svmla_f32_z(pg, acc, va, vx);
    }
    float result = svaddv_f32(pg, acc);
    for (; j < n; j++)
      result += a[i * n + j] * x[j];
    y[i] = beta * y[i] + result;
  }
}

static void scl_ml_simd_sve_gemv_t(float *SCL_RESTRICT y,
                                   const float *SCL_RESTRICT a,
                                   const float *SCL_RESTRICT x, size_t m,
                                   size_t n, float beta) {
  svbool_t pg = svptrue_b32();
  int cnt = scl_sve_f32_cnt();
  for (size_t j = 0; j < n; j++)
    y[j] *= beta;
  for (size_t i = 0; i < m; i++) {
    svfloat32_t vxi = svdup_f32(x[i]);
    const float *row = &a[i * n];
    size_t j = 0;
    for (; j + (size_t)cnt <= n; j += cnt) {
      svfloat32_t vy = svld1_f32(pg, &y[j]);
      svfloat32_t va = svld1_f32(pg, &row[j]);
      svst1_f32(pg, &y[j], svmla_f32_z(pg, vy, vxi, va));
    }
    for (; j < n; j++)
      y[j] += x[i] * row[j];
  }
}

/* ── BLAS Level 3: gemm (cache-tiled) ─────────────────────────── */
static void scl_ml_simd_sve_gemm(float *SCL_RESTRICT c,
                                 const float *SCL_RESTRICT a,
                                 const float *SCL_RESTRICT b, size_t m,
                                 size_t n, size_t k, float alpha, float beta) {
  svbool_t pg = svptrue_b32();
  int cnt = scl_sve_f32_cnt();
  for (size_t i = 0; i < m; i++) {
    size_t j = 0;
    for (; j + (size_t)cnt <= n; j += cnt) {
      svfloat32_t vc = svld1_f32(pg, &c[i * n + j]);
      svst1_f32(pg, &c[i * n + j], svmul_f32_z(pg, vc, svdup_f32(beta)));
    }
    for (; j < n; j++)
      c[i * n + j] *= beta;
  }
  const size_t T = 32;
  for (size_t i0 = 0; i0 < m; i0 += T) {
    size_t imax = i0 + T < m ? i0 + T : m;
    for (size_t j0 = 0; j0 < n; j0 += T) {
      size_t jmax = j0 + T < n ? j0 + T : n;
      for (size_t k0 = 0; k0 < k; k0 += T) {
        size_t kmax = k0 + T < k ? k0 + T : k;
        for (size_t i = i0; i < imax; i++) {
          for (size_t kk = k0; kk < kmax; kk++) {
            svfloat32_t vaik = svdup_f32(alpha * a[i * k + kk]);
            size_t j = j0;
            for (; j + (size_t)cnt <= jmax; j += cnt) {
              svfloat32_t vb = svld1_f32(pg, &b[kk * n + j]);
              svfloat32_t vc = svld1_f32(pg, &c[i * n + j]);
              svst1_f32(pg, &c[i * n + j], svmla_f32_z(pg, vc, vaik, vb));
            }
            for (; j < jmax; j++)
              c[i * n + j] += alpha * a[i * k + kk] * b[kk * n + j];
          }
        }
      }
    }
  }
}

/* ── Comparison / Selection ───────────────────────────────────── */
static void scl_ml_simd_sve_threshold(float *SCL_RESTRICT out,
                                      const float *SCL_RESTRICT in, float t,
                                      size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vt = svdup_f32(t);
  svfloat32_t one = svdup_f32(1.0f);
  svfloat32_t zero = svdup_f32(0.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svbool_t mask = svcmpgt_f32(pg, vi, vt);
    svst1_f32(pg, &out[i], svsel_f32(mask, one, zero));
  }
  for (; i < n; i++)
    out[i] = in[i] > t ? 1.0f : 0.0f;
}

static void scl_ml_simd_sve_threshold_sign(float *SCL_RESTRICT out,
                                           const float *SCL_RESTRICT in,
                                           float t, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vt = svdup_f32(t);
  svfloat32_t pos = svdup_f32(1.0f);
  svfloat32_t neg = svdup_f32(-1.0f);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svbool_t mask = svcmpge_f32(pg, vi, vt);
    svst1_f32(pg, &out[i], svsel_f32(mask, pos, neg));
  }
  for (; i < n; i++)
    out[i] = in[i] >= t ? 1.0f : -1.0f;
}

/* ── Hamming ──────────────────────────────────────────────────── */
static float scl_ml_simd_sve_hamming(const uint32_t *SCL_RESTRICT a,
                                     const uint32_t *SCL_RESTRICT b,
                                     size_t n_words) {
  svbool_t pg = svptrue_b32();
  svuint32_t acc = svdup_u32(0);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n_words; i += cnt) {
    svuint32_t va = svld1_u32(pg, &a[i]);
    svuint32_t vb = svld1_u32(pg, &b[i]);
    svuint32_t xored = sveor_u32_z(pg, va, vb);
    for (int j = 0; j < cnt; j++)
      acc[j] += (uint32_t)__builtin_popcount(xored[j]);
  }
  uint32_t result = svaddv_u32(pg, acc);
  for (; i < n_words; i++)
    result += (uint32_t)__builtin_popcount(a[i] ^ b[i]);
  return (float)result;
}

/* ── Top-K (scalar heap) ──────────────────────────────────────── */
static void scl_ml_simd_sve_topk_indices(const float *SCL_RESTRICT vals,
                                         uint32_t *SCL_RESTRICT indices,
                                         size_t n, size_t k) {
  scl_ml_simd_scalar_topk_indices(vals, indices, n, k);
}

/* ── Clamp ──────────────────────────────────────────────────────── */
static void scl_ml_simd_sve_clamp(float *SCL_RESTRICT out,
                                  const float *SCL_RESTRICT in, float lo,
                                  float hi, size_t n) {
  svbool_t pg = svptrue_b32();
  svfloat32_t vlo = svdup_f32(lo);
  svfloat32_t vhi = svdup_f32(hi);
  size_t i = 0;
  int cnt = scl_sve_f32_cnt();
  for (; i + (size_t)cnt <= n; i += cnt) {
    svfloat32_t vi = svld1_f32(pg, &in[i]);
    svst1_f32(pg, &out[i], svmin_f32_z(pg, svmax_f32_z(pg, vi, vlo), vhi));
  }
  for (; i < n; i++) {
    float v = in[i];
    out[i] = v < lo ? lo : (v > hi ? hi : v);
  }
}

/* ── Override table ─────────────────────────────────────────────── */
void scl_ml_simd_override_sve(scl_ml_simd_t *t) {
  t->dot = scl_ml_simd_sve_dot;
  t->dot_f = scl_ml_simd_sve_dot_f;
  t->norm_l2_sq = scl_ml_simd_sve_norm_l2_sq;
  t->norm_l2 = scl_ml_simd_sve_norm_l2;
  t->norm_l1 = scl_ml_simd_sve_norm_l1;
  t->add = scl_ml_simd_sve_add;
  t->sub = scl_ml_simd_sve_sub;
  t->mul = scl_ml_simd_sve_mul;
  t->div = scl_ml_simd_sve_div;
  t->abs = scl_ml_simd_sve_abs;
  t->fma = scl_ml_simd_sve_fma;
  t->add_s = scl_ml_simd_sve_add_s;
  t->mul_s = scl_ml_simd_sve_mul_s;
  t->scale_add_s = scl_ml_simd_sve_scale_add_s;
  t->sum = scl_ml_simd_sve_sum;
  t->max = scl_ml_simd_sve_max;
  t->min = scl_ml_simd_sve_min;
  t->argmax = scl_ml_simd_sve_argmax;
  t->argmin = scl_ml_simd_sve_argmin;
  t->argminmax = scl_ml_simd_sve_argminmax;
  t->sigmoid = scl_ml_simd_sve_sigmoid;
  t->relu = scl_ml_simd_sve_relu;
  t->relu6 = scl_ml_simd_sve_relu6;
  t->leaky_relu = scl_ml_simd_sve_leaky_relu;
  t->elu = scl_ml_simd_sve_elu;
  t->tanh_fast = scl_ml_simd_sve_tanh_fast;
  t->gelu = scl_ml_simd_sve_gelu;
  t->silu = scl_ml_simd_sve_silu;
  t->softmax = scl_ml_simd_sve_softmax;
  t->log_softmax = scl_ml_simd_sve_log_softmax;
  t->vexp = scl_ml_simd_sve_vexp;
  t->vlog = scl_ml_simd_sve_vlog;
  t->vsqrt = scl_ml_simd_sve_vsqrt;
  t->vrsqrt = scl_ml_simd_sve_vrsqrt;
  t->vinv = scl_ml_simd_sve_vinv;
  t->dist_l2_sq = scl_ml_simd_sve_dist_l2_sq;
  t->dist_l2 = scl_ml_simd_sve_dist_l2;
  t->dist_l1 = scl_ml_simd_sve_dist_l1;
  t->dist_cos = scl_ml_simd_sve_dist_cos;
  t->dist_cheb = scl_ml_simd_sve_dist_cheb;
  t->dist_matrix_l2_sq = scl_ml_simd_sve_dist_matrix_l2_sq;
  t->dist_matrix_cos = scl_ml_simd_sve_dist_matrix_cos;
  t->dist_matrix_l1 = scl_ml_simd_sve_dist_matrix_l1;
  t->gemv = scl_ml_simd_sve_gemv;
  t->gemv_t = scl_ml_simd_sve_gemv_t;
  t->gemm = scl_ml_simd_sve_gemm;
  t->threshold = scl_ml_simd_sve_threshold;
  t->threshold_sign = scl_ml_simd_sve_threshold_sign;
  t->hamming = scl_ml_simd_sve_hamming;
  t->topk_indices = scl_ml_simd_sve_topk_indices;
  t->clamp = scl_ml_simd_sve_clamp;
}

#else  /* !__ARM_FEATURE_SVE */
void scl_ml_simd_override_sve(scl_ml_simd_t *t) { (void)t; }
#endif /* __ARM_FEATURE_SVE */