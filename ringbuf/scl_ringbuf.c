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

    /* Round capacity to next power of 2 for fast bitmask modulo */
    size_t cap = scl_bit_ceil_sz(capacity);

    size_t bytes;
    if (scl_mul_overflow(cap, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    rb->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (!rb->data) return SCL_ERR_OUT_OF_MEMORY;

    rb->element_size = element_size;
    rb->capacity = cap;
    rb->mask = cap - 1;
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

    size_t cnt = rb->count;
    size_t cap = rb->capacity;
    size_t es = rb->element_size;

    if (scl_unlikely(cnt == cap)) {
        if (!rb->overwrite) return SCL_ERR_FULL;
        memcpy(rb->data + rb->head * es, element, es);
        rb->head = (rb->head + 1) & rb->mask;
        return SCL_OK;
    }

    size_t tail = (rb->head + cnt) & rb->mask;
    memcpy(rb->data + tail * es, element, es);
    rb->count = cnt + 1;
    return SCL_OK;
}

scl_error_t scl_ringbuf_pop(scl_ringbuf_t *rb, void *out)
{
    if (!rb || !out) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(rb->count == 0)) return SCL_ERR_EMPTY;

    size_t es = rb->element_size;
    memcpy(out, rb->data + rb->head * es, es);
    rb->head = (rb->head + 1) & rb->mask;
    rb->count--;
    return SCL_OK;
}

scl_error_t scl_ringbuf_peek(const scl_ringbuf_t *rb, size_t index, void *out)
{
    if (!rb || !out) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(index >= rb->count)) return SCL_ERR_INVALID_INDEX;

    size_t pos = (rb->head + index) & rb->mask;
    memcpy(out, rb->data + pos * rb->element_size, rb->element_size);
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
    if (scl_unlikely(!rb || !key || !cmp || !out_index))
        return SCL_ERR_NULL_PTR;

    size_t cnt = rb->count;
    size_t head = rb->head;
    size_t mask = rb->mask;
    size_t es = rb->element_size;
    unsigned char *data = rb->data;

    for (size_t i = 0; i < cnt; i++) {
        size_t pos = (head + i) & mask;
        if (cmp(data + pos * es, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
