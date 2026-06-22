#include "concurrent_heap.h"
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

scl_error_t scl_concurrent_heap_init(scl_concurrent_heap_t *heap, size_t element_size,
                                     size_t capacity, scl_concurrent_cmp_func_t cmp)
{
    if (!heap) return SCL_ERR_NULL_PTR;
    if (element_size == 0 || capacity == 0 || !cmp) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    heap->data = malloc(bytes);
    if (!heap->data) return SCL_ERR_OUT_OF_MEMORY;
    heap->element_size = element_size;
    heap->capacity = capacity;
    atomic_init(&heap->count, 0);
    heap->cmp = cmp;
    atomic_flag_clear(&heap->lock);
    return SCL_OK;
}

void scl_concurrent_heap_destroy(scl_concurrent_heap_t *heap)
{
    if (!heap) return;
    free(heap->data);
    heap->data = NULL;
    heap->capacity = 0;
    atomic_store_explicit(&heap->count, 0, memory_order_relaxed);
}

static void sift_up(unsigned char *data, size_t element_size, scl_concurrent_cmp_func_t cmp, size_t idx)
{
    unsigned char *tmp = malloc(element_size);
    if (!tmp) return;
    memcpy(tmp, data + idx * element_size, element_size);
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (cmp(tmp, data + parent * element_size) >= 0) break;
        memcpy(data + idx * element_size, data + parent * element_size, element_size);
        idx = parent;
    }
    memcpy(data + idx * element_size, tmp, element_size);
    free(tmp);
}

static void sift_down(unsigned char *data, size_t element_size, scl_concurrent_cmp_func_t cmp,
                      size_t count, size_t idx)
{
    unsigned char *tmp = malloc(element_size);
    if (!tmp) return;
    memcpy(tmp, data + idx * element_size, element_size);
    while (1) {
        size_t smallest = idx;
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        if (left < count && cmp(data + left * element_size, data + smallest * element_size) < 0)
            smallest = left;
        if (right < count && cmp(data + right * element_size, data + smallest * element_size) < 0)
            smallest = right;
        if (smallest == idx) break;
        memcpy(data + idx * element_size, data + smallest * element_size, element_size);
        idx = smallest;
    }
    memcpy(data + idx * element_size, tmp, element_size);
    free(tmp);
}

scl_error_t scl_concurrent_heap_push(scl_concurrent_heap_t *heap, const void *element)
{
    if (!heap || !element) return SCL_ERR_NULL_PTR;
    spin_lock(&heap->lock);
    size_t cnt = atomic_load_explicit(&heap->count, memory_order_relaxed);
    if (cnt == heap->capacity) {
        spin_unlock(&heap->lock);
        return SCL_ERR_FULL;
    }
    memcpy(heap->data + cnt * heap->element_size, element, heap->element_size);
    sift_up(heap->data, heap->element_size, heap->cmp, cnt);
    atomic_fetch_add_explicit(&heap->count, 1, memory_order_relaxed);
    spin_unlock(&heap->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_heap_pop(scl_concurrent_heap_t *heap, void *out)
{
    if (!heap || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&heap->lock);
    size_t cnt = atomic_load_explicit(&heap->count, memory_order_relaxed);
    if (cnt == 0) {
        spin_unlock(&heap->lock);
        return SCL_ERR_EMPTY;
    }
    memcpy(out, heap->data, heap->element_size);
    cnt--;
    if (cnt > 0) {
        memcpy(heap->data, heap->data + cnt * heap->element_size, heap->element_size);
        sift_down(heap->data, heap->element_size, heap->cmp, cnt, 0);
    }
    atomic_store_explicit(&heap->count, cnt, memory_order_relaxed);
    spin_unlock(&heap->lock);
    return SCL_OK;
}

scl_error_t scl_concurrent_heap_peek(scl_concurrent_heap_t *heap, void *out)
{
    if (!heap || !out) return SCL_ERR_NULL_PTR;
    spin_lock(&heap->lock);
    size_t cnt = atomic_load_explicit(&heap->count, memory_order_relaxed);
    if (cnt == 0) {
        spin_unlock(&heap->lock);
        return SCL_ERR_EMPTY;
    }
    memcpy(out, heap->data, heap->element_size);
    spin_unlock(&heap->lock);
    return SCL_OK;
}

size_t scl_concurrent_heap_count(const scl_concurrent_heap_t *heap)
{
    return heap ? atomic_load_explicit(&heap->count, memory_order_relaxed) : 0;
}

bool scl_concurrent_heap_empty(const scl_concurrent_heap_t *heap)
{
    return heap ? atomic_load_explicit(&heap->count, memory_order_relaxed) == 0 : true;
}
