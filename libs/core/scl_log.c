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

/* Level-based logger (DEBUG/INFO/WARN/ERROR). Colourised terminal output,
 * optional timestamps, compiled out with -DSCL_LOG_DISABLE. Zero allocations on
 * hot path.
 *
 * Thread-safety: min_level is an atomic_int; the formatted record is built
 * into a stack buffer and emitted with a single write(2) to avoid
 * interleaving under concurrency. */

#include "scl_log.h"
#include "scl_atomic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static struct {
   scl_atomic_int min_level;
   scl_atomic_int use_color;
} scl_log_state = {
     .min_level = SCL_LOG_DEBUG,
     .use_color = 1,
 };

/* Cached timestamp state — avoids per-call time()/strftime overhead.
 * last_sec is guarded by a spinlock-style atomic flag so concurrent
 * log calls never see a torn/partially-updated timestamp. */
static struct {
   time_t last_sec;
   char buf[32]; /* "YYYY-MM-DD HH:MM:SS" */
} scl_log_ts_cache = {0, {0}};
static scl_atomic_flag scl_log_ts_lock = ATOMIC_FLAG_INIT;

static const char *scl_log_level_name(scl_log_level_t level) {
  switch (level) {
  case SCL_LOG_DEBUG:
    return "DEBUG";
  case SCL_LOG_INFO:
    return "INFO";
  case SCL_LOG_WARN:
    return "WARN";
  case SCL_LOG_ERROR:
    return "ERROR";
  default:
    return "UNKNOWN";
  }
}

static const char *scl_log_color(scl_log_level_t level) {
   if (!scl_atomic_load_int_explicit(&scl_log_state.use_color,
                                      scl_memory_order_acquire))
     return "";
  switch (level) {
  case SCL_LOG_DEBUG:
    return "\033[36m";
  case SCL_LOG_INFO:
    return "\033[32m";
  case SCL_LOG_WARN:
    return "\033[33m";
  case SCL_LOG_ERROR:
    return "\033[31m";
  default:
    return "";
  }
}

static const char *scl_log_color_reset(void) {
   return scl_atomic_load_int_explicit(&scl_log_state.use_color,
                                       scl_memory_order_acquire)
              ? "\033[0m"
              : "";
 }

void scl_log_init(scl_log_config_t config) {
   scl_atomic_store_int_explicit(&scl_log_state.min_level, config.min_level,
                                 scl_memory_order_release);
   scl_atomic_store_int_explicit(&scl_log_state.use_color, config.use_color,
                                 scl_memory_order_release);
 }

void scl_log_set_level(scl_log_level_t level) {
  scl_atomic_store_int_explicit(&scl_log_state.min_level, level,
                                scl_memory_order_release);
}

static const char *scl_log_cached_timestamp(void) {
   time_t now = time(NULL);

   /* Try lock-free fast path: if the cached second hasn't changed,
    * return the existing buffer without acquiring the flag. */
   if (scl_likely(now == scl_log_ts_cache.last_sec))
     return scl_log_ts_cache.buf;

   /* Slow path: acquire the spinlock so only one thread formats
    * the timestamp at a time. */
   while (scl_atomic_flag_test_and_set_explicit(
       &scl_log_ts_lock, scl_memory_order_acquire))
     ;

   /* Double-check after acquiring — another thread may have updated. */
   if (scl_likely(now == scl_log_ts_cache.last_sec)) {
     scl_atomic_flag_clear_explicit(&scl_log_ts_lock,
                                     scl_memory_order_release);
     return scl_log_ts_cache.buf;
   }

   scl_log_ts_cache.last_sec = now;
   struct tm tmv;
   memset(&tmv, 0, sizeof(tmv));
   localtime_r(&now, &tmv);
   strftime(scl_log_ts_cache.buf, sizeof(scl_log_ts_cache.buf),
            "%Y-%m-%d %H:%M:%S", &tmv);

   scl_atomic_flag_clear_explicit(&scl_log_ts_lock,
                                   scl_memory_order_release);
   return scl_log_ts_cache.buf;
 }

static void scl_log_vwrite(scl_log_level_t level, const char *fmt,
                            va_list args) {
if (scl_unlikely(level < (scl_log_level_t)scl_atomic_load_int_explicit(
                         &scl_log_state.min_level, scl_memory_order_acquire)))
    return;

  /* Format the entire record into a stack buffer, then emit with one
   * write(2) so lines from concurrent threads never interleave. */
  char buf[1024];
  const char *ts = scl_log_cached_timestamp();
  const char *color = scl_log_color(level);
  const char *reset = scl_log_color_reset();
  const char *lvl = scl_log_level_name(level);

  /* Build header portion first. */
  int n = snprintf(buf, sizeof(buf), "%s[%s] %s: %s", color, ts, lvl, reset);
  if (scl_unlikely(n < 0 || (size_t)n >= sizeof(buf)))
    return;

  /* Append the user's formatted message. */
  va_list args_copy;
  va_copy(args_copy, args);
  int m = vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, args_copy);
  va_end(args_copy);
  if (scl_unlikely(m < 0))
    return;
  n += m;

  /* Ensure newline. */
  if (n > 0 && buf[n - 1] != '\n') {
    if ((size_t)n + 1 < sizeof(buf)) {
      buf[n] = '\n';
      buf[n + 1] = '\0';
      n++;
    }
  }

  /* Single atomic write — lines won't interleave. */
  (void)write(STDERR_FILENO, buf, (size_t)n);
}

void scl_log_vdebug(const char *fmt, va_list args) {
  scl_log_vwrite(SCL_LOG_DEBUG, fmt, args);
}

void scl_log_vinfo(const char *fmt, va_list args) {
  scl_log_vwrite(SCL_LOG_INFO, fmt, args);
}

void scl_log_vwarn(const char *fmt, va_list args) {
  scl_log_vwrite(SCL_LOG_WARN, fmt, args);
}

void scl_log_verror(const char *fmt, va_list args) {
  scl_log_vwrite(SCL_LOG_ERROR, fmt, args);
}

void scl_log_debug(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  scl_log_vdebug(fmt, args);
  va_end(args);
}

void scl_log_info(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  scl_log_vinfo(fmt, args);
  va_end(args);
}

void scl_log_warn(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  scl_log_vwarn(fmt, args);
  va_end(args);
}

void scl_log_error(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  scl_log_verror(fmt, args);
  va_end(args);
}