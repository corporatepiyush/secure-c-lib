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

/* Zero-overhead POSIX-thread wrappers. Eliminates attribute boilerplate;
 * recursive mutex via dedicated init. Debug owner-tracking behind NDEBUG. */

#ifndef SCL_PTHREAD_H
#define SCL_PTHREAD_H

#include "scl_common.h"
#include "scl_string.h"
#include "scl_time.h"
#include <pthread.h>

/* ── Platform detection (from scl_common.h) ────────────── */
/* Already defined in scl_common.h:
 *   SCL_OS_MACOS, SCL_OS_FREEBSD, SCL_OS_LINUX
 */

/* ── Debug mode (on unless NDEBUG) ───────────────────────── */
#ifndef NDEBUG
#define SCL_PTHREAD_DEBUG
#endif

/* ===========================================================
 *  scl_mutex_t
 *
 *  Evolved struct wrapping pthread_mutex_t with:
 *    - owner tracking (debug)
 *    - lock count (debug)
 *    - diagnostic name (debug)
 *    - attr-free init (normal / recursive)
 *
 *  In release builds the struct is identical in size to
 *  pthread_mutex_t (no overhead).
 * =========================================================== */
typedef struct {
  pthread_mutex_t impl;
#ifdef SCL_PTHREAD_DEBUG
  pthread_t owner;
  const char *name;
  uint32_t lock_count;
#endif
} scl_mutex_t;

