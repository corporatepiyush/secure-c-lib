#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_threadpool.h"
#include "scl_stdlib.h"
#include "scl_string.h"

static void *scl_threadpool_worker(void *arg) {
    scl_threadpool_t *pool = (scl_threadpool_t *)arg;

    while (1) {
        scl_mutex_lock(&pool->lock);

        while (pool->active && !pool->head)
            scl_cond_wait(&pool->cond, &pool->lock);

        if (!pool->active) {
            scl_mutex_unlock(&pool->lock);
            return NULL;
        }

        scl_threadpool_task_t *task = pool->head;
        pool->head = task->next;
        if (!pool->head) pool->tail = NULL;
        pool->working++;
        pool->queued--;

        scl_mutex_unlock(&pool->lock);

        task->func(task->arg);

        scl_mutex_lock(&pool->lock);
        pool->working--;
        scl_cond_broadcast(&pool->cond);
        scl_mutex_unlock(&pool->lock);

        scl_free(pool->alloc, task);
    }

    return NULL;
}

static scl_error_t scl_threadpool_create_threads(scl_threadpool_t *pool) {
    pool->thread_handles = (scl_thread_t *)scl_calloc(pool->alloc, pool->thread_count, sizeof(scl_thread_t), _Alignof(max_align_t));
    if (!pool->thread_handles) return SCL_ERR_OUT_OF_MEMORY;

    unsigned int created = 0;
    for (unsigned int i = 0; i < pool->thread_count; i++) {
        scl_error_t err = scl_thread_create(&pool->thread_handles[i], scl_threadpool_worker, pool);
        if (err != SCL_OK) {
            /* Stop the threads that did start */
            scl_mutex_lock(&pool->lock);
            pool->active = 0;
            scl_cond_broadcast(&pool->cond);
            scl_mutex_unlock(&pool->lock);
            for (unsigned int j = 0; j < created; j++)
                scl_thread_join(pool->thread_handles[j], NULL);
            scl_free(pool->alloc, pool->thread_handles);
            pool->thread_handles = NULL;
            return err;
        }
        created++;
    }
    return SCL_OK;
}

scl_error_t scl_threadpool_init(scl_allocator_t *alloc, scl_threadpool_t *pool, unsigned int thread_count) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(thread_count == 0)) return SCL_ERR_INVALID_ARG;

    (void)scl_memset(pool, 0, sizeof(*pool));
    pool->alloc = alloc;
    pool->thread_count = thread_count;
    pool->active = 1;

    scl_error_t err;

    err = scl_mutex_init(&pool->lock);
    if (err != SCL_OK) return err;

    err = scl_cond_init(&pool->cond);
    if (err != SCL_OK) {
        scl_mutex_destroy(&pool->lock);
        return err;
    }

    err = scl_threadpool_create_threads(pool);
    if (err != SCL_OK) {
        scl_cond_destroy(&pool->cond);
        scl_mutex_destroy(&pool->lock);
        return err;
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

    scl_mutex_lock(&pool->lock);

    if (pool->tail) {
        pool->tail->next = task;
    } else {
        pool->head = task;
    }
    pool->tail = task;
    pool->queued++;

    scl_cond_signal(&pool->cond);
    scl_mutex_unlock(&pool->lock);

    return SCL_OK;
}

scl_error_t scl_threadpool_wait(scl_threadpool_t *pool) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;

    scl_mutex_lock(&pool->lock);
    while (pool->queued > 0 || pool->working > 0)
        scl_cond_wait(&pool->cond, &pool->lock);
    scl_mutex_unlock(&pool->lock);

    return SCL_OK;
}

scl_error_t scl_threadpool_destroy(scl_threadpool_t *pool) {
    if (scl_unlikely(!pool)) return SCL_ERR_NULL_PTR;

    scl_mutex_lock(&pool->lock);
    pool->active = 0;
    scl_cond_broadcast(&pool->cond);
    scl_mutex_unlock(&pool->lock);

    for (unsigned int i = 0; i < pool->thread_count; i++)
        scl_thread_join(pool->thread_handles[i], NULL);

    scl_free(pool->alloc, pool->thread_handles);
    pool->thread_handles = NULL;

    while (pool->head) {
        scl_threadpool_task_t *tmp = pool->head;
        pool->head = tmp->next;
        scl_free(pool->alloc, tmp);
    }
    pool->tail = NULL;

    scl_cond_destroy(&pool->cond);
    scl_mutex_destroy(&pool->lock);

    return SCL_OK;
}
