#include "scl_selection.h"
#include <string.h>

static SCL_ALWAYS_INLINE void swap(unsigned char * a, unsigned char * b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

scl_error_t scl_sort_selection_sort(void * base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (scl_unlikely(!base || !cmp)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(count < 2)) return SCL_OK;

    for (size_t i = 0; i < count - 1; i++) {
        size_t min_idx = i;
        for (size_t j = i + 1; j < count; j++) {
            if (cmp((unsigned char *)base + j * element_size,
                    (unsigned char *)base + min_idx * element_size) < 0)
                min_idx = j;
        }
        if (scl_unlikely(min_idx != i))
            swap((unsigned char *)base + i * element_size,
                 (unsigned char *)base + min_idx * element_size,
                 element_size);
    }
    return SCL_OK;
}
