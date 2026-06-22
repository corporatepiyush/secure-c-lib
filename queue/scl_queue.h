#ifndef SCL_QUEUE_H
#define SCL_QUEUE_H

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
    size_t tail;
    size_t count;
} scl_queue_t;

scl_error_t scl_queue_init(scl_queue_t *queue, size_t element_size, size_t initial_capacity) SCL_WARN_UNUSED;
void        scl_queue_destroy(scl_queue_t *queue);
scl_error_t scl_queue_enqueue(scl_queue_t *queue, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_queue_dequeue(scl_queue_t *queue, void *out) SCL_WARN_UNUSED;
scl_error_t scl_queue_peek(const scl_queue_t *queue, void *out) SCL_WARN_UNUSED;
size_t      scl_queue_count(const scl_queue_t *queue);
bool        scl_queue_empty(const scl_queue_t *queue);

scl_error_t scl_queue_search(const scl_queue_t *queue, const void *key,
                             int (*cmp)(const void *, const void *),
                             size_t *out_index) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
