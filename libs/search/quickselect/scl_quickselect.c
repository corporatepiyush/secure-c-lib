#include "scl_quickselect.h"
#include "scl_string.h"

static void swap_elements(void *a, void *b, size_t elem_size)
{
    if (a == b) return;
    size_t i = 0;
    while (i < elem_size) {
        unsigned char t = ((unsigned char *)a)[i];
        ((unsigned char *)a)[i] = ((unsigned char *)b)[i];
        ((unsigned char *)b)[i] = t;
        i++;
    }
}

static size_t partition(void *base, size_t lo, size_t hi, size_t elem_size, scl_cmp_func_t cmp)
{
    unsigned char *bytes = (unsigned char *)base;
    void *pivot = bytes + hi * elem_size;
    size_t i = lo;
    for (size_t j = lo; j < hi; j++) {
        if (cmp(bytes + j * elem_size, pivot) < 0) {
            swap_elements(bytes + i * elem_size, bytes + j * elem_size, elem_size);
            i++;
        }
    }
    swap_elements(bytes + i * elem_size, bytes + hi * elem_size, elem_size);
    return i;
}

scl_error_t scl_search_quickselect(scl_allocator_t *alloc, void *base, size_t count, size_t elem_size, scl_cmp_func_t cmp, size_t k, void *out)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;
    if (__builtin_expect(k >= count, 0)) return SCL_ERR_INVALID_INDEX;

    size_t bytes;
    if (scl_mul_overflow(count, elem_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;
    void *copy = scl_alloc(alloc, bytes, alignof(max_align_t));
    if (__builtin_expect(copy == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;
    scl_memcpy(copy, base, bytes);

    size_t lo = 0, hi = count - 1;
    while (lo < hi) {
        size_t p = partition(copy, lo, hi, elem_size, cmp);
        if (p == k) break;
        if (p > k) hi = (p == 0) ? 0 : p - 1;
        else lo = p + 1;
    }

    scl_memcpy(out, (unsigned char *)copy + k * elem_size, elem_size);
    scl_free(alloc, copy);
    return SCL_OK;
}
