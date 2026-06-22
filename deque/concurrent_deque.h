#ifndef SCL_CONCURRENT_DEQUE_H
#define SCL_CONCURRENT_DEQUE_H

#include "../common/scl_common.h"
#include <stdatomic.h>

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

typedef struct {
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    size_t head;
    atomic_size_t count;
    atomic_flag lock;
} scl_concurrent_deque_t;

scl_error_t scl_concurrent_deque_init(scl_concurrent_deque_t *deque, size_t element_size, size_t capacity) SCL_WARN_UNUSED;
void        scl_concurrent_deque_destroy(scl_concurrent_deque_t *deque);
scl_error_t scl_concurrent_deque_push_front(scl_concurrent_deque_t *deque, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_deque_push_back(scl_concurrent_deque_t *deque, const void *element) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_deque_pop_front(scl_concurrent_deque_t *deque, void *out) SCL_WARN_UNUSED;
scl_error_t scl_concurrent_deque_pop_back(scl_concurrent_deque_t *deque, void *out) SCL_WARN_UNUSED;
size_t      scl_concurrent_deque_count(const scl_concurrent_deque_t *deque);
bool        scl_concurrent_deque_empty(const scl_concurrent_deque_t *deque);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
