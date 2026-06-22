#ifndef SCL_PARSE_ICELAKE_H
#define SCL_PARSE_ICELAKE_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    char *metadata_path;
    int format_version;
    int64_t current_snapshot_id;
    int snapshot_count;
    char *schema_json;
    size_t schema_len;
    char *raw_json;
    size_t raw_len;
} scl_parse_icelake_t;

scl_error_t scl_parse_icelake_open(scl_parse_icelake_t *parser, const char *metadata_path);
scl_error_t scl_parse_icelake_get_snapshot_count(scl_parse_icelake_t *parser, int *out);
scl_error_t scl_parse_icelake_get_schema(scl_parse_icelake_t *parser, const char **out, size_t *out_len);
scl_error_t scl_parse_icelake_close(scl_parse_icelake_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
