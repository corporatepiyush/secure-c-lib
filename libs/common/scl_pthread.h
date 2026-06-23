#ifndef SCL_PTHREAD_H
#define SCL_PTHREAD_H

#include "scl_common.h"

/*
 * scl_pthread -- portable POSIX-threads proxy adapter.
 *
 * Design decisions and portability research (details in CHANGELOG):
 *
 * 1.  RATIONALE for choosing POSIX threads over C11 <threads.h>:
 *     - C11 threads are optional (__STDC_NO_THREADS__ may be defined).
 *     - MSVC only added <threads.h> in VS 2022 17.8.
 *     - MinGW-w64 does NOT define __STDC_NO_THREADS__ even when
 *       <threads.h> is absent (confirmed via mingw-w64-public, 2023).
 *     - POSIX threads are available on every POSIX target (Linux,
 *       macOS, BSD, Solaris) and on Windows via MinGW-w64's winpthreads
 *       or pthreads-w32.
 *     - C11 threads specification is underspecified (J. Gustedt, 2012)
 *       and cannot be faithfully implemented on top of POSIX threads.
 *     - POSIX.1-2024 mandates <threads.h> but recommends pthreads for
 *       new code because of richer functionality (error-checking mutexes,
 *       rwlocks, robust mutexes, cancellation, cleanup handlers).
 *
 * 2.  TYPE SAFETY: every raw pthread type has a corresponding
 *     scl_typedef that we reuse rather than opaque-pointer wrap,
 *     because a 1-to-1 typedef is zero-cost at both compile and run
 *     time and retains full ABI compatibility.
 *
 * 3.  RETURN VALUE TRANSLATION: raw pthread functions return errno
 *     values (positive on error, 0 on success).  scl_pthread wrappers
 *     translate these to scl_error_t so that all library code speaks
 *     one error language.  Null-pointer preconditions are checked
 *     explicitly (scl_unlikely) and return SCL_ERR_NULL_PTR.
 *
 * 4.  PERFORMANCE: all wrappers are static inline; the function-call
 *     overhead is negligible compared with the underlying OS syscall.
 *     The compiler will inline these at -O1.
 *
 * 5.  WINDOWS PORT (future): when SCL_OS_WINDOWS is defined, this
 *     header could be replaced with a winpthreads shim or a Win32
 *     thread-pool backend without changing callers.
 */

#include <pthread.h>

/* ── Type aliases ────────────────────────────────────────────── */
typedef pthread_t          scl_pthread_t;
typedef pthread_attr_t     scl_pthread_attr_t;
typedef pthread_mutex_t    scl_mutex_t;
typedef pthread_mutexattr_t scl_mutexattr_t;
typedef pthread_cond_t     scl_cond_t;
typedef pthread_condattr_t scl_condattr_t;

/* ── Thread creation / join / exit ──────────────────────────── */
static inline scl_error_t scl_pthread_create(scl_pthread_t *thread,
                                              const scl_pthread_attr_t *attr,
                                              void *(*start_routine)(void *),
                                              void *arg)
{
    if (scl_unlikely(!thread || !start_routine)) return SCL_ERR_NULL_PTR;
    int ret = pthread_create(thread, attr, start_routine, arg);
    if (ret == 0) return SCL_OK;
    if (ret == EAGAIN) return SCL_ERR_ALLOC;    /* resource limit */
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    if (ret == EPERM)  return SCL_ERR_UNSUPPORTED;
    return SCL_ERR_ALLOC;
}

static inline scl_error_t scl_pthread_join(scl_pthread_t thread, void **retval) {
    int ret = pthread_join(thread, retval);
    if (ret == 0) return SCL_OK;
    if (ret == EDEADLK) return SCL_ERR_INVALID_STATE;
    if (ret == EINVAL)  return SCL_ERR_INVALID_ARG;
    if (ret == ESRCH)   return SCL_ERR_NOT_FOUND;
    return SCL_ERR_INVALID_ARG;
}

static inline void scl_pthread_exit(void *retval) {
    pthread_exit(retval);
}

