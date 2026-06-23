#ifndef SCL_CONCURRENT_QUEUE_H
#define SCL_CONCURRENT_QUEUE_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_queue_node {
    void *data;
    atomic_uintptr_t next;
} scl_concurrent_queue_node_t;

typedef struct {
    atomic_uintptr_t head;
    atomic_uintptr_t tail;
    size_t element_size;
    atomic_size_t count;
} scl_concurrent_queue_t;

scl_error_t scl_cqueue_init(scl_allocator_t *alloc, scl_concurrent_queue_t *queue, size_t element_size) SCL_WARN_UNUSED;
void        scl_cqueue_destroy(scl_allocator_t *alloc, scl_concurrent_queue_t *queue);
scl_error_t scl_cqueue_enqueue(scl_allocator_t *alloc, scl_concurrent_queue_t *queue, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_cqueue_dequeue(scl_allocator_t *alloc, scl_concurrent_queue_t *queue, void *out) SCL_WARN_UNUSED;
size_t      scl_cqueue_count(const scl_concurrent_queue_t *queue);
bool        scl_cqueue_empty(const scl_concurrent_queue_t *queue);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
