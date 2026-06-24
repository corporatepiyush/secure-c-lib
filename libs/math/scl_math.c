#include "scl_math.h"
#include <math.h>

SCL_ALWAYS_INLINE double scl_exp(double x)               { return exp(x); }
SCL_ALWAYS_INLINE double scl_log(double x)               { return x > 0.0 ? log(x) : (x == 0.0 ? -HUGE_VAL : NAN); }
SCL_ALWAYS_INLINE double scl_log2(double x)              { return x > 0.0 ? log2(x) : (x == 0.0 ? -HUGE_VAL : NAN); }
SCL_ALWAYS_INLINE double scl_log10(double x)             { return x > 0.0 ? log10(x) : (x == 0.0 ? -HUGE_VAL : NAN); }
SCL_ALWAYS_INLINE double scl_pow(double base, double exp_){ return pow(base, exp_); }
SCL_ALWAYS_INLINE double scl_sqrt(double x)              { return x >= 0.0 ? sqrt(x) : NAN; }
SCL_ALWAYS_INLINE double scl_cbrt(double x)              { return cbrt(x); }

SCL_ALWAYS_INLINE double scl_floor(double x)             { return floor(x); }
SCL_ALWAYS_INLINE double scl_ceil(double x)              { return ceil(x); }
SCL_ALWAYS_INLINE double scl_round(double x)             { return round(x); }
SCL_ALWAYS_INLINE double scl_trunc(double x)             { return trunc(x); }
SCL_ALWAYS_INLINE double scl_fmod(double x, double y)    { return y != 0.0 ? fmod(x, y) : NAN; }

SCL_ALWAYS_INLINE double scl_sin(double x)               { return sin(x); }
SCL_ALWAYS_INLINE double scl_cos(double x)               { return cos(x); }
SCL_ALWAYS_INLINE double scl_tan(double x)               { return tan(x); }
SCL_ALWAYS_INLINE double scl_asin(double x)              { return asin(x); }
SCL_ALWAYS_INLINE double scl_acos(double x)              { return acos(x); }
SCL_ALWAYS_INLINE double scl_atan(double x)              { return atan(x); }
SCL_ALWAYS_INLINE double scl_atan2(double y, double x)   { return atan2(y, x); }

SCL_ALWAYS_INLINE double scl_sinh(double x)              { return sinh(x); }
SCL_ALWAYS_INLINE double scl_cosh(double x)              { return cosh(x); }
SCL_ALWAYS_INLINE double scl_tanh(double x)              { return tanh(x); }

SCL_ALWAYS_INLINE double scl_fabs(double x)              { return fabs(x); }
SCL_ALWAYS_INLINE double scl_copysign(double mag, double sgn) { return copysign(mag, sgn); }
SCL_ALWAYS_INLINE int    scl_isnan(double x)             { return isnan(x); }
SCL_ALWAYS_INLINE int    scl_isinf(double x)             { return isinf(x); }
SCL_ALWAYS_INLINE int    scl_isfinite(double x)          { return isfinite(x); }
