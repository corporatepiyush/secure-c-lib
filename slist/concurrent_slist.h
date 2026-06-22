#ifndef SCL_CONCURRENT_SLIST_H
#define SCL_CONCURRENT_SLIST_H

#include "../common/scl_concurrent_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_atomic_slist_node {
    void *data;
    struct scl_atomic_slist_node *next;
} scl_atomic_slist_node_t;

typedef struct {
    atomic_uintptr_t head;
    size_t element_size;
    atomic_size_t count;
} scl_atomic_slist_t;

scl_error_t scl_atomic_slist_init(scl_allocator_t *alloc, scl_atomic_slist_t *list, size_t element_size) SCL_WARN_UNUSED;
void        scl_atomic_slist_destroy(scl_allocator_t *alloc, scl_atomic_slist_t *list);
scl_error_t scl_atomic_slist_push_front(scl_allocator_t *alloc, scl_atomic_slist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_atomic_slist_pop_front(scl_allocator_t *alloc, scl_atomic_slist_t *list, void *out) SCL_WARN_UNUSED;
size_t      scl_atomic_slist_count(const scl_atomic_slist_t *list);
bool        scl_atomic_slist_empty(const scl_atomic_slist_t *list);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
