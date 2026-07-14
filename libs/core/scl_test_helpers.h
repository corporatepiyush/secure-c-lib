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

/* Shared test utilities for all secure-c-libs test files.
 *
 * Centralises:
 *   - a canonical int comparator  (test_int_cmp)
 *   - a deterministic xorshift RNG (test_rng / TEST_RNG)
 *   - the SCL_ML_NEAR floating-point assertion macro
 *
 * Every test file that previously defined its own copy of these
 * should #include this header and delete the local version.
 */

#ifndef SCL_TEST_HELPERS_H
#define SCL_TEST_HELPERS_H

#include "scl_test.h"
#include <math.h>
#include <stdint.h>

/* ── Canonical integer comparator ───────────────────────────────── */
static inline int test_int_cmp(const void *a, const void *b) {
  int va = *(const int *)a, vb = *(const int *)b;
  return (va > vb) - (va < vb);
}

/* ── Deterministic xorshift RNG ─────────────────────────────────── */
static inline uint32_t test_rng(uint32_t *state) {
  *state ^= *state << 13;
  *state ^= *state >> 17;
  *state ^= *state << 5;
  return *state;
}

#define TEST_RNG(state) test_rng(&(state))

/* ── Floating-point "near" assertion for ML tests ───────────────── */
#define SCL_ML_NEAR(tr, a, b, eps)                                             \
  do {                                                                         \
    float _d = (float)fabs((double)(a) - (double)(b));                         \
    scl_expect_true(tr, _d < (eps), __FILE__, __LINE__,                        \
                    "near check: " #a " ~ " #b);                               \
  } while (0)

#endif /* SCL_TEST_HELPERS_H */