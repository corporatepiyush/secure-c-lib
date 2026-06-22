#ifndef SCL_CONCURRENT_UNIONFIND_H
#define SCL_CONCURRENT_UNIONFIND_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    atomic_uint *parent;
    atomic_uint *rank;
    size_t count;
    atomic_size_t sets;
} scl_atomic_unionfind_t;

scl_error_t scl_atomic_unionfind_init(scl_allocator_t *alloc, scl_atomic_unionfind_t *uf, size_t count) SCL_WARN_UNUSED;
void        scl_atomic_unionfind_destroy(scl_allocator_t *alloc, scl_atomic_unionfind_t *uf);
size_t      scl_atomic_unionfind_find(scl_atomic_unionfind_t *uf, size_t x);
scl_error_t scl_atomic_unionfind_union(scl_atomic_unionfind_t *uf, size_t x, size_t y) SCL_WARN_UNUSED;
bool        scl_atomic_unionfind_connected(scl_atomic_unionfind_t *uf, size_t x, size_t y);
size_t      scl_atomic_unionfind_count(const scl_atomic_unionfind_t *uf);
size_t      scl_atomic_unionfind_sets(const scl_atomic_unionfind_t *uf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
