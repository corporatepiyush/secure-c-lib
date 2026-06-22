#include "concurrent_ringbuf.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_concurrent_ringbuf_init(scl_concurrent_ringbuf_t *rb, size_t element_size, size_t capacity)
{
    if (!rb) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || capacity == 0) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    rb->data = malloc(bytes);
    if (!rb->data) return SCL_ERR_OUT_OF_MEMORY;
    rb->element_size = element_size;
    rb->capacity = capacity;
    atomic_init(&rb->head, 0);
    atomic_init(&rb->count, 0);
    return SCL_OK;
}

void scl_concurrent_ringbuf_destroy(scl_concurrent_ringbuf_t *rb)
{
    if (!rb) return;
    free(rb->data);
    rb->data = NULL;
    rb->capacity = 0;
    atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
    atomic_store_explicit(&rb->count, 0, memory_order_relaxed);
}

scl_error_t scl_concurrent_ringbuf_push(scl_concurrent_ringbuf_t *rb, const void *element)
{
    if (!rb || !element) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&rb->count, memory_order_relaxed);
    if (cnt == rb->capacity) return SCL_ERR_FULL;
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
    size_t idx = (head + cnt) % rb->capacity;
    size_t offset;
    if (scl_mul_overflow(idx, rb->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(rb->data + offset, element, rb->element_size);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&rb->count, cnt + 1, memory_order_release);
    return SCL_OK;
}

scl_error_t scl_concurrent_ringbuf_pop(scl_concurrent_ringbuf_t *rb, void *out)
{
    if (!rb || !out) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&rb->count, memory_order_acquire);
    if (cnt == 0) return SCL_ERR_EMPTY;
    size_t head = atomic_load_explicit(&rb->head, memory_order_relaxed);
    size_t offset;
    if (scl_mul_overflow(head, rb->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(out, rb->data + offset, rb->element_size);
    atomic_thread_fence(memory_order_release);
    atomic_store_explicit(&rb->head, (head + 1) % rb->capacity, memory_order_release);
    atomic_store_explicit(&rb->count, cnt - 1, memory_order_release);
    return SCL_OK;
}

scl_error_t scl_concurrent_ringbuf_peek(const scl_concurrent_ringbuf_t *rb, size_t index, void *out)
{
    if (!rb || !out) return SCL_ERR_NULL_PTR;
    size_t cnt = atomic_load_explicit(&rb->count, memory_order_acquire);
    if (index >= cnt) return SCL_ERR_INVALID_INDEX;
    size_t head = atomic_load_explicit(&rb->head, memory_order_acquire);
    size_t idx = (head + index) % rb->capacity;
    size_t offset;
    if (scl_mul_overflow(idx, rb->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(out, rb->data + offset, rb->element_size);
    return SCL_OK;
}

size_t scl_concurrent_ringbuf_count(const scl_concurrent_ringbuf_t *rb)
{
    return rb ? atomic_load_explicit(&rb->count, memory_order_acquire) : 0;
}

bool scl_concurrent_ringbuf_empty(const scl_concurrent_ringbuf_t *rb)
{
    return rb ? atomic_load_explicit(&rb->count, memory_order_acquire) == 0 : true;
}
