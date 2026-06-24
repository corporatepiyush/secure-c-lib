#ifndef SCL_ALLOC_POOL_H
#define SCL_ALLOC_POOL_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_allocator_t *scl_alloc_pool_create(scl_allocator_t *backing, size_t block_size, size_t block_count, size_t alignment) SCL_WARN_UNUSED;
void scl_alloc_pool_destroy(scl_allocator_t *alloc);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
