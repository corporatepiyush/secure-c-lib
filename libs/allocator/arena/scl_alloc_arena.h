#ifndef SCL_ALLOC_ARENA_H
#define SCL_ALLOC_ARENA_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"

scl_allocator_t *scl_alloc_arena_create(scl_allocator_t *backing, size_t capacity);
void scl_alloc_arena_reset(scl_allocator_t *alloc);
void scl_alloc_arena_destroy(scl_allocator_t *alloc);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
