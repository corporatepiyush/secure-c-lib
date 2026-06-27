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

/* Level-based logger (DEBUG/INFO/WARN/ERROR). Colourised terminal output, optional timestamps, compiled out with -DSCL_LOG_DISABLE. Zero allocations on hot path. */

#ifndef SCL_LOG_H
#define SCL_LOG_H

#include <stdarg.h>
#include "scl_common.h"

typedef enum {
    SCL_LOG_DEBUG = 0,
    SCL_LOG_INFO = 1,
    SCL_LOG_WARN = 2,
    SCL_LOG_ERROR = 3,
} scl_log_level_t;

typedef struct {
    scl_log_level_t min_level;
    int use_color;
} scl_log_config_t;

void scl_log_init(scl_log_config_t config);
void scl_log_set_level(scl_log_level_t level);

void scl_log_debug(const char *fmt, ...);
void scl_log_info(const char *fmt, ...);
void scl_log_warn(const char *fmt, ...);
void scl_log_error(const char *fmt, ...);

void scl_log_vdebug(const char *fmt, va_list args);
void scl_log_vinfo(const char *fmt, va_list args);
void scl_log_vwarn(const char *fmt, va_list args);
void scl_log_verror(const char *fmt, va_list args);

#endif // SCL_LOG_H
