#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_threadpool.h"
#include <stdlib.h>
#include <string.h>

static scl_threadpool_task_t *atomic_load_task(atomic_uintptr_t *ptr) {
    return (scl_threadpool_task_t *)atomic_load_explicit(ptr, memory_order_acquire);
}

static int atomic_cas_task(atomic_uintptr_t *ptr, scl_threadpool_task_t **expected, scl_threadpool_task_t *desired) {
    uintptr_t exp = (uintptr_t)*expected;
    return atomic_compare_exchange_strong_explicit(ptr, &exp, (uintptr_t)desired,
                                                   memory_order_acq_rel, memory_order_acquire);
}

static void *worker_thread(void *arg) {
    scl_threadpool_t *pool = (scl_threadpool_t *)arg;

    while (1) {
        if (atomic_load_explicit(&pool->stopped, memory_order_acquire))
            break;

        // CAS-based dequeue
        scl_threadpool_task_t *head = atomic_load_task(&pool->head);
        if (head) {
            scl_threadpool_task_t *next = atomic_load_task((atomic_uintptr_t *)&head->next);
            if (atomic_cas_task(&pool->head, &head, next)) {
                head->func(head->arg);
                atomic_fetch_sub_explicit(&pool->task_count, 1, memory_order_release);
                free(head);
                continue;
            }
        }

        // No work - yield
        pthread_yield_np();
    }

    return NULL;
}

scl_error_t scl_threadpool_init(scl_threadpool_t *pool, unsigned int thread_count) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(thread_count == 0, 0)) return SCL_ERR_INVALID_ARG;

    memset(pool, 0, sizeof(*pool));

    pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
    if (__builtin_expect(!pool->threads, 0)) return SCL_ERR_OUT_OF_MEMORY;

    pool->thread_count = thread_count;
    atomic_init(&pool->head, 0);
    atomic_init(&pool->tail, 0);
    atomic_init(&pool->stopped, 0);
    atomic_init(&pool->task_count, 0);

    for (unsigned int i = 0; i < thread_count; i++) {
        if (__builtin_expect(pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0, 0)) {
            // Clean up already-created threads
            atomic_store_explicit(&pool->stopped, 1, memory_order_release);
            for (unsigned int j = 0; j < i; j++)
                pthread_join(pool->threads[j], NULL);
            free(pool->threads);
            pool->threads = NULL;
            return SCL_ERR_ALLOC;
        }
    }

    return SCL_OK;
}

scl_error_t scl_threadpool_submit(scl_threadpool_t *pool, void (*func)(void *), void *arg) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!func, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(atomic_load_explicit(&pool->stopped, memory_order_acquire), 0))
        return SCL_ERR_INVALID_STATE;

    scl_threadpool_task_t *task = (scl_threadpool_task_t *)malloc(sizeof(scl_threadpool_task_t));
    if (__builtin_expect(!task, 0)) return SCL_ERR_OUT_OF_MEMORY;

    task->func = func;
    task->arg = arg;
    task->next = NULL;

    // CAS-based enqueue
    while (1) {
        scl_threadpool_task_t *head = atomic_load_task(&pool->head);
        task->next = head;
        if (atomic_cas_task(&pool->head, &head, task)) {
            atomic_fetch_add_explicit(&pool->task_count, 1, memory_order_release);
            break;
        }
    }

    return SCL_OK;
}

scl_error_t scl_threadpool_wait(scl_threadpool_t *pool) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;

    while (atomic_load_explicit(&pool->task_count, memory_order_acquire) > 0) {
        pthread_yield_np();
    }

    return SCL_OK;
}

scl_error_t scl_threadpool_destroy(scl_threadpool_t *pool) {
    if (__builtin_expect(!pool, 0)) return SCL_ERR_NULL_PTR;

    atomic_store_explicit(&pool->stopped, 1, memory_order_release);

    // Drain remaining tasks
    while (1) {
        scl_threadpool_task_t *head = atomic_load_task(&pool->head);
        if (!head) break;
        scl_threadpool_task_t *next = atomic_load_task((atomic_uintptr_t *)&head->next);
        if (atomic_cas_task(&pool->head, &head, next)) {
            free(head);
        }
    }

    for (unsigned int i = 0; i < pool->thread_count; i++)
        pthread_join(pool->threads[i], NULL);

    free(pool->threads);
    pool->threads = NULL;
    pool->thread_count = 0;
    return SCL_OK;
}
