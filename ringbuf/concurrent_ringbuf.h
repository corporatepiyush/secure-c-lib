#ifndef SCL_CONCURRENT_RINGBUF_H
#define SCL_CONCURRENT_RINGBUF_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    atomic_size_t head;
    atomic_size_t count;
} scl_atomic_ringbuf_t;

scl_error_t scl_atomic_ringbuf_init(scl_allocator_t *alloc, scl_atomic_ringbuf_t *rb, size_t element_size,
                             size_t capacity) SCL_WARN_UNUSED;
void        scl_atomic_ringbuf_destroy(scl_allocator_t *alloc, scl_atomic_ringbuf_t *rb);
scl_error_t scl_atomic_ringbuf_push(scl_atomic_ringbuf_t *rb, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_atomic_ringbuf_pop(scl_atomic_ringbuf_t *rb, void *out) SCL_WARN_UNUSED;
scl_error_t scl_atomic_ringbuf_peek(const scl_atomic_ringbuf_t *rb, size_t index, void *out) SCL_WARN_UNUSED;
size_t      scl_atomic_ringbuf_count(const scl_atomic_ringbuf_t *rb);
bool        scl_atomic_ringbuf_empty(const scl_atomic_ringbuf_t *rb);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
