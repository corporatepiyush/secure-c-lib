#include "scl_sort_insertion_sort.h"
#include <string.h>

static void swap(unsigned char *a, unsigned char *b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

scl_error_t scl_sort_insertion_sort(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    for (size_t i = 1; i < count; i++) {
        size_t j = i;
        while (j > 0 && cmp((unsigned char *)base + j * element_size,
                            (unsigned char *)base + (j - 1) * element_size) < 0) {
            swap((unsigned char *)base + j * element_size,
                 (unsigned char *)base + (j - 1) * element_size,
                 element_size);
            j--;
        }
    }
    return SCL_OK;
}
