#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_threadpool.h"
#include "../stdlib/scl_stdlib.h"
#include "../string/scl_string.h"

#include <pthread.h>

static void *scl_threadpool_worker(void *arg) {
    scl_threadpool_t *pool = (scl_threadpool_t *)arg;

    while (1) {
        pthread_mutex_t *lock = (pthread_mutex_t *)pool->lock;
        pthread_cond_t *cond = (pthread_cond_t *)pool->cond;

        pthread_mutex_lock(lock);

        while (pool->active && !pool->head)
            pthread_cond_wait(cond, lock);

        if (!pool->active) {
            pthread_mutex_unlock(lock);
            return NULL;
        }

        scl_threadpool_task_t *task = pool->head;
        pool->head = task->next;
        if (!pool->head) pool->tail = NULL;
        pool->working++;
        pool->queued--;

        pthread_mutex_unlock(lock);

        task->func(task->arg);

        pthread_mutex_lock(lock);
        pool->working--;
        pthread_cond_broadcast(cond);
        pthread_mutex_unlock(lock);

        scl_free(pool->alloc, task);
    }

    return NULL;
}

static int scl_threadpool_create_threads(scl_threadpool_t *pool) {
    pthread_t *threads = (pthread_t *)scl_calloc(pool->alloc, pool->thread_count, sizeof(pthread_t), _Alignof(max_align_t));
    if (!threads) return -1;
    pool->thread_handles = threads;

    for (unsigned int i = 0; i < pool->thread_count; i++) {
        if (pthread_create(&threads[i], NULL, scl_threadpool_worker, pool) != 0)
            return -1;
    }
    return 0;
}

scl_error_t scl_threadpool_init(scl_allocator_t *alloc, scl_threadpool_t *pool, unsigned int thread_count) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(thread_count == 0)) return SCL_ERR_INVALID_ARG;

    (void)scl_memset(pool, 0, sizeof(*pool));
    pool->alloc = alloc;
    pool->thread_count = thread_count;
    pool->active = 1;

    pthread_mutex_t *lock = (pthread_mutex_t *)scl_alloc(alloc, sizeof(pthread_mutex_t), _Alignof(max_align_t));
    if (!lock) return SCL_ERR_OUT_OF_MEMORY;
    pthread_mutex_init(lock, NULL);
    pool->lock = lock;

    pthread_cond_t *cond = (pthread_cond_t *)scl_alloc(alloc, sizeof(pthread_cond_t), _Alignof(max_align_t));
    if (!cond) {
        pthread_mutex_destroy(lock);
        scl_free(alloc, lock);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    pthread_cond_init(cond, NULL);
    pool->cond = cond;

    if (scl_threadpool_create_threads(pool) != 0) {
        pthread_cond_destroy(cond);
        scl_free(alloc, cond);
        pthread_mutex_destroy(lock);
        scl_free(alloc, lock);
        return SCL_ERR_ALLOC;
    }

    return SCL_OK;
}

scl_error_t scl_threadpool_enqueue(scl_threadpool_t *pool, scl_threadpool_task_fn func, void *arg) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!func)) return SCL_ERR_NULL_PTR;

    scl_threadpool_task_t *task = (scl_threadpool_task_t *)scl_alloc(pool->alloc, sizeof(scl_threadpool_task_t), _Alignof(max_align_t));
    if (!task) return SCL_ERR_OUT_OF_MEMORY;

    task->func = func;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_t *lock = (pthread_mutex_t *)pool->lock;
    pthread_cond_t *cond = (pthread_cond_t *)pool->cond;

    pthread_mutex_lock(lock);

    if (pool->tail) {
        pool->tail->next = task;
    } else {
        pool->head = task;
    }
    pool->tail = task;
    pool->queued++;

    pthread_cond_signal(cond);
    pthread_mutex_unlock(lock);

    return SCL_OK;
}

scl_error_t scl_threadpool_wait(scl_threadpool_t *pool) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;

    pthread_mutex_t *lock = (pthread_mutex_t *)pool->lock;
    pthread_cond_t *cond = (pthread_cond_t *)pool->cond;

    pthread_mutex_lock(lock);
    while (pool->queued > 0 || pool->working > 0)
        pthread_cond_wait(cond, lock);
    pthread_mutex_unlock(lock);

    return SCL_OK;
}

scl_error_t scl_threadpool_destroy(scl_threadpool_t *pool) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;

    pthread_mutex_t *lock = (pthread_mutex_t *)pool->lock;
    pthread_cond_t *cond = (pthread_cond_t *)pool->cond;

    pthread_mutex_lock(lock);
    pool->active = 0;
    pthread_cond_broadcast(cond);
    pthread_mutex_unlock(lock);

    pthread_t *threads = (pthread_t *)pool->thread_handles;
    for (unsigned int i = 0; i < pool->thread_count; i++)
        pthread_join(threads[i], NULL);

    scl_free(pool->alloc, threads);
    pool->thread_handles = NULL;

    while (pool->head) {
        scl_threadpool_task_t *tmp = pool->head;
        pool->head = tmp->next;
        scl_free(pool->alloc, tmp);
    }
    pool->tail = NULL;

    pthread_cond_destroy(cond);
    pthread_mutex_destroy(lock);
    scl_free(pool->alloc, cond);
    scl_free(pool->alloc, lock);
    pool->cond = NULL;
    pool->lock = NULL;

    return SCL_OK;
}
