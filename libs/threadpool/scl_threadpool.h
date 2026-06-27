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

/* Fixed-size worker thread pool (max 128). Mutex+condvar dispatch, scl_threadpool_submit with optional scl_future_t for synchronous result retrieval. */

#ifndef SCL_THREADPOOL_H
#define SCL_THREADPOOL_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "scl_atomic.h"
#include "scl_pthread.h"

/*
 * scl_threadpool.h — fixed-size worker thread pool.
 *
 * Queues tasks (function + void *arg) and executes them in FIFO order
 * across a fixed set of worker threads.  Designed for use in the HTTP
 * server's connection handler path — tasks are short-lived and the
 * pool lives for the lifetime of the server.
 *
 * KEY PROPERTIES
 * ──────────────
 *   - Fixed thread count (set at init); no dynamic thread creation.
 *   - Bounded thread count — enforce an upper limit to prevent
 *     accidental resource exhaustion (# cores * 2 or 64, whichever
 *     is smaller, is a reasonable default; we cap at 128).
 *   - Lock-free task submission: only the enqueue/dispatch hot path
 *     takes the mutex; worker threads block on the condition variable.
 *   - Wait/drain: scl_threadpool_wait blocks until the queue is empty
 *     AND all in-flight tasks complete.
 *   - Safe shutdown: scl_threadpool_destroy signals all workers,
 *     joins them, then drains any remaining queued tasks.
 *   - Allocator-aware: all internal allocations (thread handle array,
 *     task nodes) go through the caller-supplied scl_allocator_t.
 */

/* Maximum sensible thread count; avoids resource exhaustion. */
#define SCL_THREADPOOL_MAX_THREADS 128u

typedef void (*scl_threadpool_task_fn)(void *arg);

typedef struct scl_threadpool_task {
    scl_threadpool_task_fn func;
    void *arg;
    struct scl_threadpool_task *next;
} scl_threadpool_task_t;

typedef struct {
    scl_allocator_t *alloc;
    unsigned int thread_count;
    scl_atomic_int active;
    scl_threadpool_task_t *head;
    scl_threadpool_task_t *tail;
    scl_thread_t *thread_handles;
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
