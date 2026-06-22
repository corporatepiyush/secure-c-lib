#ifndef SCL_ALLOC_ARENA_H
#define SCL_ALLOC_ARENA_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>

typedef struct scl_alloc_arena {
    char *buffer;
    size_t offset;
    size_t capacity;
    struct scl_alloc_arena *next;
} scl_alloc_arena_t;

scl_error_t scl_alloc_arena_init(scl_alloc_arena_t *arena, size_t capacity);
scl_error_t scl_alloc_arena_alloc(scl_alloc_arena_t *arena, size_t size, size_t alignment, void **out_ptr);
scl_error_t scl_alloc_arena_reset(scl_alloc_arena_t *arena);
scl_error_t scl_alloc_arena_destroy(scl_alloc_arena_t *arena);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
