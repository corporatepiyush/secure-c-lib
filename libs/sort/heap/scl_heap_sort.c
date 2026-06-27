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

/* Heapsort. O(N log N) all cases. In-place, not stable. Build-max-heap then extract. */

#include "scl_heap_sort.h"
#include <string.h>

static SCL_ALWAYS_INLINE void swap(unsigned char * a, unsigned char * b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

static void sift_down(unsigned char * base, size_t start, size_t end,
                      size_t element_size, scl_cmp_func_t cmp)
{
    size_t root = start;
    while (root * 2 + 1 <= end) {
        size_t child = root * 2 + 1;
        size_t swp = root;
        if (cmp(base + swp * element_size, base + child * element_size) < 0)
            swp = child;
        if (child + 1 <= end &&
            cmp(base + swp * element_size, base + (child + 1) * element_size) < 0)
            swp = child + 1;
        if (swp == root) break;
        swap(base + root * element_size, base + swp * element_size, element_size);
        root = swp;
    }
}

scl_error_t scl_sort_heap_sort(void * base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    unsigned char *ptr = (unsigned char *)base;
    if (scl_unlikely(!base || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count < 2)) return SCL_OK;

    for (size_t i = count / 2; i > 0; i--)
        sift_down(ptr, i - 1, count - 1, element_size, cmp);

    for (size_t i = count - 1; i > 0; i--) {
        swap(ptr, ptr + i * element_size, element_size);
        sift_down(ptr, 0, i - 1, element_size, cmp);
    }

    return SCL_OK;
}
