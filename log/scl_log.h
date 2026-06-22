#ifndef SCL_LOG_H
#define SCL_LOG_H

#include <stdarg.h>
#include "../common/scl_common.h"

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
