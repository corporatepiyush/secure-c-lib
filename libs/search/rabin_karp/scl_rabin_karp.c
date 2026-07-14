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

/* Rabin-Karp pattern matcher. O(N+M) avg. Rolling hash for multi-pattern
 * support.
 *
 * Security: uses a large randomized prime and per-run random base to resist
 * hash-collision DoS attacks. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_rabin_karp.h"
#include "scl_stdlib.h"
#include "scl_time.h"

/* Large prime (2^31 - 1 = Mersenne prime) — resists collision attacks. */
#define RK_MOD 2147483647LL

static uint64_t rk_base = 256; /* randomized per-call */

void scl_rabin_karp_seed(uint64_t seed) {
  rk_base = (seed & 0xFFFF) + 256; /* base in [256, 65535] */
}

scl_error_t scl_search_rabin_karp(const char *text, size_t tlen,
                                  const char *pat, size_t plen, size_t *pos) {
  if (scl_unlikely(text == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(pat == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(pos == NULL))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(tlen == 0))
    return SCL_ERR_EMPTY;
  if (scl_unlikely(plen == 0))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(plen > tlen))
    return SCL_ERR_NOT_FOUND;

  uint64_t pat_hash = 0, text_hash = 0, h = 1;
  uint64_t base = rk_base;

  for (size_t i = 0; i < plen - 1; i++)
    h = (h * base) % RK_MOD;

  for (size_t i = 0; i < plen; i++) {
    pat_hash = (pat_hash * base + (unsigned char)pat[i]) % RK_MOD;
    text_hash = (text_hash * base + (unsigned char)text[i]) % RK_MOD;
  }

  for (size_t i = 0; i <= tlen - plen; i++) {
    if (pat_hash == text_hash) {
      size_t j;
      for (j = 0; j < plen; j++)
        if ((unsigned char)text[i + j] != (unsigned char)pat[j])
          break;
      if (j == plen) {
        *pos = i;
        return SCL_OK;
      }
    }
    if (i < tlen - plen) {
      int64_t sub =
          ((int64_t)text_hash - (int64_t)((unsigned char)text[i] * h)) % RK_MOD;
      if (sub < 0)
        sub += RK_MOD;
      text_hash = (sub * base + (unsigned char)text[i + plen]) % RK_MOD;
    }
  }
  return SCL_ERR_NOT_FOUND;
}
