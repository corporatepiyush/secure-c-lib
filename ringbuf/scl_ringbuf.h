#ifndef SCL_RINGBUF_H
#define SCL_RINGBUF_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t head;
    size_t count;
    bool overwrite;
} scl_ringbuf_t;

scl_error_t scl_ringbuf_init(scl_allocator_t *alloc, scl_ringbuf_t *rb, size_t element_size, size_t capacity,
                             bool overwrite) SCL_WARN_UNUSED;
void        scl_ringbuf_destroy(scl_allocator_t *alloc, scl_ringbuf_t *rb);
scl_error_t scl_ringbuf_push(scl_ringbuf_t *rb, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_ringbuf_pop(scl_ringbuf_t *rb, void *out) SCL_WARN_UNUSED;
scl_error_t scl_ringbuf_peek(const scl_ringbuf_t *rb, size_t index, void *out) SCL_WARN_UNUSED;
size_t      scl_ringbuf_count(const scl_ringbuf_t *rb);
size_t      scl_ringbuf_capacity(const scl_ringbuf_t *rb);
bool        scl_ringbuf_empty(const scl_ringbuf_t *rb);
bool        scl_ringbuf_full(const scl_ringbuf_t *rb);

scl_error_t scl_ringbuf_search(const scl_ringbuf_t *rb, const void *key,
                               int (*cmp)(const void *, const void *),
                               size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