/* ── Mutex ───────────────────────────────────────────────────── */
static inline scl_error_t scl_mutex_init(scl_mutex_t *mutex, const scl_mutexattr_t *attr) {
    if (scl_unlikely(!mutex)) return SCL_ERR_NULL_PTR;
    int ret = pthread_mutex_init(mutex, attr);
    if (ret == 0) return SCL_OK;
    if (ret == ENOMEM)  return SCL_ERR_OUT_OF_MEMORY;
    if (ret == EAGAIN)  return SCL_ERR_ALLOC;
    if (ret == EINVAL)  return SCL_ERR_INVALID_ARG;
    return SCL_ERR_ALLOC;
}

static inline scl_error_t scl_mutex_destroy(scl_mutex_t *mutex) {
    if (scl_unlikely(!mutex)) return SCL_ERR_NULL_PTR;
    int ret = pthread_mutex_destroy(mutex);
    if (ret == 0) return SCL_OK;
    if (ret == EBUSY)  return SCL_ERR_INVALID_STATE;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_mutex_lock(scl_mutex_t *mutex) {
    if (scl_unlikely(!mutex)) return SCL_ERR_NULL_PTR;
    int ret = pthread_mutex_lock(mutex);
    if (ret == 0) return SCL_OK;
    if (ret == EDEADLK)     return SCL_ERR_LOCK;
    if (ret == EINVAL)      return SCL_ERR_INVALID_ARG;
    return SCL_ERR_LOCK;
}

static inline scl_error_t scl_mutex_trylock(scl_mutex_t *mutex) {
    if (scl_unlikely(!mutex)) return SCL_ERR_NULL_PTR;
    int ret = pthread_mutex_trylock(mutex);
    if (ret == 0) return SCL_OK;
    if (ret == EBUSY)  return SCL_ERR_LOCK;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    return SCL_ERR_LOCK;
}

static inline scl_error_t scl_mutex_unlock(scl_mutex_t *mutex) {
    if (scl_unlikely(!mutex)) return SCL_ERR_NULL_PTR;
    int ret = pthread_mutex_unlock(mutex);
    if (ret == 0) return SCL_OK;
    if (ret == EPERM)  return SCL_ERR_LOCK;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    return SCL_ERR_LOCK;
}

/* ── Condition variable ─────────────────────────────────────── */
static inline scl_error_t scl_cond_init(scl_cond_t *cond, const scl_condattr_t *attr) {
    if (scl_unlikely(!cond)) return SCL_ERR_NULL_PTR;
    int ret = pthread_cond_init(cond, attr);
    if (ret == 0) return SCL_OK;
    if (ret == ENOMEM)  return SCL_ERR_OUT_OF_MEMORY;
    if (ret == EAGAIN)  return SCL_ERR_ALLOC;
    if (ret == EINVAL)  return SCL_ERR_INVALID_ARG;
    return SCL_ERR_ALLOC;
}

static inline scl_error_t scl_cond_destroy(scl_cond_t *cond) {
    if (scl_unlikely(!cond)) return SCL_ERR_NULL_PTR;
    int ret = pthread_cond_destroy(cond);
    if (ret == 0) return SCL_OK;
    if (ret == EBUSY)  return SCL_ERR_INVALID_STATE;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_cond_wait(scl_cond_t *cond, scl_mutex_t *mutex) {
    if (scl_unlikely(!cond || !mutex)) return SCL_ERR_NULL_PTR;
    int ret = pthread_cond_wait(cond, mutex);
    if (ret == 0) return SCL_OK;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    if (ret == EPERM)  return SCL_ERR_LOCK;
    return SCL_ERR_LOCK;
}

static inline scl_error_t scl_cond_signal(scl_cond_t *cond) {
    if (scl_unlikely(!cond)) return SCL_ERR_NULL_PTR;
    int ret = pthread_cond_signal(cond);
    if (ret == 0) return SCL_OK;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    return SCL_ERR_INVALID_ARG;
}

static inline scl_error_t scl_cond_broadcast(scl_cond_t *cond) {
    if (scl_unlikely(!cond)) return SCL_ERR_NULL_PTR;
    int ret = pthread_cond_broadcast(cond);
    if (ret == 0) return SCL_OK;
    if (ret == EINVAL) return SCL_ERR_INVALID_ARG;
    return SCL_ERR_INVALID_ARG;
}

#endif /* SCL_PTHREAD_H */
