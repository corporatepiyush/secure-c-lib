#ifndef SCL_MATH_H
#define SCL_MATH_H

#include "scl_common.h"
#include <math.h>

/*
 * scl_math — thin wrappers over <math.h> that:
 *   1. Return NaN / 0 on invalid input rather than UB
 *   2. Expose a single include point for all floating-point math
 *
 * All double-precision unless a suffix (_f / _l) is used.
 */

/* ── Exponential / logarithm ─────────────────────────────────── */
SCL_PURE double scl_exp(double x);
SCL_PURE double scl_log(double x);      /* natural log; returns -∞ if x <= 0 */
SCL_PURE double scl_log2(double x);
SCL_PURE double scl_log10(double x);
SCL_PURE double scl_pow(double base, double exp);
SCL_PURE double scl_sqrt(double x);     /* returns NaN for x < 0 */
SCL_PURE double scl_cbrt(double x);

/* ── Rounding ────────────────────────────────────────────────── */
SCL_PURE double scl_floor(double x);
SCL_PURE double scl_ceil(double x);
SCL_PURE double scl_round(double x);
SCL_PURE double scl_trunc(double x);
SCL_PURE double scl_fmod(double x, double y);   /* returns NaN if y == 0 */

/* ── Trigonometry ────────────────────────────────────────────── */
SCL_PURE double scl_sin(double x);
SCL_PURE double scl_cos(double x);
SCL_PURE double scl_tan(double x);
SCL_PURE double scl_asin(double x);
SCL_PURE double scl_acos(double x);
SCL_PURE double scl_atan(double x);
SCL_PURE double scl_atan2(double y, double x);

/* ── Hyperbolic ──────────────────────────────────────────────── */
SCL_PURE double scl_sinh(double x);
SCL_PURE double scl_cosh(double x);
SCL_PURE double scl_tanh(double x);

/* ── Utility ─────────────────────────────────────────────────── */
SCL_PURE double scl_fabs(double x);
SCL_PURE double scl_copysign(double mag, double sgn);
SCL_PURE int    scl_isnan(double x);
SCL_PURE int    scl_isinf(double x);
SCL_PURE int    scl_isfinite(double x);

/* ── Integer helpers (no libc dependency) ───────────────────── */
static inline int64_t  scl_min_i64(int64_t a, int64_t b)  { return a < b ? a : b; }
static inline int64_t  scl_max_i64(int64_t a, int64_t b)  { return a > b ? a : b; }
static inline uint64_t scl_min_u64(uint64_t a, uint64_t b){ return a < b ? a : b; }
static inline uint64_t scl_max_u64(uint64_t a, uint64_t b){ return a > b ? a : b; }
static inline double   scl_min_f64(double a, double b)    { return a < b ? a : b; }
static inline double   scl_max_f64(double a, double b)    { return a > b ? a : b; }
static inline double   scl_clamp_f64(double x, double lo, double hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

#endif /* SCL_MATH_H */
