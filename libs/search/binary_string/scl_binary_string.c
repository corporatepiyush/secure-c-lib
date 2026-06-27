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

/* Binary string search. O(log N). Midpoint string comparison on sorted array. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_binary_string.h"
#include <string.h>

scl_error_t scl_search_binary_string(const char **SCL_RESTRICT strs, size_t count, const char * key, size_t * idx)
{
    if (scl_unlikely(strs == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(idx == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (!strs[mid]) return SCL_ERR_INVALID_ARG;
        int r = strcmp(strs[mid], key);
        if (r == 0) { *idx = mid; return SCL_OK; }
        if (r < 0) lo = mid + 1;
        else hi = mid;
    }
    return SCL_ERR_NOT_FOUND;
}
