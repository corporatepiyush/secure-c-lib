#ifndef SCL_CONCURRENT_SLIST_H
#define SCL_CONCURRENT_SLIST_H

#include "scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_slist_node {
    void *data;
    struct scl_concurrent_slist_node *next;
} scl_concurrent_slist_node_t;

typedef struct {
    _Atomic scl_tagged_ptr_t head;
    size_t element_size;
    atomic_size_t count;
} scl_concurrent_slist_t;

scl_error_t scl_cslist_init(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_slist_t *SCL_RESTRICT list, size_t element_size) SCL_WARN_UNUSED;
void        scl_cslist_destroy(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_slist_t *SCL_RESTRICT list);
scl_error_t scl_cslist_push_front(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_slist_t *SCL_RESTRICT list, const void *SCL_RESTRICT element) SCL_WARN_UNUSED;
scl_error_t scl_cslist_pop_front(scl_allocator_t *SCL_RESTRICT alloc, scl_concurrent_slist_t *SCL_RESTRICT list, void *SCL_RESTRICT out) SCL_WARN_UNUSED;
SCL_PURE size_t      scl_cslist_count(const scl_concurrent_slist_t *SCL_RESTRICT list);
SCL_PURE bool        scl_cslist_empty(const scl_concurrent_slist_t *SCL_RESTRICT list);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