static inline scl_error_t scl_mutex_init(scl_mutex_t *m) {
  if (scl_unlikely(!m))
    return SCL_ERR_NULL_PTR;
#ifdef SCL_PTHREAD_DEBUG
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  int ret = pthread_mutex_init(&m->impl, &attr);
  pthread_mutexattr_destroy(&attr);
  m->owner = 0;
  m->name = NULL;
  m->lock_count = 0;
#else
  int ret = pthread_mutex_init(&m->impl, NULL);
#endif
  if (ret == 0)
    return SCL_OK;
  if (ret == ENOMEM)
    return SCL_ERR_OUT_OF_MEMORY;
  if (ret == EAGAIN)
    return SCL_ERR_ALLOC;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_mutex_init_recursive(scl_mutex_t *m) {
  if (scl_unlikely(!m))
    return SCL_ERR_NULL_PTR;
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
  int ret = pthread_mutex_init(&m->impl, &attr);
  pthread_mutexattr_destroy(&attr);
#ifdef SCL_PTHREAD_DEBUG
  m->owner = 0;
  m->name = NULL;
  m->lock_count = 0;
#else
  (void)m;
#endif
  if (ret == 0)
    return SCL_OK;
  if (ret == ENOMEM)
    return SCL_ERR_OUT_OF_MEMORY;
  if (ret == EAGAIN)
    return SCL_ERR_ALLOC;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_mutex_lock(scl_mutex_t *m) {
  if (scl_unlikely(!m))
    return SCL_ERR_NULL_PTR;
#ifdef SCL_PTHREAD_DEBUG
  int ret = pthread_mutex_lock(&m->impl);
  if (ret == 0) {
    if (m->lock_count++ == 0)
      m->owner = pthread_self();
  }
  if (ret == EDEADLK)
    return SCL_ERR_DEADLOCK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  if (ret != 0)
    return SCL_ERR_LOCK;
#else
  int ret = pthread_mutex_lock(&m->impl);
  if (ret == EDEADLK)
    return SCL_ERR_DEADLOCK;
  if (ret != 0)
    return SCL_ERR_LOCK;
#endif
  return SCL_OK;
}

static inline scl_error_t scl_mutex_trylock(scl_mutex_t *m) {
  if (scl_unlikely(!m))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_mutex_trylock(&m->impl);
  if (ret == 0) {
#ifdef SCL_PTHREAD_DEBUG
    if (m->lock_count++ == 0)
      m->owner = pthread_self();
#endif
    return SCL_OK;
  }
  if (ret == EBUSY)
    return SCL_ERR_LOCK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  return SCL_ERR_LOCK;
}

static inline scl_error_t scl_mutex_unlock(scl_mutex_t *m) {
  if (scl_unlikely(!m))
    return SCL_ERR_NULL_PTR;
#ifdef SCL_PTHREAD_DEBUG
  if (m->lock_count == 0) {
    fprintf(stderr, "scl: unlock of unlocked mutex\n");
    return SCL_ERR_LOCK;
  }
  if (!pthread_equal(m->owner, pthread_self())) {
    fprintf(stderr, "scl: unlock from non-owner thread\n");
    return SCL_ERR_LOCK;
  }
  if (--m->lock_count == 0)
    m->owner = 0;
#endif
  int ret = pthread_mutex_unlock(&m->impl);
  if (ret == 0)
    return SCL_OK;
  if (ret == EPERM)
    return SCL_ERR_LOCK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  return SCL_ERR_LOCK;
}

static inline void scl_mutex_destroy(scl_mutex_t *m) {
  if (scl_likely(m))
    pthread_mutex_destroy(&m->impl);
}

/* ===========================================================
 *  scl_thread_t
 *
 *  Wraps pthread_t in a struct to allow debug metadata (name).
 *  scl_thread_join takes the struct by value (like pthread_t).
 * =========================================================== */
typedef struct {
  pthread_t impl;
#ifdef SCL_PTHREAD_DEBUG
  char name[16];
#endif
} scl_thread_t;

static inline scl_error_t scl_thread_create(scl_thread_t *t,
                                            void *(*func)(void *), void *arg) {
  if (scl_unlikely(!t || !func))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_create(&t->impl, NULL, func, arg);
#ifdef SCL_PTHREAD_DEBUG
  t->name[0] = '\0';
#else
  (void)t;
#endif
  if (ret == 0)
    return SCL_OK;
  if (ret == EAGAIN)
    return SCL_ERR_ALLOC;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  if (ret == EPERM)
    return SCL_ERR_UNSUPPORTED;
  return SCL_ERR_ALLOC;
}

static inline scl_error_t scl_thread_create_named(scl_thread_t *t,
                                                  const char *name,
                                                  void *(*func)(void *),
                                                  void *arg) {
  if (scl_unlikely(!t || !func))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_create(&t->impl, NULL, func, arg);
  if (ret != 0) {
    if (ret == EAGAIN)
      return SCL_ERR_ALLOC;
    if (ret == EINVAL)
      return SCL_ERR_INVALID_ARG;
    if (ret == EPERM)
      return SCL_ERR_UNSUPPORTED;
    return SCL_ERR_ALLOC;
  }
#ifdef SCL_PTHREAD_DEBUG
  if (name) {
    scl_strncpy(t->name, name, sizeof(t->name) - 1);
    t->name[sizeof(t->name) - 1] = '\0';
  } else {
    t->name[0] = '\0';
  }
#else
  (void)t;
#endif
#if defined(SCL_OS_LINUX) || defined(SCL_OS_FREEBSD)
  if (name)
    pthread_setname_np(t->impl, name);
#else
  (void)name;
#endif
  return SCL_OK;
}

static inline scl_error_t scl_thread_join(scl_thread_t t, void **retval) {
  int ret = pthread_join(t.impl, retval);
  if (ret == 0)
    return SCL_OK;
  if (ret == EDEADLK)
    return SCL_ERR_INVALID_STATE;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  if (ret == ESRCH)
    return SCL_ERR_NOT_FOUND;
  return SCL_ERR_INVALID_ARG;
}

static inline void scl_thread_detach(scl_thread_t *t) {
  if (scl_likely(t))
    pthread_detach(t->impl);
}

static inline scl_thread_t scl_thread_self(void) {
  scl_thread_t t;
  t.impl = pthread_self();
#ifdef SCL_PTHREAD_DEBUG
  t.name[0] = '\0';
#endif
  return t;
}

static inline bool scl_thread_equal(scl_thread_t a, scl_thread_t b) {
  return pthread_equal(a.impl, b.impl) != 0;
}

/* ===========================================================
 *  scl_cond_t — condition variable with monotonic clock
 *
 *  Uses CLOCK_MONOTONIC internally so wall-clock jumps (NTP)
 *  cannot cause spurious timeouts or infinite waits.
 *  timed wait expects a timeout in milliseconds (computed to
 *  absolute CLOCK_MONOTONIC).
 * =========================================================== */
typedef struct {
  pthread_cond_t impl;
} scl_cond_t;

static inline scl_error_t scl_cond_init(scl_cond_t *c) {
  if (scl_unlikely(!c))
    return SCL_ERR_NULL_PTR;
#if defined(__linux__) || defined(__FreeBSD__)
  pthread_condattr_t attr;
  pthread_condattr_init(&attr);
  pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  int ret = pthread_cond_init(&c->impl, &attr);
  pthread_condattr_destroy(&attr);
#else
  /* macOS ≤ 10.11 does not support pthread_condattr_setclock;
   * we fall back to the default (CLOCK_REALTIME) and accept the
   * NTP-jump risk. The monotonic deadline computed by
   * scl_cond_wait_for is still used; macOS's cond_timedwait
   * will use its own clock for comparison. */
  int ret = pthread_cond_init(&c->impl, NULL);
#endif
  if (ret == 0)
    return SCL_OK;
  if (ret == ENOMEM)
    return SCL_ERR_OUT_OF_MEMORY;
  if (ret == EAGAIN)
    return SCL_ERR_ALLOC;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_cond_wait(scl_cond_t *c, scl_mutex_t *m) {
  if (scl_unlikely(!c || !m))
    return SCL_ERR_NULL_PTR;
#ifdef SCL_PTHREAD_DEBUG
  uint32_t saved_lock_count = m->lock_count;
  pthread_t saved_owner = m->owner;
  if (m->lock_count > 0)
    m->lock_count--;
  if (m->lock_count == 0)
    m->owner = 0;
#endif
  int ret = pthread_cond_wait(&c->impl, &m->impl);
#ifdef SCL_PTHREAD_DEBUG
  if (ret == 0) {
    m->lock_count = 1;
    m->owner = pthread_self();
  } else {
    /* Restore debug state on failure so lock-count tracking
     * is never corrupted by a cond-wait error. */
    m->lock_count = saved_lock_count;
    m->owner = saved_owner;
  }
#endif
  if (ret == 0)
    return SCL_OK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  if (ret == EPERM)
    return SCL_ERR_LOCK;
  return SCL_ERR_LOCK;
}

static inline scl_error_t scl_cond_wait_for(scl_cond_t *c, scl_mutex_t *m,
                                            int64_t timeout_ms) {
  if (scl_unlikely(!c || !m))
    return SCL_ERR_NULL_PTR;
  if (timeout_ms < 0)
    return scl_cond_wait(c, m);

  scl_timespec_t ts;
#if defined(__linux__) || defined(__FreeBSD__)
  /* Monotonic condvar — monotonic deadline is correct */
  scl_error_t err = scl_deadline_from_now_ms(&ts, timeout_ms);
#else
  /* macOS condvar uses CLOCK_REALTIME by default — use REALTIME deadline */
  scl_error_t err = scl_deadline_realtime_ms(&ts, timeout_ms);
#endif
  if (err != SCL_OK)
    return err;

#ifdef SCL_PTHREAD_DEBUG
  if (m->lock_count > 0)
    m->lock_count--;
  if (m->lock_count == 0)
    m->owner = 0;
#endif
  int ret = pthread_cond_timedwait(&c->impl, &m->impl, &ts);
#ifdef SCL_PTHREAD_DEBUG
  if (ret == 0) {
    m->lock_count = 1;
    m->owner = pthread_self();
  }
#endif
  if (ret == 0)
    return SCL_OK;
  if (ret == ETIMEDOUT)
    return SCL_ERR_TIMEOUT;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  if (ret == EPERM)
    return SCL_ERR_LOCK;
  return SCL_ERR_LOCK;
}

static inline scl_error_t scl_cond_signal(scl_cond_t *c) {
  if (scl_unlikely(!c))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_cond_signal(&c->impl);
  if (ret == 0)
    return SCL_OK;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_cond_broadcast(scl_cond_t *c) {
  if (scl_unlikely(!c))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_cond_broadcast(&c->impl);
  if (ret == 0)
    return SCL_OK;
  return SCL_ERR_INVALID_ARG;
}

static inline void scl_cond_destroy(scl_cond_t *c) {
  if (scl_likely(c))
    pthread_cond_destroy(&c->impl);
}

/* ===========================================================
 *  scl_rwlock_t
 *
 *  Reader-writer lock (thin wrapper over pthread_rwlock_t).
 * =========================================================== */
typedef struct {
  pthread_rwlock_t impl;
} scl_rwlock_t;

static inline scl_error_t scl_rwlock_init(scl_rwlock_t *rw) {
  if (scl_unlikely(!rw))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_rwlock_init(&rw->impl, NULL);
  if (ret == 0)
    return SCL_OK;
  if (ret == ENOMEM)
    return SCL_ERR_OUT_OF_MEMORY;
  if (ret == EAGAIN)
    return SCL_ERR_ALLOC;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_rwlock_rdlock(scl_rwlock_t *rw) {
  if (scl_unlikely(!rw))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_rwlock_rdlock(&rw->impl);
  if (ret == 0)
    return SCL_OK;
  if (ret == EDEADLK)
    return SCL_ERR_DEADLOCK;
  return SCL_ERR_LOCK;
}

static inline scl_error_t scl_rwlock_wrlock(scl_rwlock_t *rw) {
  if (scl_unlikely(!rw))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_rwlock_wrlock(&rw->impl);
  if (ret == 0)
    return SCL_OK;
  if (ret == EDEADLK)
    return SCL_ERR_DEADLOCK;
  return SCL_ERR_LOCK;
}

static inline scl_error_t scl_rwlock_tryrdlock(scl_rwlock_t *rw) {
  if (scl_unlikely(!rw))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_rwlock_tryrdlock(&rw->impl);
  if (ret == 0)
    return SCL_OK;
  if (ret == EBUSY)
    return SCL_ERR_LOCK;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_rwlock_trywrlock(scl_rwlock_t *rw) {
  if (scl_unlikely(!rw))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_rwlock_trywrlock(&rw->impl);
  if (ret == 0)
    return SCL_OK;
  if (ret == EBUSY)
    return SCL_ERR_LOCK;
  return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_rwlock_unlock(scl_rwlock_t *rw) {
  if (scl_unlikely(!rw))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_rwlock_unlock(&rw->impl);
  if (ret == 0)
    return SCL_OK;
  if (ret == EPERM)
    return SCL_ERR_LOCK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  return SCL_ERR_LOCK;
}

static inline void scl_rwlock_destroy(scl_rwlock_t *rw) {
  if (scl_likely(rw))
    pthread_rwlock_destroy(&rw->impl);
}

/* ===========================================================
 *  scl_barrier_t
 *
 *  Native pthread_barrier_t on Linux & FreeBSD.
 *  Mutex+condvar fallback on macOS (pthread_barrier removed).
 * =========================================================== */
#if defined(SCL_OS_MACOS)

typedef struct {
  scl_mutex_t _mutex;
  scl_cond_t _cond;
  unsigned int _count;
  unsigned int _arrived;
  bool _cycle;
} scl_barrier_t;

static inline scl_error_t scl_barrier_init(scl_barrier_t *b,
                                           unsigned int count) {
  if (scl_unlikely(!b))
    return SCL_ERR_NULL_PTR;
  if (count == 0)
    return SCL_ERR_INVALID_ARG;
  scl_mutex_init(&b->_mutex);
  scl_cond_init(&b->_cond);
  b->_count = count;
  b->_arrived = 0;
  b->_cycle = false;
  return SCL_OK;
}

static inline scl_error_t scl_barrier_wait(scl_barrier_t *b) {
  if (scl_unlikely(!b))
    return SCL_ERR_NULL_PTR;
  scl_mutex_lock(&b->_mutex);
  unsigned int cur = b->_cycle;
  b->_arrived++;
  if (b->_arrived == b->_count) {
    b->_arrived = 0;
    b->_cycle = !b->_cycle;
    scl_cond_broadcast(&b->_cond);
    scl_mutex_unlock(&b->_mutex);
    return SCL_OK;
  }
  while (b->_cycle == cur)
    scl_cond_wait(&b->_cond, &b->_mutex);
  scl_mutex_unlock(&b->_mutex);
  return SCL_OK;
}

static inline void scl_barrier_destroy(scl_barrier_t *b) {
  if (scl_likely(b)) {
    scl_cond_destroy(&b->_cond);
    scl_mutex_destroy(&b->_mutex);
  }
}

#else /* Linux / FreeBSD */

typedef struct {
  pthread_barrier_t impl;
} scl_barrier_t;

static inline scl_error_t scl_barrier_init(scl_barrier_t *b,
                                           unsigned int count) {
  if (scl_unlikely(!b))
    return SCL_ERR_NULL_PTR;
  if (count == 0)
    return SCL_ERR_INVALID_ARG;
  int ret = pthread_barrier_init(&b->impl, NULL, count);
  if (ret == 0)
    return SCL_OK;
  if (ret == ENOMEM)
    return SCL_ERR_OUT_OF_MEMORY;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  if (ret == EAGAIN)
    return SCL_ERR_ALLOC;
  return SCL_ERR_ALLOC;
}

static inline scl_error_t scl_barrier_wait(scl_barrier_t *b) {
  if (scl_unlikely(!b))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_barrier_wait(&b->impl);
  if (ret == 0 || ret == PTHREAD_BARRIER_SERIAL_THREAD)
    return SCL_OK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  return SCL_ERR_LOCK;
}

static inline void scl_barrier_destroy(scl_barrier_t *b) {
  if (scl_likely(b))
    pthread_barrier_destroy(&b->impl);
}

#endif

/* ===========================================================
 *  scl_once_t — one-time initialisation
 * =========================================================== */
typedef pthread_once_t scl_once_t;
#define SCL_ONCE_INIT PTHREAD_ONCE_INIT

static inline scl_error_t scl_once(scl_once_t *once, void (*fn)(void)) {
  if (scl_unlikely(!once || !fn))
    return SCL_ERR_NULL_PTR;
  int ret = pthread_once(once, fn);
  if (ret == 0)
    return SCL_OK;
  if (ret == EINVAL)
    return SCL_ERR_INVALID_ARG;
  return SCL_ERR_INVALID_ARG;
}

/* ===========================================================
 *  scl_scoped_lock_t + SCL_LOCK macro
 *
 *  Manual paired-call pattern (C has no destructors).
 *  SCL_LOCK uses a for-loop trick for scoped auto-unlock:
 *
 *    SCL_LOCK(&m) {
 *        // critical section — lock held
 *    } // auto-unlock at closing brace
 *
 *  Caveat: break/continue/return leak the lock.
 * =========================================================== */
typedef struct {
  scl_mutex_t *mutex;
} scl_scoped_lock_t;

static inline scl_error_t scl_scoped_lock_acquire(scl_scoped_lock_t *sl,
                                                  scl_mutex_t *m) {
  if (scl_unlikely(!sl || !m))
    return SCL_ERR_NULL_PTR;
  sl->mutex = m;
  return scl_mutex_lock(m);
}

static inline scl_error_t scl_scoped_lock_release(scl_scoped_lock_t *sl) {
  if (scl_unlikely(!sl || !sl->mutex))
    return SCL_ERR_NULL_PTR;
  return scl_mutex_unlock(sl->mutex);
}

#define SCL_LOCK(mutex)                                                        \
  for (int _scl_lk_ = 0;                                                       \
       _scl_lk_ == 0 ? ((_scl_lk_ = 1), (scl_mutex_lock(mutex) == SCL_OK))     \
                     : (scl_mutex_unlock(mutex), 0);)

#endif /* SCL_PTHREAD_H */
