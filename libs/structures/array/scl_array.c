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

/* array data structure. */

#include "scl_array.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_array_init(scl_allocator_t *alloc, scl_array_t *arr, size_t element_size, size_t initial_capacity)
{
    if (scl_unlikely(!arr)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;

    arr->data = NULL;
    arr->element_size = element_size;
    arr->capacity = 0;
    arr->count = 0;

    if (initial_capacity > 0) {
        size_t bytes;
        if (scl_mul_overflow(initial_capacity, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        arr->data = scl_alloc(alloc, bytes, alignof(max_align_t));
        if (scl_unlikely(!arr->data)) return SCL_ERR_OUT_OF_MEMORY;
        arr->capacity = initial_capacity;
    }
    return SCL_OK;
}

void scl_array_destroy(scl_allocator_t *alloc, scl_array_t *arr)
{
    if (arr) {
        scl_free(alloc, arr->data);
        arr->data = NULL;
        arr->capacity = 0;
        arr->count = 0;
    }
}

SCL_COLD_PATH scl_error_t scl_array_reserve(scl_allocator_t *alloc, scl_array_t *arr, size_t new_capacity)
{
    if (scl_unlikely(!arr)) return SCL_ERR_NULL_PTR;
    if (new_capacity <= arr->capacity) return SCL_OK;

    size_t es = arr->element_size;
    size_t old_bytes, new_bytes;
    if (scl_mul_overflow(arr->capacity, es, &old_bytes))
        old_bytes = 0;
    if (scl_mul_overflow(new_capacity, es, &new_bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = scl_realloc(alloc, arr->data, old_bytes, new_bytes, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;

    arr->data = tmp;
    arr->capacity = new_capacity;
    return SCL_OK;
}

scl_error_t scl_array_push(scl_allocator_t *alloc, scl_array_t *arr, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!arr || !element)) return SCL_ERR_NULL_PTR;

    size_t cnt = arr->count;
    size_t es = arr->element_size;

    if (scl_unlikely(cnt == arr->capacity)) {
        size_t new_cap = arr->capacity == 0 ? 4 : arr->capacity * 2;
        scl_error_t err = scl_array_reserve(alloc, arr, new_cap);
        if (err != SCL_OK) return err;
    }

    scl_memcpy(arr->data + cnt * es, element, es);
    arr->count = cnt + 1;
    return SCL_OK;
}

scl_error_t scl_array_pop(scl_array_t *arr, void *out)
{
    if (scl_unlikely(!arr || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(arr->count == 0)) return SCL_ERR_EMPTY;

    size_t es = arr->element_size;
    arr->count--;
    scl_memcpy(out, arr->data + arr->count * es, es);
    return SCL_OK;
}

scl_error_t scl_array_get(const scl_array_t *arr, size_t index, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!arr || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(index >= arr->count)) return SCL_ERR_INVALID_INDEX;

    size_t es = arr->element_size;
    scl_memcpy(out, arr->data + index * es, es);
    return SCL_OK;
}

scl_error_t scl_array_set(scl_array_t *arr, size_t index, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!arr || !element)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(index >= arr->count)) return SCL_ERR_INVALID_INDEX;

    size_t es = arr->element_size;
    scl_memcpy(arr->data + index * es, element, es);
    return SCL_OK;
}

scl_error_t scl_array_insert(scl_allocator_t *alloc, scl_array_t *arr, size_t index, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!arr || !element)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(index > arr->count)) return SCL_ERR_INVALID_INDEX;

    size_t cnt = arr->count;
    size_t es = arr->element_size;

    if (scl_unlikely(cnt == arr->capacity)) {
        size_t new_cap = arr->capacity == 0 ? 4 : arr->capacity * 2;
        scl_error_t err = scl_array_reserve(alloc, arr, new_cap);
        if (err != SCL_OK) return err;
    }

    size_t tail = cnt - index;
    if (tail > 0) {
        scl_memmove(arr->data + (index + 1) * es,
                arr->data + index * es,
                tail * es);
    }

    scl_memcpy(arr->data + index * es, element, es);
    arr->count = cnt + 1;
    return SCL_OK;
}

scl_error_t scl_array_remove(scl_array_t *arr, size_t index, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!arr || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(index >= arr->count)) return SCL_ERR_INVALID_INDEX;

    size_t es = arr->element_size;
    scl_memcpy(out, arr->data + index * es, es);

    size_t tail = arr->count - index - 1;
    if (tail > 0) {
        scl_memmove(arr->data + index * es,
                arr->data + (index + 1) * es,
                tail * es);
    }
    arr->count--;
    return SCL_OK;
}

scl_error_t scl_array_shrink(scl_allocator_t *alloc, scl_array_t *arr)
{
    if (scl_unlikely(!arr)) return SCL_ERR_NULL_PTR;
    if (arr->count == arr->capacity) return SCL_OK;

    size_t old_bytes, new_bytes;
    if (scl_mul_overflow(arr->capacity, arr->element_size, &old_bytes))
        old_bytes = 0;
    if (scl_mul_overflow(arr->count, arr->element_size, &new_bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    if (arr->count == 0) {
        scl_free(alloc, arr->data);
        arr->data = NULL;
        arr->capacity = 0;
        return SCL_OK;
    }

    unsigned char *tmp = scl_realloc(alloc, arr->data, old_bytes, new_bytes, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;

    arr->data = tmp;
    arr->capacity = arr->count;
    return SCL_OK;
}

size_t scl_array_count(const scl_array_t *arr)
{
    return arr ? arr->count : 0;
}

size_t scl_array_capacity(const scl_array_t *arr)
{
    return arr ? arr->capacity : 0;
}

bool scl_array_empty(const scl_array_t *arr)
{
    return arr ? arr->count == 0 : true;
}

scl_error_t scl_array_linear_search(const scl_array_t *restrict arr, const void *restrict key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *restrict out_index)
{
    if (scl_unlikely(!arr || !key || !cmp || !out_index))
        return SCL_ERR_NULL_PTR;

    size_t cnt = arr->count;
    size_t es = arr->element_size;
    unsigned char *data = arr->data;

    for (size_t i = 0; i < cnt; i++) {
        if (cmp(data + i * es, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_array_binary_search(const scl_array_t *restrict arr, const void *restrict key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *restrict out_index)
{
    if (scl_unlikely(!arr || !key || !cmp || !out_index))
        return SCL_ERR_NULL_PTR;

    size_t lo = 0, hi = arr->count;
    size_t es = arr->element_size;
    unsigned char *data = arr->data;

    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(data + mid * es, key);
        if (c < 0)
            lo = mid + 1;
        else if (c > 0)
            hi = mid;
        else {
            *out_index = mid;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
