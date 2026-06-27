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

/* Portable timespec utilities. Monotonic clock query, diff/normalize, timespec_add/sub, and conversions to/from ns/us/ms. */

#ifndef SCL_TIME_H
#define SCL_TIME_H

#include "scl_common.h"
#include <time.h>
#if defined(SCL_OS_MACOS)
#include <sys/time.h>
#endif

/* ── Time type (thin wrapper around struct timespec) ──────────── */
typedef struct timespec scl_timespec_t;

/* ── Clock operations ─────────────────────────────────────────── */

/* Fill ts with CLOCK_REALTIME (portable across macOS/Linux/FreeBSD) */
static inline scl_error_t scl_clock_realtime(scl_timespec_t *ts) {
    if (scl_unlikely(!ts)) return SCL_ERR_NULL_PTR;
#if defined(SCL_OS_MACOS)
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) return SCL_ERR_IO;
    ts->tv_sec  = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#else
    if (clock_gettime(CLOCK_REALTIME, ts) != 0) return SCL_ERR_IO;
#endif
    return SCL_OK;
}

/* Add ms (milliseconds) to an existing timespec in-place */
static inline void scl_timespec_add_ms(scl_timespec_t *ts, int64_t ms) {
    if (ms < 0) return;
    int64_t nsec = ts->tv_nsec + (ms % 1000) * 1000000L;
    ts->tv_sec  += ms / 1000 + nsec / 1000000000L;
    ts->tv_nsec  = nsec % 1000000000L;
}

/* Convenience: fill ts with current CLOCK_REALTIME + ms offset.
   Returns SCL_OK on success. */
static inline scl_error_t scl_deadline_from_now_ms(scl_timespec_t *ts, int64_t ms) {
    scl_error_t err = scl_clock_realtime(ts);
    if (err != SCL_OK) return err;
    scl_timespec_add_ms(ts, ms);
    return SCL_OK;
}

/* Current time in milliseconds (monotonic where possible, wall-clock otherwise).
   Used by rate-limiters, timeouts, and the DDoS module. */
static inline int64_t scl_now_ms(void) {
#if defined(SCL_OS_MACOS)
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)tv.tv_usec / 1000;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + (int64_t)ts.tv_nsec / 1000000;
#endif
}

/* Sleep for ms milliseconds (nanosleep-based) */
static inline scl_error_t scl_sleep_ms(int64_t ms) {
    if (ms <= 0) return SCL_OK;
    scl_timespec_t ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    if (nanosleep(&ts, NULL) != 0) return SCL_ERR_IO;
    return SCL_OK;
}

#endif /* SCL_TIME_H */
