#include "scl_array.h"
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

scl_error_t scl_array_init(scl_array_t *arr, size_t element_size, size_t initial_capacity)
{
    if (!arr) return SCL_ERR_NULL_PTR;
    if (element_size == 0) return SCL_ERR_INVALID_ARG;

    arr->data = NULL;
    arr->element_size = element_size;
    arr->capacity = 0;
    arr->count = 0;

    if (initial_capacity > 0) {
        size_t bytes;
        if (scl_mul_overflow(initial_capacity, element_size, &bytes))
            return SCL_ERR_SIZE_OVERFLOW;
        arr->data = malloc(bytes);
        if (!arr->data) return SCL_ERR_OUT_OF_MEMORY;
        arr->capacity = initial_capacity;
    }
    return SCL_OK;
}

void scl_array_destroy(scl_array_t *arr)
{
    if (arr) {
        free(arr->data);
        arr->data = NULL;
        arr->capacity = 0;
        arr->count = 0;
    }
}

scl_error_t scl_array_reserve(scl_array_t *arr, size_t new_capacity)
{
    if (!arr) return SCL_ERR_NULL_PTR;
    if (new_capacity <= arr->capacity) return SCL_OK;

    size_t bytes;
    if (scl_mul_overflow(new_capacity, arr->element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    unsigned char *tmp = realloc(arr->data, bytes);
    if (!tmp) return SCL_ERR_OUT_OF_MEMORY;

    arr->data = tmp;
    arr->capacity = new_capacity;
    return SCL_OK;
}

scl_error_t scl_array_push(scl_array_t *arr, const void *element)
{
    if (!arr || !element) return SCL_ERR_NULL_PTR;

    if (arr->count == arr->capacity) {
        size_t new_cap = arr->capacity == 0 ? 4 : arr->capacity * 2;
        if (new_cap <= arr->capacity) {
            if (arr->capacity == 0) new_cap = 4;
            else return SCL_ERR_SIZE_OVERFLOW;
        }
        scl_error_t err = scl_array_reserve(arr, new_cap);
        if (err != SCL_OK) return err;
    }

    size_t offset;
    if (scl_mul_overflow(arr->count, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(arr->data + offset, element, arr->element_size);
    arr->count++;
    return SCL_OK;
}

scl_error_t scl_array_pop(scl_array_t *arr, void *out)
{
    if (!arr || !out) return SCL_ERR_NULL_PTR;
    if (arr->count == 0) return SCL_ERR_EMPTY;

    arr->count--;
    size_t offset;
    if (scl_mul_overflow(arr->count, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, arr->data + offset, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_array_get(const scl_array_t *arr, size_t index, void *out)
{
    if (!arr || !out) return SCL_ERR_NULL_PTR;
    if (index >= arr->count) return SCL_ERR_INVALID_INDEX;

    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, arr->data + offset, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_array_set(scl_array_t *arr, size_t index, const void *element)
{
    if (!arr || !element) return SCL_ERR_NULL_PTR;
    if (index >= arr->count) return SCL_ERR_INVALID_INDEX;

    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(arr->data + offset, element, arr->element_size);
    return SCL_OK;
}

scl_error_t scl_array_insert(scl_array_t *arr, size_t index, const void *element)
{
    if (!arr || !element) return SCL_ERR_NULL_PTR;
    if (index > arr->count) return SCL_ERR_INVALID_INDEX;

    if (arr->count == arr->capacity) {
        size_t new_cap = arr->capacity == 0 ? 4 : arr->capacity * 2;
        scl_error_t err = scl_array_reserve(arr, new_cap);
        if (err != SCL_OK) return err;
    }

    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    size_t tail_count = arr->count - index;
    if (tail_count > 0) {
        memmove(arr->data + offset + arr->element_size,
                arr->data + offset,
                tail_count * arr->element_size);
    }

    memcpy(arr->data + offset, element, arr->element_size);
    arr->count++;
    return SCL_OK;
}

scl_error_t scl_array_remove(scl_array_t *arr, size_t index, void *out)
{
    if (!arr || !out) return SCL_ERR_NULL_PTR;
    if (index >= arr->count) return SCL_ERR_INVALID_INDEX;

    size_t offset;
    if (scl_mul_overflow(index, arr->element_size, &offset))
        return SCL_ERR_SIZE_OVERFLOW;

    memcpy(out, arr->data + offset, arr->element_size);

    size_t tail_count = arr->count - index - 1;
    if (tail_count > 0) {
        memmove(arr->data + offset,
                arr->data + offset + arr->element_size,
                tail_count * arr->element_size);
    }
    arr->count--;
    return SCL_OK;
}

scl_error_t scl_array_shrink(scl_array_t *arr)
{
    if (!arr) return SCL_ERR_NULL_PTR;
    if (arr->count == arr->capacity) return SCL_OK;

    size_t bytes;
    if (scl_mul_overflow(arr->count, arr->element_size, &bytes))
        return SCL_ERR_SIZE_OVERFLOW;

    if (arr->count == 0) {
        free(arr->data);
        arr->data = NULL;
        arr->capacity = 0;
        return SCL_OK;
    }

    unsigned char *tmp = realloc(arr->data, bytes);
    if (!tmp) return SCL_ERR_OUT_OF_MEMORY;

    arr->data = tmp;
    arr->capacity = arr->count;
    return SCL_OK;
}

size_t scl_array_count(const scl_array_t *arr)
{
    return arr ? arr->count : 0;
}

size_t scl_array_capacity(const scl_array_t *arr)
{
    return arr ? arr->capacity : 0;
}

bool scl_array_empty(const scl_array_t *arr)
{
    return arr ? arr->count == 0 : true;
}

scl_error_t scl_array_linear_search(const scl_array_t *restrict arr, const void *restrict key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *restrict out_index)
{
    if (__builtin_expect(!arr || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    for (size_t i = 0; i < arr->count; i++) {
        if (cmp(arr->data + i * arr->element_size, key) == 0) {
            *out_index = i;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_array_binary_search(const scl_array_t *restrict arr, const void *restrict key,
                                    int (*cmp)(const void *, const void *),
                                    size_t *restrict out_index)
{
    if (__builtin_expect(!arr || !key || !cmp || !out_index, 0))
        return SCL_ERR_NULL_PTR;

    size_t lo = 0, hi = arr->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(arr->data + mid * arr->element_size, key);
        if (c < 0)
            lo = mid + 1;
        else if (c > 0)
            hi = mid;
        else {
            *out_index = mid;
            return SCL_OK;
        }
    }
    return SCL_ERR_NOT_FOUND;
}
