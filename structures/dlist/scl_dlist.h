#ifndef SCL_DLIST_H
#define SCL_DLIST_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_dlist_node {
    struct scl_dlist_node *prev;
    struct scl_dlist_node *next;
    unsigned char data[];  /* flexible array member: element_size bytes */
} scl_dlist_node_t;

typedef struct {
    scl_dlist_node_t *head;
    scl_dlist_node_t *tail;
    size_t element_size;
    size_t count;
} scl_dlist_t;

scl_error_t scl_dlist_init(scl_dlist_t *list, size_t element_size) SCL_WARN_UNUSED;
void        scl_dlist_destroy(scl_allocator_t *alloc, scl_dlist_t *list);
scl_error_t scl_dlist_push_front(scl_allocator_t *alloc, scl_dlist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_dlist_push_back(scl_allocator_t *alloc, scl_dlist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_dlist_pop_front(scl_allocator_t *alloc, scl_dlist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_dlist_pop_back(scl_allocator_t *alloc, scl_dlist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_dlist_front(const scl_dlist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_dlist_back(const scl_dlist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_dlist_insert_at(scl_allocator_t *alloc, scl_dlist_t *list, size_t index, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_dlist_remove_at(scl_allocator_t *alloc, scl_dlist_t *list, size_t index, void *out) SCL_WARN_UNUSED;
scl_error_t scl_dlist_remove(scl_allocator_t *alloc, scl_dlist_t *list, const void *element,
                             int (*cmp)(const void *, const void *)) SCL_WARN_UNUSED;
scl_error_t scl_dlist_search(const scl_dlist_t *list, const void *key,
                             int (*cmp)(const void *, const void *),
                             void *out) SCL_WARN_UNUSED;
size_t      scl_dlist_count(const scl_dlist_t *list);
bool        scl_dlist_empty(const scl_dlist_t *list);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
