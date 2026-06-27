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

/* Counting sort. O(N+k) on integer keys. Non-comparison. Stable. Requires key-range precomputation. */

#include "scl_counting.h"
#include <string.h>

scl_error_t scl_sort_counting_sort(scl_allocator_t *alloc, int32_t * base, size_t count)
{
    if (scl_unlikely(!base)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count < 2)) return SCL_OK;

    int32_t min_val = base[0], max_val = base[0];
    for (size_t i = 1; i < count; i++) {
        if (base[i] < min_val) min_val = base[i];
        if (base[i] > max_val) max_val = base[i];
    }

    size_t range;
    if (scl_unlikely(scl_add_overflow((size_t)(max_val - min_val), 1, &range)))
        return SCL_ERR_SIZE_OVERFLOW;

    size_t *counts = (size_t *)scl_calloc(alloc, range, sizeof(size_t), alignof(max_align_t));
    if (scl_unlikely(!counts)) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < count; i++)
        counts[(size_t)(base[i] - min_val)]++;

    size_t idx = 0;
    for (size_t i = 0; i < range; i++) {
        for (size_t j = 0; j < counts[i]; j++)
            base[idx++] = (int32_t)((size_t)min_val + i);
    }

    scl_free(alloc, counts);
    return SCL_OK;
}
