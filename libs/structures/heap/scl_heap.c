#include "scl_heap.h"
#include "scl_string.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_heap_init(scl_allocator_t *alloc, scl_heap_t *heap, size_t element_size, size_t initial_capacity,
                          scl_cmp_func_t cmp)
{
    if (!heap || !cmp) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    heap->data = NULL;
    heap->element_size = element_size;
    heap->capacity = 0;
    heap->count = 0;
    heap->cmp = cmp;

    if (initial_capacity > 0) {
        size_t bytes;
        if (scl_mul_overflow(initial_capacity, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        heap->data = scl_alloc(alloc, bytes, alignof(max_align_t));
        if (!heap->data) return SCL_ERR_OUT_OF_MEMORY;
        heap->capacity = initial_capacity;
    }
    return SCL_OK;
}

void scl_heap_destroy(scl_allocator_t *alloc, scl_heap_t *heap)
{
    if (heap) {
        scl_free(alloc, heap->data);
        heap->data = NULL;
        heap->capacity = 0;
        heap->count = 0;
    }
}

static inline size_t scl_heap_parent(size_t i) { return (i - 1) / 2; }
static inline size_t scl_heap_left(size_t i) { return 2 * i + 1; }
static inline size_t scl_heap_right(size_t i) { return 2 * i + 2; }

static void scl_heap_swap(scl_heap_t *heap, size_t i, size_t j)
{
    unsigned char *SCL_RESTRICT a = heap->data + i * heap->element_size;
    unsigned char *SCL_RESTRICT b = heap->data + j * heap->element_size;
    size_t n = heap->element_size;
    while (n >= sizeof(uint64_t)) {
        uint64_t t;
        scl_memcpy(&t, a, sizeof(uint64_t));
        scl_memcpy(a, b, sizeof(uint64_t));
        scl_memcpy(b, &t, sizeof(uint64_t));
        a += sizeof(uint64_t); b += sizeof(uint64_t); n -= sizeof(uint64_t);
    }
    if (n & 4) {
        uint32_t t;
        scl_memcpy(&t, a, 4); scl_memcpy(a, b, 4); scl_memcpy(b, &t, 4);
        a += 4; b += 4;
    }
    if (n & 2) {
        uint16_t t;
        scl_memcpy(&t, a, 2); scl_memcpy(a, b, 2); scl_memcpy(b, &t, 2);
        a += 2; b += 2;
    }
    if (n & 1) {
        unsigned char t = *a; *a = *b; *b = t;
    }
}

static void scl_heap_sift_up(scl_heap_t *heap, size_t i)
{
    unsigned char *data = heap->data;
    size_t es = heap->element_size;
    scl_cmp_func_t cmp = heap->cmp;
    while (i > 0) {
        size_t p = scl_heap_parent(i);
        if (cmp(data + p * es, data + i * es) <= 0)
            break;
        scl_heap_swap(heap, i, p);
        i = p;
    }
}

static void scl_heap_sift_down(scl_heap_t *heap, size_t i)
{
    unsigned char *data = heap->data;
    size_t es = heap->element_size;
    scl_cmp_func_t cmp = heap->cmp;
    size_t n = heap->count;
    while (1) {
        size_t smallest = i;
        size_t l = scl_heap_left(i);
        size_t r = scl_heap_right(i);

        if (l < n && cmp(data + l * es, data + smallest * es) < 0)
            smallest = l;
        if (r < n && cmp(data + r * es, data + smallest * es) < 0)
            smallest = r;

        if (smallest == i) break;
        scl_heap_swap(heap, i, smallest);
        i = smallest;
    }
}

scl_error_t scl_heap_push(scl_allocator_t *alloc, scl_heap_t *heap, const void *element)
{
    if (!heap || !element) return SCL_ERR_NULL_PTR;

    size_t cnt = heap->count;
    size_t es = heap->element_size;

    if (scl_unlikely(cnt == heap->capacity)) {
        size_t new_cap = heap->capacity == 0 ? 4 : heap->capacity * 2;
        size_t old_bytes = heap->capacity * es;
        size_t new_bytes;
        if (scl_mul_overflow(new_cap, es, &new_bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        unsigned char *tmp = scl_realloc(alloc, heap->data, old_bytes, new_bytes, alignof(max_align_t));
        if (!tmp) return SCL_ERR_OUT_OF_MEMORY;
        heap->data = tmp;
        heap->capacity = new_cap;
    }

    scl_memcpy(heap->data + cnt * es, element, es);
    scl_heap_sift_up(heap, cnt);
    heap->count = cnt + 1;
    return SCL_OK;
}

scl_error_t scl_heap_pop(scl_heap_t *heap, void *out)
{
    if (!heap || !out) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(heap->count == 0)) return SCL_ERR_EMPTY;

    size_t es = heap->element_size;
    scl_memcpy(out, heap->data, es);
    heap->count--;
    if (heap->count > 0) {
        scl_memcpy(heap->data, heap->data + heap->count * es, es);
        scl_heap_sift_down(heap, 0);
    }
    return SCL_OK;
}

scl_error_t scl_heap_peek(const scl_heap_t *heap, void *out)
{
    if (!heap || !out) return SCL_ERR_NULL_PTR;
    if (heap->count == 0) return SCL_ERR_EMPTY;
    scl_memcpy(out, heap->data, heap->element_size);
    return SCL_OK;
}

size_t scl_heap_count(const scl_heap_t *heap) { return heap ? heap->count : 0; }
bool scl_heap_empty(const scl_heap_t *heap) { return heap ? heap->count == 0 : true; }

scl_error_t scl_heap_search(const scl_heap_t *restrict heap, const void *restrict key,
                            scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(!heap || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < heap->count; i++) {
        if (cmp(heap->data + i * heap->element_size, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
