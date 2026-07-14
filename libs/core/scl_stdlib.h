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

/* Safe stdlib wrappers: scl_atoi (range-checked), scl_abs (INT_MIN-safe),
 * thread-local splitmix64 PRNG. Bounds-checks all conversions; returns clamped
 * values on overflow. */

#ifndef SCL_STDLIB_H
#define SCL_STDLIB_H

#include "scl_common.h"
#include <stddef.h>
#include <stdint.h>

/*
 * scl_stdlib.h — safe wrappers for <stdlib.h> functionality.
 *
 * Key improvements over raw libc:
 *  - NULL-pointer guard: every string argument is checked; NULL
 *    returns 0 / 0.0 / NULL (safe degradation, not UB).
 *  - Overflow-safe abs: scl_abs(INT_MIN) returns INT_MAX instead of UB
 *    (two's complement negation wraps around in C; we cap it).
 *  - strtol/strtoul wrappers clear errno before the call and detect
 *    ERANGE so callers can distinguish overflow from valid parsing.
 *  - scl_rand is backed by a CSPRNG (arc4random on BSD, /dev/urandom
 *    on Linux) — never the predictable libc rand().
 *  - scl_realloc wraps the pattern of allocate-copy-free so callers
 *    don't need to store the old allocation size.
 *
 * These wrappers let application code avoid #include <stdlib.h>
 * entirely; only this .c file touches the libc header.
 */

/* Integer conversion */
SCL_PURE int scl_atoi(const char *str);
SCL_PURE long scl_atol(const char *str);
SCL_PURE long long scl_atoll(const char *str);

long scl_strtol(const char *str, char **SCL_RESTRICT endptr, int base);
long long scl_strtoll(const char *str, char **SCL_RESTRICT endptr, int base);
unsigned long scl_strtoul(const char *str, char **SCL_RESTRICT endptr,
                          int base);
unsigned long long scl_strtoull(const char *str, char **SCL_RESTRICT endptr,
                                int base);

SCL_PURE double scl_atof(const char *str);
double scl_strtod(const char *str, char **SCL_RESTRICT endptr);

/* Absolute value — overflow-safe (INT_MIN → INT_MAX) */
SCL_CONST int scl_abs(int x);
SCL_CONST long scl_labs(long x);
SCL_CONST long long scl_llabs(long long x);

/* Random numbers — backed by system CSPRNG */
int scl_rand(void);
uint32_t scl_rand_u32(void);
uint64_t scl_rand_u64(void);
void scl_srand(unsigned int seed);

/* Environment */
char *scl_getenv(const char *name);

#endif // SCL_STDLIB_H
