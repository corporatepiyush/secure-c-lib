#ifndef SCL_PARSE_TSV_H
#define SCL_PARSE_TSV_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>
#include <stdio.h>

typedef enum {
    SCL_TSV_STATE_FIELD_START,
    SCL_TSV_STATE_FIELD,
    SCL_TSV_STATE_CR
} scl_parse_tsv_state_t;

typedef struct {
    scl_parse_tsv_state_t state;
    char *buffer;
    size_t buffer_cap;
    size_t buffer_len;
    size_t pos;
    int eof;
} scl_parse_tsv_t;

scl_error_t scl_parse_tsv_init(scl_parse_tsv_t *parser);
scl_error_t scl_parse_tsv_feed(scl_parse_tsv_t *parser, const char *data, size_t len);
scl_error_t scl_parse_tsv_next_field(scl_parse_tsv_t *parser, const char **out, size_t *out_len);
scl_error_t scl_parse_tsv_next_row(scl_parse_tsv_t *parser);
scl_error_t scl_parse_tsv_destroy(scl_parse_tsv_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
