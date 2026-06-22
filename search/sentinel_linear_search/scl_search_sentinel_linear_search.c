#include "scl_search_sentinel_linear_search.h"
#include <string.h>

scl_error_t scl_search_sentinel_linear_search(scl_allocator_t *alloc, const void *restrict base, size_t count, size_t elem_size, const void *restrict key, scl_cmp_func_t cmp, size_t *restrict out_index)
{
    if (__builtin_expect(base == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(key == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(cmp == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(out_index == NULL, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(count == 0, 0)) return SCL_ERR_EMPTY;

    unsigned char *buf = (unsigned char *)scl_alloc(alloc, elem_size, alignof(max_align_t));
    if (__builtin_expect(buf == NULL, 0)) return SCL_ERR_OUT_OF_MEMORY;
    memcpy(buf, key, elem_size);

    size_t bytes;
    if (scl_mul_overflow(count, elem_size, &bytes)) { scl_free(alloc, buf); return SCL_ERR_SIZE_OVERFLOW; }
    unsigned char *copy = (unsigned char *)scl_alloc(alloc, bytes, alignof(max_align_t));
    if (__builtin_expect(copy == NULL, 0)) { scl_free(alloc, buf); return SCL_ERR_OUT_OF_MEMORY; }
    memcpy(copy, base, bytes);

    size_t i = 0;
    while (1) {
        if (i < count) {
            int r = cmp(copy + i * elem_size, buf);
            if (r == 0) break;
        }
        i++;
        if (i > count) break;
    }

    scl_free(alloc, buf);
    scl_free(alloc, copy);

    if (i < count) {
        *out_index = i;
        return SCL_OK;
    }
    return SCL_ERR_NOT_FOUND;
}
