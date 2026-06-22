#ifndef SCL_SLIST_H
#define SCL_SLIST_H

#include "../common/scl_common.h"

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct scl_slist_node {
    struct scl_slist_node *next;
    unsigned char data[];  /* flexible array member: element_size bytes */
} scl_slist_node_t;

typedef struct {
    scl_slist_node_t *head;
    scl_slist_node_t *tail;
    size_t element_size;
    size_t count;
} scl_slist_t;

scl_error_t scl_slist_init(scl_slist_t *list, size_t element_size) SCL_WARN_UNUSED;
void        scl_slist_destroy(scl_allocator_t *alloc, scl_slist_t *list);
scl_error_t scl_slist_push_front(scl_allocator_t *alloc, scl_slist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_slist_push_back(scl_allocator_t *alloc, scl_slist_t *list, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_slist_pop_front(scl_allocator_t *alloc, scl_slist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_slist_front(const scl_slist_t *list, void *out) SCL_WARN_UNUSED;
scl_error_t scl_slist_back(const scl_slist_t *list, void *out) SCL_WARN_UNUSED;
size_t      scl_slist_count(const scl_slist_t *list);
bool        scl_slist_empty(const scl_slist_t *list);
scl_error_t scl_slist_remove(scl_allocator_t *alloc, scl_slist_t *list, const void *element,
                             int (*cmp)(const void *, const void *)) SCL_WARN_UNUSED;
scl_error_t scl_slist_search(const scl_slist_t *list, const void *key,
                             int (*cmp)(const void *, const void *),
                             void *out) SCL_WARN_UNUSED;

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
