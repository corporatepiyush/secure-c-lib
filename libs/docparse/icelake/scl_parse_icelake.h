#ifndef SCL_PARSE_ICELAKE_H
#define SCL_PARSE_ICELAKE_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "json/scl_parse_json.h"

typedef struct {
    scl_allocator_t *alloc;
    char *filename;
    char *manifest_json;
    size_t manifest_len;
    int64_t snapshot_id;
    int num_manifest_files;
    char **manifest_files;
    int num_data_files;
    char **data_files;
} scl_parse_icelake_t;

scl_error_t scl_parse_icelake_open(scl_allocator_t *alloc, scl_parse_icelake_t *parser, const char *path);
scl_error_t scl_parse_icelake_get_snapshot_id(scl_parse_icelake_t *parser, int64_t *out);
scl_error_t scl_parse_icelake_get_manifest_count(scl_parse_icelake_t *parser, int *out);
scl_error_t scl_parse_icelake_get_manifest_path(scl_parse_icelake_t *parser, int index, const char **out, size_t *out_len);
scl_error_t scl_parse_icelake_get_data_file_path(scl_parse_icelake_t *parser, int index, const char **out, size_t *out_len);
scl_error_t scl_parse_icelake_close(scl_parse_icelake_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
