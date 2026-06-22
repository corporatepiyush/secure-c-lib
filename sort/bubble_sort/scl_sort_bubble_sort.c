#include "scl_sort_bubble_sort.h"
#include <string.h>
#include <stdbool.h>

static void swap(unsigned char *a, unsigned char *b, size_t element_size)
{
    while (element_size--) {
        unsigned char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

scl_error_t scl_sort_bubble_sort(void *base, size_t count, size_t element_size, scl_cmp_func_t cmp)
{
    if (!base || !cmp) return SCL_ERR_NULL_PTR;
    if (count < 2) return SCL_OK;

    bool swapped;
    for (size_t i = 0; i < count - 1; i++) {
        swapped = false;
        for (size_t j = 0; j < count - 1 - i; j++) {
            if (cmp((unsigned char *)base + (j + 1) * element_size,
                    (unsigned char *)base + j * element_size) < 0) {
                swap((unsigned char *)base + j * element_size,
                     (unsigned char *)base + (j + 1) * element_size,
                     element_size);
                swapped = true;
            }
        }
        if (!swapped) break;
    }
    return SCL_OK;
}
