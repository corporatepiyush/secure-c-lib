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

/* Hardened math wrappers returning NaN on domain error (no UB). Wraps
 * exp/log/pow/sqrt/trig with rigorous pre-call argument validation before
 * calling libc.
 *
 * All functions are static inline so the compiler can inline them at -O2
 * without LTO (important for the ML scalar hot path). */

#ifndef SCL_MATH_H
#define SCL_MATH_H

#include "scl_common.h"
#include <math.h>

/* ── Exponential / logarithm ─────────────────────────────────── */
static inline double scl_exp(double x) { return exp(x); }
static inline double scl_log(double x) {
  return x > 0.0 ? log(x) : (x == 0.0 ? -HUGE_VAL : NAN);
}
static inline double scl_log2(double x) {
  return x > 0.0 ? log2(x) : (x == 0.0 ? -HUGE_VAL : NAN);
}
static inline double scl_log10(double x) {
  return x > 0.0 ? log10(x) : (x == 0.0 ? -HUGE_VAL : NAN);
}
static inline double scl_pow(double base, double exp_) { return pow(base, exp_); }
static inline double scl_sqrt(double x) { return x >= 0.0 ? sqrt(x) : NAN; }
static inline double scl_cbrt(double x) { return cbrt(x); }

/* ── Rounding ────────────────────────────────────────────────── */
static inline double scl_floor(double x) { return floor(x); }
static inline double scl_ceil(double x) { return ceil(x); }
static inline double scl_round(double x) { return round(x); }
static inline double scl_trunc(double x) { return trunc(x); }
static inline double scl_fmod(double x, double y) {
  return y != 0.0 ? fmod(x, y) : NAN;
}

/* ── Trigonometry ────────────────────────────────────────────── */
static inline double scl_sin(double x) { return sin(x); }
static inline double scl_cos(double x) { return cos(x); }
static inline double scl_tan(double x) { return tan(x); }
static inline double scl_asin(double x) { return asin(x); }
static inline double scl_acos(double x) { return acos(x); }
static inline double scl_atan(double x) { return atan(x); }
static inline double scl_atan2(double y, double x) { return atan2(y, x); }

/* ── Hyperbolic ──────────────────────────────────────────────── */
static inline double scl_sinh(double x) { return sinh(x); }
static inline double scl_cosh(double x) { return cosh(x); }
static inline double scl_tanh(double x) { return tanh(x); }

/* ── Utility ─────────────────────────────────────────────────── */
static inline double scl_fabs(double x) { return fabs(x); }
static inline double scl_copysign(double mag, double sgn) {
  return copysign(mag, sgn);
}
static inline int scl_isnan(double x) { return isnan(x); }
static inline int scl_isinf(double x) { return isinf(x); }
static inline int scl_isfinite(double x) { return isfinite(x); }

/* ── Integer helpers (no libc dependency) ───────────────────── */
static inline int64_t scl_min_i64(int64_t a, int64_t b) {
  return a < b ? a : b;
}
static inline int64_t scl_max_i64(int64_t a, int64_t b) {
  return a > b ? a : b;
}
static inline uint64_t scl_min_u64(uint64_t a, uint64_t b) {
  return a < b ? a : b;
}
static inline uint64_t scl_max_u64(uint64_t a, uint64_t b) {
  return a > b ? a : b;
}
static inline double scl_min_f64(double a, double b) { return a < b ? a : b; }
static inline double scl_max_f64(double a, double b) { return a > b ? a : b; }
static inline double scl_clamp_f64(double x, double lo, double hi) {
  return x < lo ? lo : (x > hi ? hi : x);
}

#endif /* SCL_MATH_H */