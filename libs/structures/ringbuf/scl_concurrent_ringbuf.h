#ifndef SCL_CONCURRENT_RINGBUF_H
#define SCL_CONCURRENT_RINGBUF_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    /* Producer writes count, consumer reads head — separate cache lines */
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t mask;
    char _pad0[SCL_CACHE_LINE_SIZE - 4 * sizeof(size_t)];
    atomic_size_t head SCL_CACHE_ALIGNED;
    atomic_size_t count SCL_CACHE_ALIGNED;
    scl_spinlock_t lock;
} scl_concurrent_ringbuf_t;

scl_error_t scl_cringbuf_init(scl_allocator_t *alloc, scl_concurrent_ringbuf_t *rb, size_t element_size,
                             size_t capacity) SCL_WARN_UNUSED;
void        scl_cringbuf_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_ringbuf_t *SCL_RESTRICT rb);
scl_error_t scl_cringbuf_push(scl_concurrent_ringbuf_t *SCL_RESTRICT rb, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cringbuf_pop(scl_concurrent_ringbuf_t *SCL_RESTRICT rb, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
scl_error_t scl_cringbuf_peek(const scl_concurrent_ringbuf_t *SCL_RESTRICT rb, size_t index, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_cringbuf_count(const scl_concurrent_ringbuf_t *SCL_RESTRICT rb);
SCL_PURE bool        scl_cringbuf_empty(const scl_concurrent_ringbuf_t *SCL_RESTRICT rb);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
