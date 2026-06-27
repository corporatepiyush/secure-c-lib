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

/* Sentinel linear search. Places target at end to elide bounds check per iteration. */

#include "scl_sentinel_linear.h"
#include "scl_string.h"

scl_error_t scl_search_sentinel_linear_search(scl_allocator_t * alloc, const void * base, size_t count, size_t elem_size, const void * key, scl_cmp_func_t cmp, size_t * out_index)
{
    if (scl_unlikely(base == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(cmp == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    unsigned char *buf = (unsigned char *)scl_alloc(alloc, elem_size, alignof(max_align_t));
    if (scl_unlikely(buf == NULL)) return SCL_ERR_OUT_OF_MEMORY;
    scl_memcpy(buf, key, elem_size);

    size_t bytes;
    if (scl_mul_overflow(count, elem_size, &bytes)) { scl_free(alloc, buf); return SCL_ERR_SIZE_OVERFLOW; }
    unsigned char *copy = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(copy == NULL)) { scl_free(alloc, buf); return SCL_ERR_OUT_OF_MEMORY; }
    scl_memcpy(copy, base, bytes);

    size_t i = 0;
    while (1) {
        if (i < count) {
            int r = cmp(copy + i * elem_size, buf);
            if (r == 0) break;
        }
        i++;
        if (i > count) break;
    }

    scl_free(alloc, buf);
    scl_free(alloc, copy);

    if (i < count) {
        *out_index = i;
        return SCL_OK;
    }
    return SCL_ERR_NOT_FOUND;
}
