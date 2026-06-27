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

/* Unbounded binary search. O(log N). Exponential probe then binary on unknown-length array. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_unbounded_binary.h"

scl_error_t scl_search_unbounded_binary_search(scl_cmp_func_t cmp, const void * key, size_t * out_index, void *(*getter)(size_t index, void *ctx), void *ctx, size_t max_count)
{
    if (scl_unlikely(cmp == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(getter == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(max_count == 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = 1;
    void *elem = getter(0, ctx);
    if (scl_unlikely(elem == NULL)) return SCL_ERR_NOT_FOUND;
    if (cmp(elem, key) == 0) {
        *out_index = 0;
        return SCL_OK;
    }

    while (hi < max_count) {
        elem = getter(hi, ctx);
        if (!elem) break;
        int r = cmp(elem, key);
        if (r == 0) {
            *out_index = hi;
            return SCL_OK;
        }
        if (r > 0) break;
        lo = hi;
        hi *= 2;
    }
    if (hi >= max_count) hi = max_count - 1;
    if (hi < lo) hi = lo;

    while (lo <= hi) {
        size_t mid = lo + (hi - lo) / 2;
        elem = getter(mid, ctx);
        if (!elem) { hi = (mid == 0) ? 0 : mid - 1; continue; }
        int r = cmp(elem, key);
        if (r == 0) {
            *out_index = mid;
            return SCL_OK;
        }
        if (r < 0) lo = mid + 1;
        else hi = (mid == 0) ? 0 : mid - 1;
    }
    return SCL_ERR_NOT_FOUND;
}
