#ifndef SCL_THREADPOOL_H
#define SCL_THREADPOOL_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "scl_pthread.h"

typedef void (*scl_threadpool_task_fn)(void *arg);

typedef struct scl_threadpool_task {
    scl_threadpool_task_fn func;
    void *arg;
    struct scl_threadpool_task *next;
} scl_threadpool_task_t;

typedef struct {
    scl_allocator_t *alloc;
    unsigned int thread_count;
    int active;
    scl_threadpool_task_t *head;
    scl_threadpool_task_t *tail;
    scl_pthread_t *thread_handles;
    scl_mutex_t lock;
    scl_cond_t cond;
    unsigned int working;
    unsigned int queued;
} scl_threadpool_t;

scl_error_t scl_threadpool_init(scl_allocator_t *alloc, scl_threadpool_t *pool, unsigned int thread_count);
scl_error_t scl_threadpool_enqueue(scl_threadpool_t *pool, scl_threadpool_task_fn func, void *arg);
scl_error_t scl_threadpool_wait(scl_threadpool_t *pool);
scl_error_t scl_threadpool_destroy(scl_threadpool_t *pool);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
