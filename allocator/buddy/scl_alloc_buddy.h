#ifndef SCL_ALLOC_BUDDY_H
#define SCL_ALLOC_BUDDY_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>

#define SCL_BUDDY_MAX_ORDER 20

typedef struct scl_buddy_node {
    struct scl_buddy_node *next;
    unsigned int order;
} scl_buddy_node_t;

typedef struct {
    unsigned char *pool;
    size_t pool_size;
    unsigned int max_order;
    scl_buddy_node_t *free_lists[SCL_BUDDY_MAX_ORDER + 1];
} scl_alloc_buddy_t;

scl_error_t scl_alloc_buddy_init(scl_alloc_buddy_t *buddy, unsigned int max_order);
scl_error_t scl_alloc_buddy_alloc(scl_alloc_buddy_t *buddy, size_t size, void **out_ptr);
scl_error_t scl_alloc_buddy_free(scl_alloc_buddy_t *buddy, void *ptr);
scl_error_t scl_alloc_buddy_destroy(scl_alloc_buddy_t *buddy);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
