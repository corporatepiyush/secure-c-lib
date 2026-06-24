#include "scl_concurrent_deque.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_cdeque_init(scl_allocator_t *alloc, scl_concurrent_deque_t *deque, size_t element_size, size_t capacity)
{
    if (scl_unlikely(!deque)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0 || capacity == 0)) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    deque->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!deque->data)) return SCL_ERR_OUT_OF_MEMORY;
    deque->element_size = element_size;
    deque->capacity = capacity;
    deque->head = 0;
    atomic_init(&deque->count, 0);
    scl_spinlock_init(&deque->lock);
    return SCL_OK;
}

void scl_cdeque_destroy(scl_allocator_t *alloc, scl_concurrent_deque_t *deque)
{
    if (scl_unlikely(!deque)) return;
    scl_free(alloc, deque->data);
    deque->data = NULL;
    deque->capacity = 0;
    atomic_store_explicit(&deque->count, 0, memory_order_relaxed);
}

scl_error_t scl_cdeque_push_front(scl_allocator_t *alloc, scl_concurrent_deque_t *deque, const void  *SCL_RESTRICT element)
{
    (void)alloc;
    if (scl_unlikely(!deque || !element)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == deque->capacity) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_FULL;
    }
    deque->head = (deque->head == 0) ? deque->capacity - 1 : deque->head - 1;
    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset)) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    scl_memcpy(deque->data + offset, element, deque->element_size);
    atomic_fetch_add_explicit(&deque->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&deque->lock);
    return SCL_OK;
}

scl_error_t scl_cdeque_push_back(scl_allocator_t *alloc, scl_concurrent_deque_t *deque, const void  *SCL_RESTRICT element)
{
    (void)alloc;
    if (scl_unlikely(!deque || !element)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == deque->capacity) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_FULL;
    }
    size_t tail = (deque->head + cnt) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset)) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    scl_memcpy(deque->data + offset, element, deque->element_size);
    atomic_fetch_add_explicit(&deque->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&deque->lock);
    return SCL_OK;
}

scl_error_t scl_cdeque_pop_front(scl_concurrent_deque_t *deque, void *out)
{
    if (scl_unlikely(!deque || !out)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == 0) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_EMPTY;
    }
    size_t offset;
    if (scl_mul_overflow(deque->head, deque->element_size, &offset)) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    scl_memcpy(out, deque->data + offset, deque->element_size);
    deque->head = (deque->head + 1) % deque->capacity;
    atomic_fetch_sub_explicit(&deque->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&deque->lock);
    return SCL_OK;
}

scl_error_t scl_cdeque_pop_back(scl_concurrent_deque_t *deque, void *out)
{
    if (scl_unlikely(!deque || !out)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&deque->lock);
    size_t cnt = atomic_load_explicit(&deque->count, memory_order_relaxed);
    if (cnt == 0) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_EMPTY;
    }
    size_t tail = (deque->head + cnt - 1) % deque->capacity;
    size_t offset;
    if (scl_mul_overflow(tail, deque->element_size, &offset)) {
        scl_spinlock_unlock(&deque->lock);
        return SCL_ERR_SIZE_OVERFLOW;
    }
    scl_memcpy(out, deque->data + offset, deque->element_size);
    atomic_fetch_sub_explicit(&deque->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&deque->lock);
    return SCL_OK;
}

size_t scl_cdeque_count(const scl_concurrent_deque_t *deque)
{
    return deque ? atomic_load_explicit(&deque->count, memory_order_relaxed) : 0;
}

bool scl_cdeque_empty(const scl_concurrent_deque_t *deque)
{
    return deque ? atomic_load_explicit(&deque->count, memory_order_relaxed) == 0 : true;
}
