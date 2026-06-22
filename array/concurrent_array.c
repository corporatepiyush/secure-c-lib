#include "concurrent_array.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_atomic_array_init(scl_allocator_t *alloc, scl_atomic_array_t *arr, size_t element_size, size_t capacity)
{
    if (!arr) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || capacity == 0) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    arr->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!arr->data) return SCL_ERR_OUT_OF_MEMORY;
    arr->element_size = element_size;
    arr->capacity = capacity;
    atomic_init(&arr->count, 0);
    return SCL_OK;
}

void scl_atomic_array_destroy(scl_allocator_t *alloc, scl_atomic_array_t *arr)
{
    if (arr) {
        scl_free(alloc, arr->data);
        arr->data = NULL;
        arr->capacity = 0;
        atomic_store_explicit(&arr->count, 0, memory_order_relaxed);
    }
}

scl_error_t scl_atomic_array_push(scl_allocator_t *alloc, scl_atomic_array_t *arr, const void *element)
{
    (void)alloc;
    if (!arr || !element) return SCL_ERR_NULL_PTR;
    size_t old = atomic_load_explicit(&arr->count, memory_order_relaxed);
    while (1) {
        if (old >= arr->capacity) return SCL_ERR_FULL;
        if (atomic_compare_exchange_weak_explicit(&arr->count, &old, old + 1,
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    size_t offset;
    if (scl_mul_overflow(old, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(arr->data + offset, element, arr->element_size);
    atomic_thread_fence(memory_order_release);
    return SCL_OK;
}

scl_error_t scl_atomic_array_pop(scl_atomic_array_t *arr, void *out)
{
    if (!arr || !out) return SCL_ERR_NULL_PTR;
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
    memcpy(out, arr->data + offset, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_atomic_array_get(const scl_atomic_array_t *arr, size_t index, void *out)
{
    if (!arr || !out) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&arr->count, memory_order_acquire);
    if (index >= cnt) return SCL_ERR_INVALID_INDEX;
    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(out, arr->data + offset, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_atomic_array_set(scl_atomic_array_t *arr, size_t index, const void *element)
{
    if (!arr || !element) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&arr->count, memory_order_acquire);
    if (index >= cnt) return SCL_ERR_INVALID_INDEX;
    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(arr->data + offset, element, arr->element_size);
    return SCL_OK;
}

size_t scl_atomic_array_count(const scl_atomic_array_t *arr)
{
    return arr ? atomic_load_explicit(&arr->count, memory_order_relaxed) : 0;
}

bool scl_atomic_array_empty(const scl_atomic_array_t *arr)
{
    return arr ? atomic_load_explicit(&arr->count, memory_order_relaxed) == 0 : true;
}
