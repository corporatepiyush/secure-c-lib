#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_fibonacci.h"

scl_error_t scl_search_fibonacci_search(const void * base, size_t count, size_t elem_size, const void * key, scl_cmp_func_t cmp, size_t * out_index)
{
    if (scl_unlikely(base == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(key == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(cmp == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(out_index == NULL)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count == 0)) return SCL_ERR_EMPTY;

    size_t fib2 = 0, fib1 = 1, fib = fib1 + fib2;
    while (fib < count) {
        fib2 = fib1;
        fib1 = fib;
        fib = fib1 + fib2;
    }

    size_t offset = 0;
    const unsigned char *bytes = (const unsigned char *)base;

    while (fib > 1) {
        size_t i = offset + fib2;
        if (i >= count) i = count - 1;

        const void *elem = bytes + i * elem_size;
        int r = cmp(elem, key);
        if (r == 0) {
            *out_index = i;
            return SCL_OK;
        }
        if (r < 0) {
            fib = fib1;
            fib1 = fib2;
            fib2 = (fib > fib1) ? fib - fib1 : 0;
            offset = i + 1;
        } else {
            fib = fib2;
            fib1 = fib1 - fib2;
            fib2 = fib - fib1;
        }
    }

    if (fib1 && offset < count) {
        const void *elem = bytes + offset * elem_size;
        if (cmp(elem, key) == 0) {
            *out_index = offset;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
