# Pthread Design — `scl_pthread.h`

## 1. Motivation

Raw POSIX threads (pthreads) are powerful but error‑prone:
- Every function returns a raw errno — nothing distinguishes "lock held by
  another thread" from "invalid mutex pointer".
- `pthread_mutex_init` requires a mostly‑NULL `pthread_mutexattr_t *` for
  90 % of use cases — boilerplate that adds zero safety.
- No debug aids: you cannot ask a mutex "who holds me?" or "how many times
  have I been locked?" without custom wrappers.
- No scope‑based locking in C (the language has no destructors).
- Platform differences are exposed directly: `pthread_barrier_t` is gone
  on macOS 10.15+, `pthread_setname_np` has 3 different signatures,
  condition‑variable clock sources differ.

`scl_pthread` is not a "redesign pthreads for fun".  It is a thin,
value‑adding proxy that eliminates boilerplate, catches bugs in debug
builds, and hides platform differences — **without** adding runtime cost
in release builds.

---

## 2. Design Principles

1. **Zero overhead in release** — every wrapper is `static inline`.
   The optimizer sees through the abstraction at `-O1`.
2. **Error translation** — raw errno values are mapped to `scl_error_t`
   so that all library code speaks one error language.
3. **NULL‑pointer guards** — every wrapper checks its mandatory pointer
   arguments and returns `SCL_ERR_NULL_PTR`.
4. **Debug metadata** — when `NDEBUG` is **not** defined, mutexes track
   their current owner thread and lock count, unlocking from the wrong
   thread is caught immediately.  Zero cost when `NDEBUG` is defined.
5. **No `attr` parameters** — special mutex behaviour (recursive,
   error‑check) is selected via a dedicated init function, not via a
   separate attr object.
6. **Platform abstraction** — thread naming, barriers, and timed waits
   work identically on Linux, macOS, and FreeBSD despite wildly different
   underlying implementations.
7. **C‑first** — no C++ features, no templates, no RAII (but a scope‑lock
   helper is provided for manual paired‑call convenience).

---

## 3. Platform Differences

| Feature | Linux (glibc) | macOS (XNU) | FreeBSD (libthr) |
|---------|---------------|-------------|-------------------|
| **`pthread_t` type** | `unsigned long` | `struct _opaque_pthread_t *` (pointer) | `uintptr_t` (integer) |
| **`pthread_barrier_t`** | Available | **Removed in 10.15+** | Available |
| **`pthread_setname_np`** | `setname_np(thread, name)` | `setname_np(name)` (self only) | `setname_np(thread, name)` |
| **`pthread_getname_np`** | Available | **Not available** | Available (≥ 11.0) |
| **`pthread_condattr_setclock`** | Available (CLOCK_MONOTONIC) | **Not available** (uses mach clock) | Available |
| **Robust mutexes** | Available | **Not available** | Available |
| **Priority inheritance** | Available | **Not available** | Available |
| **`PTHREAD_MUTEX_ADAPTIVE_NP`** | Available | **Not available** | Not available |
| **`clock_gettime`** | Always | ≥ 10.12 (use `gettimeofday` fallback) | Always |

### Workarounds employed

- **Barrier on macOS**: implemented with `scl_mutex` + `scl_cond` + counter.
  See `scl_barrier_t` definition below.
- **Thread naming**: abstracted via `scl_thread_create_named`.  On macOS
  the name is stored in the struct and applied from within the thread
  start routine (since the macOS API cannot name another thread).  On
  Linux and FreeBSD the name is applied immediately via
  `pthread_setname_np(thread, name)`.
- **`pthread_t` comparison**: always uses `pthread_equal()`, never `==`,
  because macOS uses a pointer type and equality of distinct
  `_opaque_pthread_t` objects is not guaranteed even for the same thread.
- **Timed waits**: use `clock_gettime(CLOCK_REALTIME, ...)` on all
  platforms to compute absolute deadlines.  macOS ignores `CLOCK_MONOTONIC`
  for `pthread_cond_timedwait` anyway (it uses `mach_absolute_time`), so
  `CLOCK_REALTIME` is the safe portable choice.  On pre‑10.12 macOS a
  `gettimeofday` fallback is used.
- **Debug mode**: `SCL_PTHREAD_DEBUG` is defined when `NDEBUG` is not
  defined, enabling ownership tracking, lock‑count recording, and
  diagnostic assertion checks.  This has **zero** impact on release
  builds (the fields are hidden behind `#ifdef`).

---

## 4. Type Hierarchy

