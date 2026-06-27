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

/* Thread-safe array data structure. Guarded by scl_spinlock_t. */

#include "scl_concurrent_array.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_carray_init(scl_allocator_t *alloc, scl_concurrent_array_t *arr, size_t element_size, size_t capacity)
{
    if (scl_unlikely(!arr)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0 || capacity == 0)) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    arr->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!arr->data)) return SCL_ERR_OUT_OF_MEMORY;
    arr->element_size = element_size;
    arr->capacity = capacity;
    atomic_init(&arr->count, 0);
    return SCL_OK;
}

void scl_carray_destroy(scl_allocator_t *alloc, scl_concurrent_array_t *arr)
{
    if (arr) {
        scl_free(alloc, arr->data);
        arr->data = NULL;
        arr->capacity = 0;
        atomic_store_explicit(&arr->count, 0, memory_order_relaxed);
    }
}

scl_error_t scl_carray_push(scl_allocator_t *alloc, scl_concurrent_array_t *arr, const void  *SCL_RESTRICT element)
{
    (void)alloc;
    if (scl_unlikely(!arr || !element)) return SCL_ERR_NULL_PTR;
    size_t old = atomic_load_explicit(&arr->count, memory_order_relaxed);
    while (1) {
        if (scl_unlikely(old >= arr->capacity)) return SCL_ERR_FULL;
        if (atomic_compare_exchange_weak_explicit(&arr->count, &old, old + 1,
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    size_t offset;
    if (scl_mul_overflow(old, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(arr->data + offset, element, arr->element_size);
    atomic_thread_fence(memory_order_release);
    return SCL_OK;
}

scl_error_t scl_carray_pop(scl_concurrent_array_t *arr, void *out)
{
    if (scl_unlikely(!arr || !out)) return SCL_ERR_NULL_PTR;
    size_t old = atomic_load_explicit(&arr->count, memory_order_relaxed);
    while (1) {
        if (old == 0) return SCL_ERR_EMPTY;
        if (atomic_compare_exchange_weak_explicit(&arr->count, &old, old - 1,
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    size_t offset;
    if (scl_mul_overflow(old - 1, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(out, arr->data + offset, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_carray_get(const scl_concurrent_array_t *arr, size_t index, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!arr || !out)) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&arr->count, memory_order_acquire);
    if (scl_unlikely(index >= cnt)) return SCL_ERR_INVALID_INDEX;
    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(out, arr->data + offset, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_carray_set(scl_concurrent_array_t *arr, size_t index, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!arr || !element)) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&arr->count, memory_order_acquire);
    if (scl_unlikely(index >= cnt)) return SCL_ERR_INVALID_INDEX;
    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(arr->data + offset, element, arr->element_size);
    return SCL_OK;
}

size_t scl_carray_count(const scl_concurrent_array_t *arr)
{
    return arr ? atomic_load_explicit(&arr->count, memory_order_relaxed) : 0;
}

bool scl_carray_empty(const scl_concurrent_array_t *arr)
{
    return arr ? atomic_load_explicit(&arr->count, memory_order_relaxed) == 0 : true;
}
