#ifndef SCL_ALLOC_POOL_H
#define SCL_ALLOC_POOL_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>

typedef struct {
    void *chunk;
    void *free_list;
    size_t block_size;
    size_t total_blocks;
    size_t free_count;
} scl_alloc_pool_t;

scl_error_t scl_alloc_pool_init(scl_alloc_pool_t *pool, size_t block_size, size_t block_count);
scl_error_t scl_alloc_pool_alloc(scl_alloc_pool_t *pool, void **out_ptr);
scl_error_t scl_alloc_pool_free(scl_alloc_pool_t *pool, void *ptr);
scl_error_t scl_alloc_pool_destroy(scl_alloc_pool_t *pool);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