```
                        ┌──────────────┐
                        │  scl_mutex_t  │  struct with .impl (pthread_mutex_t)
                        │              │  + debug: owner, name, lock_count
                        └──────────────┘
                              │
                    ┌─────────┴──────────┐
                    │                    │
            ┌───────▼──────┐    ┌────────▼──────────┐
            │scl_scoped_lock│    │ scl_cond_t         │
            │_t            │    │ .impl = pthread_   │
            │(acquire/     │    │        cond_t       │
            │ release)     │    └───────────────────┘
            └──────────────┘              │
                                   ┌──────▼──────┐
                                   │scl_barrier_t │
                                   │native on     │
                                   │Linux/FreeBSD;│
                                   │mutex+cond    │
                                   │fallback on   │
                                   │macOS         │
                                   └─────────────┘

┌──────────────┐     ┌────────────────┐     ┌───────────────────┐
│ scl_thread_t  │     │ scl_rwlock_t   │     │ scl_once_t        │
│ .impl =       │     │ .impl =        │     │ typedef for       │
│  pthread_t    │     │ pthread_rwlock │     │ pthread_once_t    │
│ + name[16]    │     │        _t      │     │ + SCL_ONCE_INIT   │
│ (debug)       │     └────────────────┘     └───────────────────┘
└──────────────┘
```

Every type is a struct (not a bare typedef) so that debug fields can be
added without changing the public API.  On releases builds (`NDEBUG`)
the debug fields vanish and each struct contains only its `impl` member,
making it identical in size to the raw pthread type.

---

## 5. API Overview

### 5.1 Mutex

```c
scl_error_t scl_mutex_init(scl_mutex_t *m);
scl_error_t scl_mutex_init_recursive(scl_mutex_t *m);

scl_error_t scl_mutex_lock(scl_mutex_t *m);
scl_error_t scl_mutex_trylock(scl_mutex_t *m);
scl_error_t scl_mutex_unlock(scl_mutex_t *m);
void        scl_mutex_destroy(scl_mutex_t *m);
```

**Key decisions:**
- `pthread_mutexattr_t` is eliminated from the public API.  The two most
  common mutex types (normal, recursive) get dedicated init functions.
- `scl_mutex_init_recursive` sets the `PTHREAD_MUTEX_RECURSIVE` type.
- In debug builds, `lock` stores `pthread_self()` into `.owner` and
  increments `.lock_count`; `unlock` tests that `.owner == self` and
  decrements `.lock_count`, aborting on mismatch (double‑unlock or
  foreign‑thread unlock).
- `destroy` returns `void` — there is no useful recovery from a failed
  `pthread_mutex_destroy` (the only failure mode is `EBUSY`, which is a
  caller bug, not a runtime condition).
- `lock` returning `SCL_ERR_DEADLOCK` when a non‑recursive mutex would
  deadlock is only reliable on systems with `PTHREAD_MUTEX_ERRORCHECK`
  (FreeBSD, macOS with debug build, Linux with `PTHREAD_MUTEX_ERRORCHECK`).

### 5.2 Thread

```c
scl_error_t  scl_thread_create(scl_thread_t *t,
                                void *(*func)(void *), void *arg);
scl_error_t  scl_thread_create_named(scl_thread_t *t, const char *name,
                                      void *(*func)(void *), void *arg);
scl_error_t  scl_thread_join(scl_thread_t t, void **retval);
void         scl_thread_detach(scl_thread_t *t);
scl_thread_t scl_thread_self(void);
bool         scl_thread_equal(scl_thread_t a, scl_thread_t b);
```

**Key decisions:**
- `scl_pthread_attr_t` is eliminated — the common case (default attributes)
  is the only path.  Stack size and scheduling policy are rare enough that
  they can be added via a future `scl_thread_attr_t` if needed.
- `scl_thread_create_named` sets a human‑readable name for the thread.
  On Linux and FreeBSD the name is set immediately via
  `pthread_setname_np`.  On macOS (which lacks per‑thread naming) the name
  is stored in the struct and applied from within the start routine.
- `scl_thread_join` takes the struct **by value** (like `pthread_join`)
  because the join operation only needs the `pthread_t` inside.
- `scl_thread_self` and `scl_thread_equal` mirror `pthread_self` and
  `pthread_equal` with a consistent return type.
- Debug builds store the thread name in `scl_thread_t.name[16]`.

### 5.3 Condition Variable

```c
scl_error_t scl_cond_init(scl_cond_t *c);
scl_error_t scl_cond_wait(scl_cond_t *c, scl_mutex_t *m);
scl_error_t scl_cond_wait_for(scl_cond_t *c, scl_mutex_t *m,
                               int64_t timeout_ms);
scl_error_t scl_cond_signal(scl_cond_t *c);
scl_error_t scl_cond_broadcast(scl_cond_t *c);
void        scl_cond_destroy(scl_cond_t *c);
```

