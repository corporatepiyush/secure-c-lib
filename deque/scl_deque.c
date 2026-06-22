#include "scl_deque.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_deque_init(scl_deque_t *deque, size_t element_size, size_t initial_capacity)
{
    if (!deque) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    deque->data = NULL;
    deque->element_size = element_size;
    deque->capacity = 0;
    deque->head = 0;
    deque->count = 0;

    if (initial_capacity > 0) {
        size_t bytes;
        if (scl_mul_overflow(initial_capacity, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        deque->data = malloc(bytes);
        if (!deque->data) return SCL_ERR_OUT_OF_MEMORY;
        deque->capacity = initial_capacity;
    }
    return SCL_OK;
}

void scl_deque_destroy(scl_deque_t *deque)
{
    if (deque) {
        free(deque->data);
        deque->data = NULL;
        deque->capacity = 0;
        deque->head = 0;
        deque->count = 0;
    }
}

static scl_error_t scl_deque_grow(scl_deque_t *deque)
{
    size_t new_cap = deque->capacity == 0 ? 4 : deque->capacity * 2;
    size_t bytes;
    if (scl_mul_overflow(new_cap, deque->element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = malloc(bytes);
    if (!tmp) return SCL_ERR_OUT_OF_MEMORY;

    for (size_t i = 0; i < deque->count; i++) {
        size_t src_idx = (deque->head + i) % deque->capacity;
        memcpy(tmp + i * deque->element_size,
               deque->data + src_idx * deque->element_size,
               deque->element_size);
    }

    free(deque->data);
    deque->data = tmp;
    deque->head = 0;
    deque->capacity = new_cap;
    return SCL_OK;
}

scl_error_t scl_deque_push_front(scl_deque_t *deque, const void *element)
{
    if (!deque || !element) return SCL_ERR_NULL_PTR;

    if (deque->count == deque->capacity) {
        scl_error_t err = scl_deque_grow(deque);
        if (err != SCL_OK) return err;
    }

    deque->head = (deque->head == 0) ? deque->capacity - 1 : deque->head - 1;
    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(deque->data + offset, element, deque->element_size);
    deque->count++;
    return SCL_OK;
}

scl_error_t scl_deque_push_back(scl_deque_t *deque, const void *element)
{
    if (!deque || !element) return SCL_ERR_NULL_PTR;

    if (deque->count == deque->capacity) {
        scl_error_t err = scl_deque_grow(deque);
        if (err != SCL_OK) return err;
    }

    size_t tail = (deque->head + deque->count) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(deque->data + offset, element, deque->element_size);
    deque->count++;
    return SCL_OK;
}

scl_error_t scl_deque_pop_front(scl_deque_t *deque, void *out)
{
    if (!deque || !out) return SCL_ERR_NULL_PTR;
    if (deque->count == 0) return SCL_ERR_EMPTY;

    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, deque->data + offset, deque->element_size);
    deque->head = (deque->head + 1) % deque->capacity;
    deque->count--;
    return SCL_OK;
}

scl_error_t scl_deque_pop_back(scl_deque_t *deque, void *out)
{
    if (!deque || !out) return SCL_ERR_NULL_PTR;
    if (deque->count == 0) return SCL_ERR_EMPTY;

    size_t tail = (deque->head + deque->count - 1) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, deque->data + offset, deque->element_size);
    deque->count--;
    return SCL_OK;
}

scl_error_t scl_deque_peek_front(const scl_deque_t *deque, void *out)
{
    if (!deque || !out) return SCL_ERR_NULL_PTR;
    if (deque->count == 0) return SCL_ERR_EMPTY;

    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, deque->data + offset, deque->element_size);
    return SCL_OK;
}

scl_error_t scl_deque_peek_back(const scl_deque_t *deque, void *out)
{
    if (!deque || !out) return SCL_ERR_NULL_PTR;
    if (deque->count == 0) return SCL_ERR_EMPTY;

    size_t tail = (deque->head + deque->count - 1) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, deque->data + offset, deque->element_size);
    return SCL_OK;
}

size_t scl_deque_count(const scl_deque_t *deque)
{
    return deque ? deque->count : 0;
}

bool scl_deque_empty(const scl_deque_t *deque)
{
    return deque ? deque->count == 0 : true;
}

scl_error_t scl_deque_search(const scl_deque_t *restrict deque, const void *restrict key,
                             int (*cmp)(const void *, const void *),
                             size_t *restrict out_index)
{
    if (__builtin_expect(!deque || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < deque->count; i++) {
        size_t pos = (deque->head + i) % deque->capacity;
        if (cmp(deque->data + pos * deque->element_size, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
