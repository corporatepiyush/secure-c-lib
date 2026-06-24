#include "scl_concurrent_heap.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

static scl_error_t sift_up(scl_allocator_t *alloc, unsigned char *data, size_t element_size, scl_cmp_func_t cmp, size_t idx)
{
    unsigned char *tmp = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;
    scl_memcpy(tmp, data + idx * element_size, element_size);
    while (idx > 0) {
        size_t parent = (idx - 1) / 2;
        if (cmp(tmp, data + parent * element_size) >= 0) break;
        scl_memcpy(data + idx * element_size, data + parent * element_size, element_size);
        idx = parent;
    }
    scl_memcpy(data + idx * element_size, tmp, element_size);
    scl_free(alloc, tmp);
    return SCL_OK;
}

static scl_error_t sift_down(scl_allocator_t *alloc, unsigned char *data, size_t element_size, scl_cmp_func_t cmp,
                      size_t count, size_t idx)
{
    unsigned char *tmp = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (scl_unlikely(!tmp)) return SCL_ERR_OUT_OF_MEMORY;
    scl_memcpy(tmp, data + idx * element_size, element_size);
    while (1) {
        size_t smallest = idx;
        size_t left = 2 * idx + 1;
        size_t right = 2 * idx + 2;
        if (left < count && cmp(data + left * element_size, data + smallest * element_size) < 0)
            smallest = left;
        if (right < count && cmp(data + right * element_size, data + smallest * element_size) < 0)
            smallest = right;
        if (smallest == idx) break;
        scl_memcpy(data + idx * element_size, data + smallest * element_size, element_size);
        idx = smallest;
    }
    scl_memcpy(data + idx * element_size, tmp, element_size);
    scl_free(alloc, tmp);
    return SCL_OK;
}

scl_error_t scl_cheap_init(scl_allocator_t *alloc, scl_concurrent_heap_t *heap, size_t element_size,
                          size_t capacity, scl_cmp_func_t cmp)
{
    if (scl_unlikely(!heap)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0 || capacity == 0 || !cmp)) return SCL_ERR_INVALID_ARG;
    size_t bytes;
    if (scl_mul_overflow(capacity, element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    heap->data = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (scl_unlikely(!heap->data)) return SCL_ERR_OUT_OF_MEMORY;
    heap->element_size = element_size;
    heap->capacity = capacity;
    atomic_init(&heap->count, 0);
    heap->cmp = cmp;
    scl_spinlock_init(&heap->lock);
    return SCL_OK;
}

void scl_cheap_destroy(scl_allocator_t *alloc, scl_concurrent_heap_t *heap)
{
    if (scl_unlikely(!heap)) return;
    scl_free(alloc, heap->data);
    heap->data = NULL;
    heap->capacity = 0;
    atomic_store_explicit(&heap->count, 0, memory_order_relaxed);
}

scl_error_t scl_cheap_push(scl_allocator_t *alloc, scl_concurrent_heap_t *heap, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!heap || !element)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&heap->lock);
    size_t cnt = atomic_load_explicit(&heap->count, memory_order_relaxed);
    if (cnt == heap->capacity) {
        scl_spinlock_unlock(&heap->lock);
        return SCL_ERR_FULL;
    }
    scl_memcpy(heap->data + cnt * heap->element_size, element, heap->element_size);
    scl_error_t err = sift_up(alloc, heap->data, heap->element_size, heap->cmp, cnt);
    if (err != SCL_OK) { scl_spinlock_unlock(&heap->lock); return err; }
    atomic_fetch_add_explicit(&heap->count, 1, memory_order_relaxed);
    scl_spinlock_unlock(&heap->lock);
    return SCL_OK;
}

scl_error_t scl_cheap_pop(scl_allocator_t *alloc, scl_concurrent_heap_t *heap, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!heap || !out)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&heap->lock);
    size_t cnt = atomic_load_explicit(&heap->count, memory_order_relaxed);
    if (cnt == 0) {
        scl_spinlock_unlock(&heap->lock);
        return SCL_ERR_EMPTY;
    }
    scl_memcpy(out, heap->data, heap->element_size);
    cnt--;
    if (cnt > 0) {
        scl_memcpy(heap->data, heap->data + cnt * heap->element_size, heap->element_size);
        scl_error_t err = sift_down(alloc, heap->data, heap->element_size, heap->cmp, cnt, 0);
        if (err != SCL_OK) { scl_spinlock_unlock(&heap->lock); return err; }
    }
    atomic_store_explicit(&heap->count, cnt, memory_order_relaxed);
    scl_spinlock_unlock(&heap->lock);
    return SCL_OK;
}

scl_error_t scl_cheap_peek(scl_concurrent_heap_t *heap, void *out)
{
    if (scl_unlikely(!heap || !out)) return SCL_ERR_NULL_PTR;
    scl_spinlock_lock(&heap->lock);
    size_t cnt = atomic_load_explicit(&heap->count, memory_order_relaxed);
    if (cnt == 0) {
        scl_spinlock_unlock(&heap->lock);
        return SCL_ERR_EMPTY;
    }
    scl_memcpy(out, heap->data, heap->element_size);
    scl_spinlock_unlock(&heap->lock);
    return SCL_OK;
}

size_t scl_cheap_count(const scl_concurrent_heap_t *heap)
{
    return heap ? atomic_load_explicit(&heap->count, memory_order_relaxed) : 0;
}

bool scl_cheap_empty(const scl_concurrent_heap_t *heap)
{
    return heap ? atomic_load_explicit(&heap->count, memory_order_relaxed) == 0 : true;
}
