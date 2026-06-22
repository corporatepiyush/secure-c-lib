#include "scl_log.h"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

static scl_log_level_t g_level = SCL_LOG_INFO;
static scl_log_writer_t g_writer = NULL;
static void *g_writer_ctx = NULL;

static const char *level_label(scl_log_level_t lvl) {
    switch (lvl) {
    case SCL_LOG_DEBUG: return "DEBUG";
    case SCL_LOG_INFO:  return "INFO";
    case SCL_LOG_WARN:  return "WARN";
    case SCL_LOG_ERROR: return "ERROR";
    default:            return "?";
    }
}

static void default_writer(scl_log_level_t level, const char *msg, void *ctx) {
    (void)ctx;
    fprintf(stderr, "%s: %s\n", level_label(level), msg);
}

void scl_log_set_level(scl_log_level_t level) {
    g_level = level;
}

void scl_log_set_writer(scl_log_writer_t writer, void *ctx) {
    g_writer = writer;
    g_writer_ctx = ctx;
}

void scl_log_write(scl_log_level_t level, const char *file, int line,
                   const char *func, const char *fmt, ...) {
    if (level < g_level) return;
    if (!g_writer) g_writer = default_writer;

    char buf[4096];
    va_list args;
    va_start(args, fmt);
    int n = snprintf(buf, sizeof(buf), "%s:%d [%s] ", file, line, func);
    if (n > 0 && (size_t)n < sizeof(buf)) {
        vsnprintf(buf + n, sizeof(buf) - (size_t)n, fmt, args);
    }
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';
    g_writer(level, buf, g_writer_ctx);
}
