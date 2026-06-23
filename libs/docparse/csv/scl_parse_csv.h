#ifndef SCL_PARSE_CSV_H
#define SCL_PARSE_CSV_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef enum {
    SCL_CSV_STATE_FIELD_START,
    SCL_CSV_STATE_UNQUOTED,
    SCL_CSV_STATE_QUOTED,
    SCL_CSV_STATE_QUOTE_END,
    SCL_CSV_STATE_CR
} scl_parse_csv_state_t;

typedef struct {
    scl_allocator_t *alloc;
    scl_parse_csv_state_t state;
    char *buffer;
    size_t buffer_cap;
    size_t buffer_len;
    size_t pos;
    int row_started;
    int eof;
} scl_parse_csv_t;

scl_error_t scl_parse_csv_init(scl_allocator_t *alloc, scl_parse_csv_t *parser);
scl_error_t scl_parse_csv_feed(scl_parse_csv_t *parser, const char *data, size_t len);
scl_error_t scl_parse_csv_next_field(scl_parse_csv_t *parser, const char **out, size_t *out_len);
scl_error_t scl_parse_csv_next_row(scl_parse_csv_t *parser);
scl_error_t scl_parse_csv_destroy(scl_parse_csv_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
