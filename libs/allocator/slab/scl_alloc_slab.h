#ifndef SCL_ALLOC_SLAB_H
#define SCL_ALLOC_SLAB_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_allocator_t *scl_alloc_slab_create(scl_allocator_t *backing, const size_t *bucket_sizes, size_t num_buckets);
void scl_alloc_slab_destroy(scl_allocator_t *alloc);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
