#ifndef SCL_CONCURRENT_QUEUE_H
#define SCL_CONCURRENT_QUEUE_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_atomic_queue_node {
    void *data;
    atomic_uintptr_t next;
} scl_atomic_queue_node_t;

typedef struct {
    atomic_uintptr_t head;
    atomic_uintptr_t tail;
    size_t element_size;
    atomic_size_t count;
} scl_atomic_queue_t;

scl_error_t scl_atomic_queue_init(scl_allocator_t *alloc, scl_atomic_queue_t *queue, size_t element_size) SCL_WARN_UNUSED;
void        scl_atomic_queue_destroy(scl_allocator_t *alloc, scl_atomic_queue_t *queue);
scl_error_t scl_atomic_queue_enqueue(scl_allocator_t *alloc, scl_atomic_queue_t *queue, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_atomic_queue_dequeue(scl_allocator_t *alloc, scl_atomic_queue_t *queue, void *out) SCL_WARN_UNUSED;
size_t      scl_atomic_queue_count(const scl_atomic_queue_t *queue);
bool        scl_atomic_queue_empty(const scl_atomic_queue_t *queue);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
