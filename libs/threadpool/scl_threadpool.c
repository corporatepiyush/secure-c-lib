/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Fixed-size worker thread pool (max 128). Mutex+condvar dispatch,
 * scl_threadpool_submit with optional scl_future_t for synchronous result
 * retrieval. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_threadpool.h"
#include "scl_stdlib.h"
#include "scl_string.h"

/*
 * scl_threadpool.c — fixed-size worker thread pool.
 *
 * THREADING MODEL
 * ───────────────
 * The pool uses a single mutex (lock) + condition variable (cond)
 * to protect the shared task queue.  This is a classic producer-
 * consumer design:
 *
 *   Producers (callers of scl_threadpool_enqueue):
 *     - Lock, append to tail, signal, unlock.
 *     - Fast path: O(1) lock, no syscall if a worker is already
 *       awake (cond_signal is a no-op when no thread is waiting).
 *
 *   Consumers (worker threads):
 *     - Lock, wait while queue is empty AND pool is active.
 *     - Dequeue from head, increment working, unlock.
 *     - Execute task (outside the lock — execution can block).
 *     - Lock, decrement working, broadcast (to wake wait()),
 *       unlock.  Free the task node.
 *
 * The "working" counter lets scl_threadpool_wait() know when all
 * in-flight tasks have completed.  We use broadcast (not signal)
 * when decrementing working because multiple waiters may be
 * waiting in scl_threadpool_wait().
 *
 * SHUTDOWN SEQUENCE
 * ─────────────────
 * 1. Set pool->active = 0 under the lock.
 * 2. Broadcast the condition variable (wakes all workers).
 * 3. Workers see !active, exit their loop, return NULL.
 * 4. The initiator joins all threads (waits for them to exit).
 * 5. Free the thread handle array.
 * 6. Drain any tasks still in the queue (should be none if
 *    the caller called scl_threadpool_wait first, but we handle
 *    it defensively).
 * 7. Destroy mutex + condvar.
 *
 * The shutdown does NOT interrupt a running task — it waits for
 * the task to finish (via scl_thread_join).  If the caller needs
 * cancellation, they must implement it in the task function
 * (e.g., via an atomic flag).
 *
 * THREAD_COUNT BOUNDS
 * ───────────────────
 * We cap thread_count at SCL_THREADPOOL_MAX_THREADS (128) to
 * prevent accidental resource exhaustion.  Each thread consumes
 * ~8 MB of virtual address space for its stack (on macOS/Linux),
 * so 128 threads ~ 1 GB of virtual address space.
 */

static void *scl_threadpool_worker(void *arg) {
  scl_threadpool_t *pool = (scl_threadpool_t *)arg;

  while (1) {
    scl_mutex_lock(&pool->lock);

    /*
     * Wait while the queue is empty and the pool is still active.
     * Spurious wakeups are handled by re-checking the predicate.
     */
    while (scl_likely(pool->active) && !pool->head)
      scl_cond_wait(&pool->cond, &pool->lock);

    /* Shutdown signal received — exit. */
    if (scl_unlikely(!pool->active)) {
      scl_mutex_unlock(&pool->lock);
      return NULL;
    }

    /* Dequeue the head task. */
    scl_threadpool_task_t *task = pool->head;
    pool->head = task->next;
    if (!pool->head)
      pool->tail = NULL;
    pool->working++;
    pool->queued--;

    scl_mutex_unlock(&pool->lock);

    /* Execute the task (outside the lock so it can block). */
    task->func(task->arg);

    /* Notify waiters that a task slot has freed up. */
    scl_mutex_lock(&pool->lock);
    pool->working--;
    scl_cond_broadcast(&pool->cond);
    scl_mutex_unlock(&pool->lock);

    scl_free(pool->alloc, task);
  }

  return NULL;
}

static scl_error_t scl_threadpool_create_threads(scl_threadpool_t *pool) {
  pool->thread_handles =
      (scl_thread_t *)scl_calloc(pool->alloc, pool->thread_count,
                                 sizeof(scl_thread_t), _Alignof(max_align_t));
  if (!pool->thread_handles)
    return SCL_ERR_OUT_OF_MEMORY;

  unsigned int created = 0;
  for (unsigned int i = 0; i < pool->thread_count; i++) {
    scl_error_t err = scl_thread_create(&pool->thread_handles[i],
                                        scl_threadpool_worker, pool);
    if (err != SCL_OK) {
      /* Partial failure: stop created threads, join them, clean up. */
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

scl_error_t scl_threadpool_init(scl_allocator_t *alloc, scl_threadpool_t *pool,
                                unsigned int thread_count) {
  if (scl_unlikely(!pool))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(thread_count == 0))
    return SCL_ERR_INVALID_ARG;
  if (scl_unlikely(thread_count > SCL_THREADPOOL_MAX_THREADS))
    return SCL_ERR_INVALID_ARG;

  (void)scl_memset(pool, 0, sizeof(*pool));
  pool->alloc = alloc;
  pool->thread_count = thread_count;
  pool->active = 1;

  scl_error_t err;

  err = scl_mutex_init(&pool->lock);
  if (err != SCL_OK)
    return err;

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

scl_error_t scl_threadpool_enqueue(scl_threadpool_t *pool,
                                   scl_threadpool_task_fn func, void *arg) {
  if (scl_unlikely(!pool))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!func))
    return SCL_ERR_NULL_PTR;

  scl_threadpool_task_t *task = (scl_threadpool_task_t *)scl_alloc(
      pool->alloc, sizeof(scl_threadpool_task_t), _Alignof(max_align_t));
  if (!task)
    return SCL_ERR_OUT_OF_MEMORY;

  task->func = func;
  task->arg = arg;
  task->next = NULL;

  scl_mutex_lock(&pool->lock);

  if (scl_likely(pool->tail != NULL)) {
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
  if (scl_unlikely(!pool))
    return SCL_ERR_NULL_PTR;

  scl_mutex_lock(&pool->lock);
  /* Wait until both the queue is empty AND all workers are idle. */
  while (pool->queued > 0 || pool->working > 0)
    scl_cond_wait(&pool->cond, &pool->lock);
  scl_mutex_unlock(&pool->lock);

  return SCL_OK;
}

scl_error_t scl_threadpool_destroy(scl_threadpool_t *pool) {
  if (scl_unlikely(!pool))
    return SCL_ERR_NULL_PTR;

  /* Signal all workers to exit. */
  scl_mutex_lock(&pool->lock);
  pool->active = 0;
  scl_cond_broadcast(&pool->cond);
  scl_mutex_unlock(&pool->lock);

  /* Join all worker threads. */
  for (unsigned int i = 0; i < pool->thread_count; i++)
    scl_thread_join(pool->thread_handles[i], NULL);

  /* Free thread handle array. */
  scl_free(pool->alloc, pool->thread_handles);
  pool->thread_handles = NULL;

  /* Drain any remaining tasks (safety net; should be empty). */
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
