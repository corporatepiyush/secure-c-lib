#ifndef SCL_LOG_H
#define SCL_LOG_H

#include "../common/scl_common.h"

typedef enum {
    SCL_LOG_DEBUG = 0,
    SCL_LOG_INFO,
    SCL_LOG_WARN,
    SCL_LOG_ERROR
} scl_log_level_t;

typedef void (*scl_log_writer_t)(scl_log_level_t level, const char *msg, void *ctx);

/* Set minimum level (messages below are suppressed) */
void scl_log_set_level(scl_log_level_t level);

/* Set custom writer; default writes to stderr with level prefix */
void scl_log_set_writer(scl_log_writer_t writer, void *ctx);

/* Core logging functions */
void scl_log_write(scl_log_level_t level, const char *file, int line,
                   const char *func, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

#define scl_log_debug(...)  scl_log_write(SCL_LOG_DEBUG,  __FILE__, __LINE__, __func__, __VA_ARGS__)
#define scl_log_info(...)   scl_log_write(SCL_LOG_INFO,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define scl_log_warn(...)   scl_log_write(SCL_LOG_WARN,   __FILE__, __LINE__, __func__, __VA_ARGS__)
#define scl_log_error(...)  scl_log_write(SCL_LOG_ERROR,  __FILE__, __LINE__, __func__, __VA_ARGS__)

#endif
