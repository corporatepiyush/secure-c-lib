#include "scl_ringbuf.h"
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_ringbuf_init(scl_allocator_t *alloc, scl_ringbuf_t *rb, size_t element_size, size_t capacity,
                             bool overwrite)
{
    if (!rb) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || capacity == 0) return SCL_ERR_INVALID_ARG;

    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    rb->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!rb->data) return SCL_ERR_OUT_OF_MEMORY;

    rb->element_size = element_size;
    rb->capacity = capacity;
    rb->head = 0;
    rb->count = 0;
    rb->overwrite = overwrite;
    return SCL_OK;
}

void scl_ringbuf_destroy(scl_allocator_t *alloc, scl_ringbuf_t *rb)
{
    if (rb) {
        scl_free(alloc, rb->data);
        rb->data = NULL;
        rb->capacity = 0;
        rb->count = 0;
    }
}

scl_error_t scl_ringbuf_push(scl_ringbuf_t *rb, const void *element)
{
    if (!rb || !element) return SCL_ERR_NULL_PTR;

    if (rb->count == rb->capacity) {
        if (!rb->overwrite) return SCL_ERR_FULL;
        size_t offset;
        if (scl_mul_overflow(rb->head, rb->element_size, &offset))
            return SCL_ERR_SIZE_OVERFLOW;
        memcpy(rb->data + offset, element, rb->element_size);
        rb->head = (rb->head + 1) % rb->capacity;
        return SCL_OK;
    }

    size_t tail = (rb->head + rb->count) % rb->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, rb->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(rb->data + offset, element, rb->element_size);
    rb->count++;
    return SCL_OK;
}

scl_error_t scl_ringbuf_pop(scl_ringbuf_t *rb, void *out)
{
    if (!rb || !out) return SCL_ERR_NULL_PTR;
    if (rb->count == 0) return SCL_ERR_EMPTY;

    size_t offset;
    if (scl_mul_overflow(rb->head, rb->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(out, rb->data + offset, rb->element_size);
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return SCL_OK;
}

scl_error_t scl_ringbuf_peek(const scl_ringbuf_t *rb, size_t index, void *out)
{
    if (!rb || !out) return SCL_ERR_NULL_PTR;
    if (index >= rb->count) return SCL_ERR_INVALID_INDEX;

    size_t pos = (rb->head + index) % rb->capacity;
    size_t offset;
    if (scl_mul_overflow(pos, rb->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;
    memcpy(out, rb->data + offset, rb->element_size);
    return SCL_OK;
}

size_t scl_ringbuf_count(const scl_ringbuf_t *rb) { return rb ? rb->count : 0; }
size_t scl_ringbuf_capacity(const scl_ringbuf_t *rb) { return rb ? rb->capacity : 0; }
bool scl_ringbuf_empty(const scl_ringbuf_t *rb) { return rb ? rb->count == 0 : true; }
bool scl_ringbuf_full(const scl_ringbuf_t *rb) { return rb ? rb->count == rb->capacity : false; }

scl_error_t scl_ringbuf_search(const scl_ringbuf_t *restrict rb, const void *restrict key,
                               int (*cmp)(const void *, const void *),
                               size_t *restrict out_index)
{
    if (__builtin_expect(!rb || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < rb->count; i++) {
        size_t pos = (rb->head + i) % rb->capacity;
        if (cmp(rb->data + pos * rb->element_size, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
