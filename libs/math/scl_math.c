#include "scl_math.h"
#include <math.h>

double scl_exp(double x)               { return exp(x); }
double scl_log(double x)               { return x > 0.0 ? log(x) : (x == 0.0 ? -HUGE_VAL : NAN); }
double scl_log2(double x)              { return x > 0.0 ? log2(x) : (x == 0.0 ? -HUGE_VAL : NAN); }
double scl_log10(double x)             { return x > 0.0 ? log10(x) : (x == 0.0 ? -HUGE_VAL : NAN); }
double scl_pow(double base, double exp_){ return pow(base, exp_); }
double scl_sqrt(double x)              { return x >= 0.0 ? sqrt(x) : NAN; }
double scl_cbrt(double x)              { return cbrt(x); }

double scl_floor(double x)             { return floor(x); }
double scl_ceil(double x)              { return ceil(x); }
double scl_round(double x)             { return round(x); }
double scl_trunc(double x)             { return trunc(x); }
double scl_fmod(double x, double y)    { return y != 0.0 ? fmod(x, y) : NAN; }

double scl_sin(double x)               { return sin(x); }
double scl_cos(double x)               { return cos(x); }
double scl_tan(double x)               { return tan(x); }
double scl_asin(double x)              { return asin(x); }
double scl_acos(double x)              { return acos(x); }
double scl_atan(double x)              { return atan(x); }
double scl_atan2(double y, double x)   { return atan2(y, x); }

double scl_sinh(double x)              { return sinh(x); }
double scl_cosh(double x)              { return cosh(x); }
double scl_tanh(double x)              { return tanh(x); }

double scl_fabs(double x)              { return fabs(x); }
double scl_copysign(double mag, double sgn) { return copysign(mag, sgn); }
int    scl_isnan(double x)             { return isnan(x); }
int    scl_isinf(double x)             { return isinf(x); }
int    scl_isfinite(double x)          { return isfinite(x); }
