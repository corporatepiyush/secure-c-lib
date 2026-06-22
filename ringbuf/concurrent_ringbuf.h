#ifndef SCL_CONCURRENT_RINGBUF_H
#define SCL_CONCURRENT_RINGBUF_H

#include "../common/scl_common.h"
#include <stdatomic.h>

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
} scl_concurrent_ringbuf_t;

scl_error_t scl_concurrent_ringbuf_init(scl_concurrent_ringbuf_t *rb, size_t element_size,
                                        size_t capacity) SCL_WARN_UNUSED;
void        scl_concurrent_ringbuf_destroy(scl_concurrent_ringbuf_t *rb);
scl_error_t scl_concurrent_ringbuf_push(scl_concurrent_ringbuf_t *rb, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_ringbuf_pop(scl_concurrent_ringbuf_t *rb, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_ringbuf_peek(const scl_concurrent_ringbuf_t *rb, size_t index, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_ringbuf_count(const scl_concurrent_ringbuf_t *rb);
bool        scl_concurrent_ringbuf_empty(const scl_concurrent_ringbuf_t *rb);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
