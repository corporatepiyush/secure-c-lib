#ifndef SCL_PARSE_JSON_H
#define SCL_PARSE_JSON_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"

typedef enum {
    SCL_JSON_NULL,
    SCL_JSON_BOOL,
    SCL_JSON_INT64,
    SCL_JSON_DOUBLE,
    SCL_JSON_STRING,
    SCL_JSON_ARRAY,
    SCL_JSON_OBJECT
} scl_parse_json_type_t;

typedef struct scl_parse_json_value {
    scl_parse_json_type_t type;
    union {
        int bool_val;
        int64_t int64_val;
        double double_val;
        char *string_val;
    };
    struct scl_parse_json_value *parent;
    size_t child_count;
    size_t child_cap;
    struct scl_parse_json_value **children;
    char **keys;
} scl_parse_json_value_t;

scl_error_t scl_parse_json_parse(scl_allocator_t *alloc, const char *json_str, scl_parse_json_value_t **out_root);
scl_parse_json_type_t scl_parse_json_get_type(const scl_parse_json_value_t *val);
int64_t scl_parse_json_get_int(const scl_parse_json_value_t *val);
double scl_parse_json_get_double(const scl_parse_json_value_t *val);
const char *scl_parse_json_get_string(const scl_parse_json_value_t *val);
int scl_parse_json_get_bool(const scl_parse_json_value_t *val);
scl_parse_json_value_t *scl_parse_json_object_get(const scl_parse_json_value_t *obj, const char *key);
scl_parse_json_value_t *scl_parse_json_array_get(const scl_parse_json_value_t *arr, size_t index);
size_t scl_parse_json_array_len(const scl_parse_json_value_t *arr);
size_t scl_parse_json_object_len(const scl_parse_json_value_t *obj);
void scl_parse_json_free(scl_allocator_t *alloc, scl_parse_json_value_t *root);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
