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

/* Exponential search. O(log N). Doubles range then binary. Good for unbounded arrays. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_exponential.h"

static scl_error_t binary_search_range(const void *base, size_t lo, size_t hi, size_t elem_size, const void *key, scl_cmp_func_t cmp, size_t *out_index)
{
    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        const void *elem = (const unsigned char *)base + mid * elem_size;
        int r = cmp(elem, key);
        if (r == 0) { *out_index = mid; return SCL_OK; }
        if (r < 0) lo = mid + 1;
        else hi = (mid == 0) ? 0 : mid - 1;
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_search_exponential_search(const void * base, size_t count, size_t elem_size, const void * key, scl_cmp_func_t cmp, size_t * out_index)
{
    if (scl_unlikely(base == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(cmp == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    if (cmp(base, key) == 0) {
        *out_index = 0;
        return SCL_OK;
    }
    if (count == 1) return SCL_ERR_NOT_FOUND;

    size_t bound = 1;
    const unsigned char *bytes = (const unsigned char *)base;
    while (bound < count) {
        const void *elem = bytes + bound * elem_size;
        int r = cmp(elem, key);
        if (r >= 0) {
            size_t hi = bound;
            if (hi >= count) hi = count - 1;
            return binary_search_range(base, bound / 2, hi, elem_size, key, cmp, out_index);
        }
        bound *= 2;
    }
    return binary_search_range(base, bound / 2, count - 1, elem_size, key, cmp, out_index);
}
