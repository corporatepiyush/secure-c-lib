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

/* KMP pattern matcher. O(N+M). Prefix function (pi) skips backtracking on mismatch. */

#include "scl_kmp.h"
#include <string.h>

scl_error_t scl_search_kmp(scl_allocator_t * alloc, const char * text, size_t tlen, const char * pat, size_t plen, size_t * pos)
{
    if (scl_unlikely(text == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(pat == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(pos == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(tlen == 0)) return SCL_ERR_EMPTY;
    if (scl_unlikely(plen == 0)) return SCL_ERR_INVALID_ARG;
    if (scl_unlikely(plen > tlen)) return SCL_ERR_NOT_FOUND;

    size_t *lps = (size_t *)scl_calloc(alloc, plen, sizeof(size_t), alignof(max_align_t));
    if (scl_unlikely(lps == NULL)) return SCL_ERR_OUT_OF_MEMORY;

    size_t len = 0, i = 1;
    while (i < plen) {
        if (pat[i] == pat[len]) {
            lps[i++] = ++len;
        } else if (len) {
            len = lps[len - 1];
        } else {
            lps[i++] = 0;
        }
    }

    i = 0;
    size_t j = 0;
    scl_error_t result = SCL_ERR_NOT_FOUND;
    while (i < tlen) {
        if (pat[j] == text[i]) {
            i++; j++;
        }
        if (j == plen) {
            *pos = i - j;
            result = SCL_OK;
            break;
        } else if (i < tlen && pat[j] != text[i]) {
            if (j) j = lps[j - 1];
            else i++;
        }
    }

    scl_free(alloc, lps);
    return result;
}
