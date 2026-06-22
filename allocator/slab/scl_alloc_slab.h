#ifndef SCL_ALLOC_SLAB_H
#define SCL_ALLOC_SLAB_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>

#define SCL_ALLOC_SLAB_CLASSES 10

typedef struct {
    void *chunk;
    void *free_list;
    size_t block_size;
    size_t total_blocks;
    size_t free_count;
} scl_alloc_slab_pool_t;

typedef struct {
    scl_alloc_slab_pool_t pools[SCL_ALLOC_SLAB_CLASSES];
} scl_alloc_slab_t;

extern const size_t scl_alloc_slab_sizes[SCL_ALLOC_SLAB_CLASSES];

scl_error_t scl_alloc_slab_init(scl_alloc_slab_t *slab);
scl_error_t scl_alloc_slab_alloc(scl_alloc_slab_t *slab, size_t size, void **out_ptr);
scl_error_t scl_alloc_slab_free(scl_alloc_slab_t *slab, void *ptr);
scl_error_t scl_alloc_slab_destroy(scl_alloc_slab_t *slab);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
