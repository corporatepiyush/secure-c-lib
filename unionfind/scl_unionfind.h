#ifndef SCL_UNIONFIND_H
#define SCL_UNIONFIND_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    size_t *parent;
    size_t *rank;
    size_t count;
    size_t sets;
} scl_unionfind_t;

scl_error_t scl_unionfind_init(scl_allocator_t *alloc, scl_unionfind_t *uf, size_t count) SCL_WARN_UNUSED;
void        scl_unionfind_destroy(scl_allocator_t *alloc, scl_unionfind_t *uf);
size_t      scl_unionfind_find(scl_unionfind_t *uf, size_t x);
scl_error_t scl_unionfind_union(scl_unionfind_t *uf, size_t x, size_t y) SCL_WARN_UNUSED;
bool        scl_unionfind_connected(const scl_unionfind_t *uf, size_t x, size_t y);
size_t      scl_unionfind_count(const scl_unionfind_t *uf);
size_t      scl_unionfind_sets(const scl_unionfind_t *uf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
