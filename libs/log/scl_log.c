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

#include "scl_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

static struct {
    scl_log_level_t min_level;
    int use_color;
} scl_log_state = {
    .min_level = SCL_LOG_DEBUG,
    .use_color = 1,
};

static const char *scl_log_level_name(scl_log_level_t level) {
    switch (level) {
        case SCL_LOG_DEBUG: return "DEBUG";
        case SCL_LOG_INFO: return "INFO";
        case SCL_LOG_WARN: return "WARN";
        case SCL_LOG_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

static const char *scl_log_color(scl_log_level_t level) {
    if (!scl_log_state.use_color) return "";
    switch (level) {
        case SCL_LOG_DEBUG: return "\033[36m";
        case SCL_LOG_INFO: return "\033[32m";
        case SCL_LOG_WARN: return "\033[33m";
        case SCL_LOG_ERROR: return "\033[31m";
        default: return "";
    }
}

static const char *scl_log_color_reset(void) {
    return scl_log_state.use_color ? "\033[0m" : "";
}

void scl_log_init(scl_log_config_t config) {
    scl_log_state.min_level = config.min_level;
    scl_log_state.use_color = config.use_color;
}

void scl_log_set_level(scl_log_level_t level) {
    scl_log_state.min_level = level;
}

static void scl_log_vwrite(scl_log_level_t level, const char *fmt, va_list args) {
    if (scl_unlikely(level < scl_log_state.min_level)) return;
    
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    fprintf(stderr, "%s[%s] %s: %s", 
            scl_log_color(level),
            timestamp,
            scl_log_level_name(level),
            scl_log_color_reset());
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
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
