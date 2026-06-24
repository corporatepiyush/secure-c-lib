#include "scl_queue.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_queue_init(scl_allocator_t *alloc, scl_queue_t *queue, size_t element_size, size_t initial_capacity)
{
    if (scl_unlikely(!queue)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;

    queue->data = NULL;
    queue->element_size = element_size;
    queue->capacity = 0;
    queue->mask = 0;
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;

    if (initial_capacity > 0) {
        size_t cap = scl_bit_ceil_sz(initial_capacity);
        size_t bytes;
        if (scl_mul_overflow(cap, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        queue->data = scl_alloc(alloc, bytes, alignof(max_align_t));
        if (scl_unlikely(!queue->data)) return SCL_ERR_OUT_OF_MEMORY;
        queue->capacity = cap;
        queue->mask = cap - 1;
    }
    return SCL_OK;
}

void scl_queue_destroy(scl_allocator_t *alloc, scl_queue_t *queue)
{
    if (queue) {
        scl_free(alloc, queue->data);
        queue->data = NULL;
        queue->capacity = 0;
        queue->head = 0;
        queue->tail = 0;
        queue->count = 0;
    }
}

scl_error_t scl_queue_enqueue(scl_allocator_t *alloc, scl_queue_t *queue, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!queue || !element)) return SCL_ERR_NULL_PTR;

    size_t cnt = queue->count;
    size_t cap = queue->capacity;
    size_t es = queue->element_size;

    if (scl_unlikely(cnt == cap)) {
        /* Grow — round to next power of 2 */
        size_t new_cap = cap == 0 ? 4 : cap * 2;
        size_t new_bytes;
        if (scl_mul_overflow(new_cap, es, &new_bytes))
            return SCL_ERR_SIZE_OVERFLOW;

        unsigned char *tmp = scl_alloc(alloc, new_bytes, alignof(max_align_t));
        if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;

        size_t head = queue->head;
        size_t mask = queue->mask;
        for (size_t i = 0; i < cnt; i++) {
            size_t src_idx = (head + i) & mask;
            scl_memcpy(tmp + i * es, queue->data + src_idx * es, es);
        }

        scl_free(alloc, queue->data);
        queue->data = tmp;
        queue->head = 0;
        queue->tail = cnt;
        queue->capacity = new_cap;
        queue->mask = new_cap - 1;
        cap = new_cap;
    }

    scl_memcpy(queue->data + queue->tail * es, element, es);
    queue->tail = (queue->tail + 1) & queue->mask;
    queue->count = cnt + 1;
    return SCL_OK;
}

scl_error_t scl_queue_dequeue(scl_queue_t *queue, void *out)
{
    if (scl_unlikely(!queue || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(queue->count == 0)) return SCL_ERR_EMPTY;

    size_t es = queue->element_size;
    scl_memcpy(out, queue->data + queue->head * es, es);
    queue->head = (queue->head + 1) & queue->mask;
    queue->count--;
    return SCL_OK;
}

scl_error_t scl_queue_peek(const scl_queue_t *queue, void *out)
{
    if (scl_unlikely(!queue || !out)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(queue->count == 0)) return SCL_ERR_EMPTY;

    scl_memcpy(out, queue->data + queue->head * queue->element_size, queue->element_size);
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
    if (scl_unlikely(!queue || !key || !cmp || !out_index))
        return SCL_ERR_NULL_PTR;

    size_t cnt = queue->count;
    size_t head = queue->head;
    size_t mask = queue->mask;
    size_t es = queue->element_size;
    unsigned char *data = queue->data;

    for (size_t i = 0; i < cnt; i++) {
        size_t pos = (head + i) & mask;
        if (cmp(data + pos * es, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
