#ifndef SCL_ALLOC_ARENA_H
#define SCL_ALLOC_ARENA_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

scl_allocator_t *scl_alloc_arena_create(scl_allocator_t *backing, size_t capacity, size_t max_capacity) SCL_WARN_UNUSED;
void scl_alloc_arena_reset(scl_allocator_t *alloc);
void scl_alloc_arena_destroy(scl_allocator_t *alloc);

/* Arena stats (optional introspection) */
typedef struct {
    size_t bytes_used;
    size_t bytes_wasted;
    size_t node_count;
} scl_alloc_arena_stats_t;

bool scl_alloc_arena_stats(scl_allocator_t *alloc, scl_alloc_arena_stats_t *out);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
