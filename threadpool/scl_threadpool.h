#ifndef SCL_THREADPOOL_H
#define SCL_THREADPOOL_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../common/scl_common.h"
#include <stddef.h>
#include <stdatomic.h>
#include <pthread.h>

typedef struct scl_threadpool_task {
    void (*func)(void *);
    void *arg;
    struct scl_threadpool_task *next;
} scl_threadpool_task_t;

typedef struct {
    pthread_t *threads;
    unsigned int thread_count;
    atomic_uintptr_t head;
    atomic_uintptr_t tail;
    atomic_int stopped;
    atomic_uint task_count;
} scl_threadpool_t;

scl_error_t scl_threadpool_init(scl_threadpool_t *pool, unsigned int thread_count);
scl_error_t scl_threadpool_submit(scl_threadpool_t *pool, void (*func)(void *), void *arg);
scl_error_t scl_threadpool_wait(scl_threadpool_t *pool);
scl_error_t scl_threadpool_destroy(scl_threadpool_t *pool);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