**Key decisions:**
- `pthread_condattr_t` is eliminated (always default).
- `scl_cond_wait_for` adds a **timeout in milliseconds** — the most common
  timed‑wait pattern.  The absolute deadline is computed internally via
  `clock_gettime(CLOCK_REALTIME, ...)`.
- Return value is `SCL_ERR_TIMEOUT` when the timeout expires without a
  signal (mapped from `ETIMEDOUT`).
- `destroy` returns `void` (same rationale as mutex; `EBUSY` is a caller
  bug).

### 5.4 Reader‑Writer Lock

```c
scl_error_t scl_rwlock_init(scl_rwlock_t *rw);
scl_error_t scl_rwlock_rdlock(scl_rwlock_t *rw);
scl_error_t scl_rwlock_wrlock(scl_rwlock_t *rw);
scl_error_t scl_rwlock_tryrdlock(scl_rwlock_t *rw);
scl_error_t scl_rwlock_trywrlock(scl_rwlock_t *rw);
scl_error_t scl_rwlock_unlock(scl_rwlock_t *rw);
void        scl_rwlock_destroy(scl_rwlock_t *rw);
```

**Key decisions:**
- Simple thin wrapper over `pthread_rwlock_t`.  No debug metadata in the
  initial version (can be added when needed).
- No upgrade/downgrade operations (rarely used; can be added later).

### 5.5 Barrier

```c
scl_error_t scl_barrier_init(scl_barrier_t *b, unsigned int count);
scl_error_t scl_barrier_wait(scl_barrier_t *b);
void        scl_barrier_destroy(scl_barrier_t *b);
```

**Key decisions:**
- **macOS fallback**: Since `pthread_barrier_t` was removed in macOS 10.15,
  `scl_barrier_t` is implemented with `scl_mutex_t` + `scl_cond_t` + a
  counter on macOS.  On Linux and FreeBSD it uses the native
  `pthread_barrier_t` for maximum performance.
- `scl_barrier_wait` returns `SCL_OK` and `SCL_ERR_TIMEOUT` is never
  returned (barriers have no timeout).  The return value signals only
  error conditions.
- `count` must be ≥ 1; `SCL_ERR_INVALID_ARG` is returned otherwise.

### 5.6 One‑time Initialisation

```c
typedef pthread_once_t scl_once_t;
#define SCL_ONCE_INIT PTHREAD_ONCE_INIT
scl_error_t scl_once(scl_once_t *once, void (*fn)(void));
```

**Key decisions:**
- `scl_once_t` is a bare typedef to `pthread_once_t`.  This is safe
  because `pthread_once` is simple enough that a wrapper adds no value.
- The `SCL_ONCE_INIT` macro provides the portable zero‑initialiser.

### 5.7 Scoped Lock Helper

```c
typedef struct {
    scl_mutex_t *mutex;
} scl_scoped_lock_t;

static inline scl_error_t scl_scoped_lock_acquire(scl_scoped_lock_t *sl,
                                                   scl_mutex_t *m);
static inline scl_error_t scl_scoped_lock_release(scl_scoped_lock_t *sl);
```

**Key decisions:**
- C has no destructors, so true RAII is impossible.  This type provides
  a **paired‑call pattern** that makes lock/unlock coupling visible and
  reduces the risk of forgetting an unlock.
- The macro `SCL_LOCK(mutex)` uses a `for`‑loop trick to scope the lock:

```c
#define SCL_LOCK(mutex) \
    for (int _scl_lk_ = 0; \
         _scl_lk_ < 1 ? (scl_mutex_lock(mutex), 1) : 0; \
         _scl_lk_++, scl_mutex_unlock(mutex))

    // Usage:
    SCL_LOCK(&m) {
        /* critical section */
    }
```

- **Caveat**: `break`, `continue`, or early `return` inside the block
  **will not** release the lock (C has no `defer`).  Use the explicit
  `scl_scoped_lock_acquire`/`release` pattern if you need early exits.

---

## 6. Error Handling

| Error condition | Return code | Notes |
|----------------|-------------|-------|
| Pointer argument is NULL | `SCL_ERR_NULL_PTR` | Checked first in every function |
| Lock already held by another thread | `SCL_ERR_LOCK` | `pthread_mutex_lock` returns EDEADLK (error‑check mutex), EBUSY (trylock) |
| Recursive lock on non‑recursive mutex | `SCL_ERR_DEADLOCK` | Only with error‑check mutex type |
| Mutex unlock from non‑owner thread | `SCL_ERR_LOCK` | `pthread_mutex_unlock` returns EPERM |
| Condition wait timeout | `SCL_ERR_TIMEOUT` | `pthread_cond_timedwait` returns ETIMEDOUT |
| Caller violates precondition (e.g. double init, destroy while locked) | `SCL_ERR_INVALID_ARG` or `SCL_ERR_INVALID_STATE` | Runtime assertion in debug builds |
| Out of memory (rare, NPTL allocation) | `SCL_ERR_OUT_OF_MEMORY` | From `pthread_mutex_init` ENOMEM |
| Thread creation resource limit | `SCL_ERR_ALLOC` | From `pthread_create` EAGAIN |
| Joining non‑joinable or invalid thread | `SCL_ERR_NOT_FOUND` | From `pthread_join` ESRCH |
| All other system errors | `SCL_ERR_ALLOC` (catch‑all) | Fallback for unexpected errno values |

