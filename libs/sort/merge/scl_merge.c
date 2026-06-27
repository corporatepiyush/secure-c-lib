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

/* Mergesort. O(N log N). Stable. Top-down or bottom-up. O(N) auxiliary space. */

#include "scl_merge.h"
#include "scl_string.h"

scl_error_t scl_sort_merge_sort(scl_allocator_t *alloc, void * base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (scl_unlikely(!base || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count < 2)) return SCL_OK;

    size_t bytes;
    if (scl_unlikely(scl_mul_overflow(count, element_size, &bytes)))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t width = 1; scl_likely(width < count); width *= 2) {
        for (size_t i = 0; i < count; i += 2 * width) {
            size_t mid = i + width;
            if (scl_unlikely(mid >= count)) continue;
            size_t r = i + 2 * width;
            if (r > count) r = count;

            size_t li = i, ri = mid, ti = 0;

            while (scl_likely(li < mid && ri < r)) {
                if (cmp((unsigned char *)base + li * element_size,
                        (unsigned char *)base + ri * element_size) <= 0)
                    scl_memcpy(tmp + ti++ * element_size,
                           (unsigned char *)base + li++ * element_size,
                           element_size);
                else
                    scl_memcpy(tmp + ti++ * element_size,
                           (unsigned char *)base + ri++ * element_size,
                           element_size);
            }
            while (li < mid)
                scl_memcpy(tmp + ti++ * element_size,
                       (unsigned char *)base + li++ * element_size,
                       element_size);
            while (ri < r)
                scl_memcpy(tmp + ti++ * element_size,
                       (unsigned char *)base + ri++ * element_size,
                       element_size);

            scl_memcpy((unsigned char *)base + i * element_size, tmp, ti * element_size);
        }
    }

    scl_free(alloc, tmp);
    return SCL_OK;
}
