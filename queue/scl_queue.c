#include "scl_queue.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_queue_init(scl_queue_t *queue, size_t element_size, size_t initial_capacity)
{
    if (!queue) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    queue->data = NULL;
    queue->element_size = element_size;
    queue->capacity = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    if (initial_capacity > 0) {
        size_t bytes;
        if (scl_mul_overflow(initial_capacity, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        queue->data = malloc(bytes);
        if (!queue->data) return SCL_ERR_OUT_OF_MEMORY;
        queue->capacity = initial_capacity;
    }
    return SCL_OK;
}

void scl_queue_destroy(scl_queue_t *queue)
{
    if (queue) {
        free(queue->data);
        queue->data = NULL;
        queue->capacity = 0;
        queue->head = 0;
        queue->tail = 0;
        queue->count = 0;
    }
}

scl_error_t scl_queue_enqueue(scl_queue_t *queue, const void *element)
{
    if (!queue || !element) return SCL_ERR_NULL_PTR;

    if (queue->count == queue->capacity) {
        size_t new_cap = queue->capacity == 0 ? 4 : queue->capacity * 2;
        size_t bytes;
        if (scl_mul_overflow(new_cap, queue->element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;

        unsigned char *tmp = malloc(bytes);
        if (!tmp) return SCL_ERR_OUT_OF_MEMORY;

        for (size_t i = 0; i < queue->count; i++) {
            size_t src_idx = (queue->head + i) % queue->capacity;
            memcpy(tmp + i * queue->element_size,
                   queue->data + src_idx * queue->element_size,
                   queue->element_size);
        }

        free(queue->data);
        queue->data = tmp;
        queue->head = 0;
        queue->tail = queue->count;
        queue->capacity = new_cap;
    }

    size_t offset;
    if (scl_mul_overflow(queue->tail, queue->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(queue->data + offset, element, queue->element_size);
    queue->tail = (queue->tail + 1) % queue->capacity;
    queue->count++;
    return SCL_OK;
}

scl_error_t scl_queue_dequeue(scl_queue_t *queue, void *out)
{
    if (!queue || !out) return SCL_ERR_NULL_PTR;
    if (queue->count == 0) return SCL_ERR_EMPTY;

    size_t offset;
    if (scl_mul_overflow(queue->head, queue->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, queue->data + offset, queue->element_size);
    queue->head = (queue->head + 1) % queue->capacity;
    queue->count--;
    return SCL_OK;
}

scl_error_t scl_queue_peek(const scl_queue_t *queue, void *out)
{
    if (!queue || !out) return SCL_ERR_NULL_PTR;
    if (queue->count == 0) return SCL_ERR_EMPTY;

    size_t offset;
    if (scl_mul_overflow(queue->head, queue->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, queue->data + offset, queue->element_size);
    return SCL_OK;
}

size_t scl_queue_count(const scl_queue_t *queue)
{
    return queue ? queue->count : 0;
}

bool scl_queue_empty(const scl_queue_t *queue)
{
    return queue ? queue->count == 0 : true;
}

scl_error_t scl_queue_search(const scl_queue_t *restrict queue, const void *restrict key,
                             int (*cmp)(const void *, const void *),
                             size_t *restrict out_index)
{
    if (__builtin_expect(!queue || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < queue->count; i++) {
        size_t pos = (queue->head + i) % queue->capacity;
        if (cmp(queue->data + pos * queue->element_size, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
