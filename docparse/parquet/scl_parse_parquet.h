#ifndef SCL_PARSE_PARQUET_H
#define SCL_PARSE_PARQUET_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    char *filename;
    FILE *fp;
    unsigned char *buf;
    size_t buf_size;
    int64_t num_rows;
    int num_columns;
    char **column_names;
    int *column_types;
} scl_parse_parquet_t;

scl_error_t scl_parse_parquet_open(scl_parse_parquet_t *parser, const char *filename);
scl_error_t scl_parse_parquet_get_row_count(scl_parse_parquet_t *parser, int64_t *out);
scl_error_t scl_parse_parquet_get_column_count(scl_parse_parquet_t *parser, int *out);
scl_error_t scl_parse_parquet_get_column_name(scl_parse_parquet_t *parser, int index, const char **out, size_t *out_len);
scl_error_t scl_parse_parquet_close(scl_parse_parquet_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
