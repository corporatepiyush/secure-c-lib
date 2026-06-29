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

/* Interpolation search. O(log log N) avg on uniform data. Probe by value. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_interpolation.h"

scl_error_t scl_search_interpolation_search(const int64_t * arr, size_t count, int64_t key, size_t * out_index)
{
    if (scl_unlikely(arr == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    size_t lo = 0, hi = count - 1;

    while (lo <= hi && key >= arr[lo] && key <= arr[hi]) {
        if (scl_unlikely(lo == hi)) {
            if (arr[lo] == key) {
                *out_index = lo;
                return SCL_OK;
            }
            break;
        }
        if (arr[hi] == arr[lo]) break;
        /* Compute the interpolation fraction in floating point: subtracting two
         * int64 values directly (key-arr[lo], arr[hi]-arr[lo]) overflows — and
         * is undefined behaviour — for adversarial ranges like {INT64_MIN,
         * INT64_MAX}. Widening to double first cannot overflow. */
        double frac = ((double)key - (double)arr[lo]) / ((double)arr[hi] - (double)arr[lo]);
        if (frac < 0.0) frac = 0.0; else if (frac > 1.0) frac = 1.0;
        size_t pos = lo + (size_t)(frac * (double)(hi - lo));
        if (pos < lo) pos = lo;
        if (pos > hi) pos = hi;

        if (arr[pos] == key) {
            *out_index = pos;
            return SCL_OK;
        }
        if (arr[pos] < key) lo = pos + 1;
        else hi = (pos == 0) ? 0 : pos - 1;
    }
    return SCL_ERR_NOT_FOUND;
}
