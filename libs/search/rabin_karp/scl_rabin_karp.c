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

/* Rabin-Karp pattern matcher. O(N+M) avg. Rolling hash for multi-pattern support. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_rabin_karp.h"
#include <stdlib.h>

#define RK_BASE 256
#define RK_MOD 1000000007

scl_error_t scl_search_rabin_karp(const char * text, size_t tlen, const char * pat, size_t plen, size_t * pos)
{
    if (scl_unlikely(text == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(pat == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(pos == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(tlen == 0)) return SCL_ERR_EMPTY;
    if (scl_unlikely(plen == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(plen > tlen)) return SCL_ERR_NOT_FOUND;

    int64_t pat_hash = 0, text_hash = 0, h = 1;

    for (size_t i = 0; i < plen - 1; i++)
        h = (h * RK_BASE) % RK_MOD;

    for (size_t i = 0; i < plen; i++) {
        pat_hash = (pat_hash * RK_BASE + pat[i]) % RK_MOD;
        text_hash = (text_hash * RK_BASE + text[i]) % RK_MOD;
    }

    for (size_t i = 0; i <= tlen - plen; i++) {
        if (pat_hash == text_hash) {
            size_t j;
            for (j = 0; j < plen; j++)
                if (text[i + j] != pat[j]) break;
            if (j == plen) {
                *pos = i;
                return SCL_OK;
            }
        }
        if (i < tlen - plen) {
            text_hash = (RK_BASE * (text_hash - text[i] * h) + text[i + plen]) % RK_MOD;
            if (text_hash < 0) text_hash += RK_MOD;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
