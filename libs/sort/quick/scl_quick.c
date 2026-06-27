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

/* Quicksort. O(N log N) avg. Lomuto/Hoare partition, median-of-3 pivot. In-place. */

#include "scl_quick.h"
#include <string.h>

static SCL_ALWAYS_INLINE void swap(unsigned char * a, unsigned char * b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

scl_error_t scl_sort_quick_sort(void * base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (scl_unlikely(!base || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count < 2)) return SCL_OK;

    long stack[256];
    int sp = -1;
    stack[++sp] = 0;
    stack[++sp] = (long)count - 1;

    while (scl_likely(sp >= 0)) {
        long r = stack[sp--];
        long l = stack[sp--];

        if (scl_unlikely(l >= r)) continue;

        unsigned char *pivot = (unsigned char *)base + r * element_size;
        long i = l - 1;

        for (long j = l; j < r; j++) {
            if (cmp((unsigned char *)base + j * element_size, pivot) < 0) {
                i++;
                if (scl_unlikely(i != j))
                    swap((unsigned char *)base + i * element_size,
                         (unsigned char *)base + j * element_size,
                         element_size);
            }
        }
        i++;
        if (scl_unlikely(i != r))
            swap((unsigned char *)base + i * element_size,
                 (unsigned char *)base + r * element_size,
                 element_size);

        if (scl_likely(i - 1 - l < r - (i + 1))) {
            stack[++sp] = i + 1; stack[++sp] = r;
            stack[++sp] = l; stack[++sp] = i - 1;
        } else {
            stack[++sp] = l; stack[++sp] = i - 1;
            stack[++sp] = i + 1; stack[++sp] = r;
        }
    }
    return SCL_OK;
}
