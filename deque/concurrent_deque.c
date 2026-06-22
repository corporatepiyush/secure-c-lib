#include "concurrent_deque.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire)) {
        __asm__ volatile("yield");
    }
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

scl_error_t scl_concurrent_deque_init(scl_concurrent_deque_t *deque, size_t element_size, size_t capacity)
{
    if (!deque) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || capacity == 0) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    deque->data = malloc(bytes);
    if (!deque->data) return SCL_ERR_OUT_OF_MEMORY;
    deque->element_size = element_size;
    deque->capacity = capacity;
    deque->head = 0;
    atomic_init(&deque->count, 0);
    atomic_flag_clear(&deque->lock);
    return SCL_OK;
}

void scl_concurrent_deque_destroy(scl_concurrent_deque_t *deque)
{
    if (!deque) return;
    free(deque->data);
    deque->data = NULL;
    deque->capacity = 0;
    atomic_store_explicit(&deque->count, 0, memory_order_relaxed);
}

scl_error_t scl_concurrent_deque_push_front(scl_concurrent_deque_t *deque, const void *element)
{
    if (!deque || !element) return SCL_ERR_NULL_PTR;
    spin_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == deque->capacity) {
        spin_unlock(&deque->lock);
        return SCL_ERR_FULL;
    }
    deque->head = (deque->head == 0) ? deque->capacity - 1 : deque->head - 1;
    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset)) {
        spin_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    memcpy(deque->data + offset, element, deque->element_size);
    atomic_fetch_add_explicit(&deque->count, 1, memory_order_relaxed);
    spin_unlock(&deque->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_deque_push_back(scl_concurrent_deque_t *deque, const void *element)
{
    if (!deque || !element) return SCL_ERR_NULL_PTR;
    spin_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == deque->capacity) {
        spin_unlock(&deque->lock);
        return SCL_ERR_FULL;
    }
    size_t tail = (deque->head + cnt) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset)) {
        spin_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    memcpy(deque->data + offset, element, deque->element_size);
    atomic_fetch_add_explicit(&deque->count, 1, memory_order_relaxed);
    spin_unlock(&deque->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_deque_pop_front(scl_concurrent_deque_t *deque, void *out)
{
    if (!deque || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == 0) {
        spin_unlock(&deque->lock);
        return SCL_ERR_EMPTY;
    }
    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset)) {
        spin_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    memcpy(out, deque->data + offset, deque->element_size);
    deque->head = (deque->head + 1) % deque->capacity;
    atomic_fetch_sub_explicit(&deque->count, 1, memory_order_relaxed);
    spin_unlock(&deque->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_deque_pop_back(scl_concurrent_deque_t *deque, void *out)
{
    if (!deque || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == 0) {
        spin_unlock(&deque->lock);
        return SCL_ERR_EMPTY;
    }
    size_t tail = (deque->head + cnt - 1) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset)) {
        spin_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    memcpy(out, deque->data + offset, deque->element_size);
    atomic_fetch_sub_explicit(&deque->count, 1, memory_order_relaxed);
    spin_unlock(&deque->lock);
    return SCL_OK;
}

size_t scl_concurrent_deque_count(const scl_concurrent_deque_t *deque)
{
    return deque ? atomic_load_explicit(&deque->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_deque_empty(const scl_concurrent_deque_t *deque)
{
    return deque ? atomic_load_explicit(&deque->count, memory_order_relaxed) == 0 : true;
}
