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

/* Linear scan. O(N). Brute-force, no ordering requirement. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_linear.h"

scl_error_t scl_search_linear_search(const void * base, size_t count, size_t elem_size, const void * key, scl_cmp_func_t cmp, size_t * out_index)
{
    if (scl_unlikely(base == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(cmp == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    for (size_t i = 0; i < count; i++) {
        const void *elem = (const unsigned char *)base + i * elem_size;
        if (scl_unlikely(cmp(elem, key) == 0)) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
