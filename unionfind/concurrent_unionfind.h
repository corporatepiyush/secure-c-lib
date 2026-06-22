#ifndef SCL_CONCURRENT_UNIONFIND_H
#define SCL_CONCURRENT_UNIONFIND_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    atomic_uint *parent;
    atomic_uint *rank;
    size_t count;
    atomic_size_t sets;
} scl_concurrent_unionfind_t;

scl_error_t scl_concurrent_unionfind_init(scl_concurrent_unionfind_t *uf, size_t count) SCL_WARN_UNUSED;
void        scl_concurrent_unionfind_destroy(scl_concurrent_unionfind_t *uf);
size_t      scl_concurrent_unionfind_find(scl_concurrent_unionfind_t *uf, size_t x);
scl_error_t scl_concurrent_unionfind_union(scl_concurrent_unionfind_t *uf, size_t x, size_t y) SCL_WARN_UNUSED;
bool        scl_concurrent_unionfind_connected(scl_concurrent_unionfind_t *uf, size_t x, size_t y);
size_t      scl_concurrent_unionfind_count(const scl_concurrent_unionfind_t *uf);
size_t      scl_concurrent_unionfind_sets(const scl_concurrent_unionfind_t *uf);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