---

## 7. Debug Mode

When `NDEBUG` is **not** defined:

```c
typedef struct {
    pthread_mutex_t impl;
    pthread_t       owner;        // Current lock owner (0 if unlocked)
    const char     *name;         // Optional diagnostic label
    uint32_t        lock_count;   // Number of nested locks
} scl_mutex_t;
```

- `scl_mutex_lock` stores `pthread_self()` in `owner` and increments
  `lock_count`.  If `owner` is already `self` and the mutex is not
  recursive, it aborts (double‑lock detection).
- `scl_mutex_unlock` asserts `owner == pthread_self()` and decrements
  `lock_count`.  If `lock_count` reaches 0, `owner` is cleared.
- `scl_thread_t` gains a `char name[16]` field for thread identification.

There is **zero** cost in release builds (`NDEBUG` defined): the debug
fields are compiled away entirely.

---

## 8. Thread Safety

- `scl_mutex_t`, `scl_rwlock_t`, `scl_barrier_t`, `scl_once_t` are
  thread‑safe by construction (they delegate to pthreads).
- `scl_cond_t` is thread‑safe (must be paired with a mutex).
- `scl_thread_t` is **not** safe to access from multiple threads
  concurrently (the same as pthread_t — the thread object belongs to
  the creating thread).
- `scl_scoped_lock_t` is **not** thread‑safe (it is per‑thread
  stack‑local by design).

---

## 9. Migration Guide (raw pthread → scl_pthread)

| Raw pthread | scl_pthread |
|-------------|-------------|
| `pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;` | `scl_mutex_t m; scl_mutex_init(&m);` |
| `pthread_mutex_init(&m, NULL);` | `scl_mutex_init(&m);` |
| `pthread_mutex_init(&m, &attr); pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);` | `scl_mutex_init_recursive(&m);` |
| `pthread_mutex_lock(&m);` | `scl_mutex_lock(&m);` |
| `pthread_mutex_unlock(&m);` | `scl_mutex_unlock(&m);` |
| `pthread_mutex_destroy(&m);` | `scl_mutex_destroy(&m);` |
| `pthread_t t; pthread_create(&t, NULL, fn, arg);` | `scl_thread_t t; scl_thread_create(&t, fn, arg);` |
| `pthread_join(t, &ret);` | `scl_thread_join(&t, &ret);` |
| `pthread_self();` | `scl_thread_self();` |
| `pthread_equal(a, b);` | `scl_thread_equal(a, b);` |
| `pthread_cond_t c; pthread_cond_init(&c, NULL);` | `scl_cond_t c; scl_cond_init(&c);` |
| `pthread_cond_timedwait(&c, &m, &ts);` | `scl_cond_wait_for(&c, &m, 100);` |
| `pthread_cond_signal(&c);` | `scl_cond_signal(&c);` |
| `pthread_rwlock_t rw; pthread_rwlock_init(&rw, NULL);` | `scl_rwlock_t rw; scl_rwlock_init(&rw);` |
| `pthread_barrier_t b; pthread_barrier_init(&b, NULL, n);` | `scl_barrier_t b; scl_barrier_init(&b, n);` |
| `pthread_once_t o = PTHREAD_ONCE_INIT; pthread_once(&o, fn);` | `scl_once_t o = SCL_ONCE_INIT; scl_once(&o, fn);` |

---

## 10. Future Extensions (not yet implemented)

- **`scl_thread_attr_t`**: stack size, scheduling policy, priority, guard
  size.  Use case: high‑performance systems that need precise control over
  thread stack.
- **`scl_semaphore_t`**: wrapper over `sem_t` (POSIX semaphores).
  Available on all three targets but deprecated on macOS.
- **`scl_thread_pool_t`**: higher‑level construct that manages a set of
  worker threads and a task queue (currently lives in `libs/threadpool/`;
  may be integrated into the common API).
- **`scl_latch`** (C++20 `std::latch`): single‑use barrier that counts
  down to zero.
- **Lock ordering validation**: in debug mode, track a global lock order
  graph and detect potential deadlocks at acquisition time (like Abseil
  `Mutex`).  Requires per‑thread state and a global dependency graph.
