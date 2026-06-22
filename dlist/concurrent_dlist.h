#ifndef SCL_CONCURRENT_DLIST_H
#define SCL_CONCURRENT_DLIST_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_concurrent_dlist_node {
    void *data;
    struct scl_concurrent_dlist_node *prev;
    struct scl_concurrent_dlist_node *next;
} scl_concurrent_dlist_node_t;

typedef struct {
    scl_concurrent_dlist_node_t *head;
    scl_concurrent_dlist_node_t *tail;
    size_t element_size;
    atomic_size_t count;
    atomic_flag lock;
} scl_concurrent_dlist_t;

scl_error_t scl_concurrent_dlist_init(scl_concurrent_dlist_t *list, size_t element_size) SCL_WARN_UNUSED;
void        scl_concurrent_dlist_destroy(scl_concurrent_dlist_t *list);
scl_error_t scl_concurrent_dlist_push_front(scl_concurrent_dlist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_dlist_push_back(scl_concurrent_dlist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_dlist_pop_front(scl_concurrent_dlist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_dlist_pop_back(scl_concurrent_dlist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_dlist_insert_at(scl_concurrent_dlist_t *list, size_t index, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_dlist_remove_at(scl_concurrent_dlist_t *list, size_t index, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_dlist_count(const scl_concurrent_dlist_t *list);
bool        scl_concurrent_dlist_empty(const scl_concurrent_dlist_t *list);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
