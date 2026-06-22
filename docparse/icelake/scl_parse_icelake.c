#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_icelake.h"
#include "../json/scl_parse_json.h"
#include <stdlib.h>
#include <string.h>

scl_error_t scl_parse_icelake_open(scl_parse_icelake_t *parser, const char *metadata_path) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!metadata_path, 0)) return SCL_ERR_NULL_PTR;

    memset(parser, 0, sizeof(*parser));

    parser->metadata_path = strdup(metadata_path);
    if (!parser->metadata_path) return SCL_ERR_OUT_OF_MEMORY;

    /* Read entire file */
    FILE *fp = fopen(metadata_path, "rb");
    if (!fp) { free(parser->metadata_path); return SCL_ERR_NOT_FOUND; }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); free(parser->metadata_path); return SCL_ERR_ALLOC; }
    rewind(fp);

    parser->raw_json = (char *)malloc((size_t)sz + 1);
    if (!parser->raw_json) { fclose(fp); free(parser->metadata_path); return SCL_ERR_OUT_OF_MEMORY; }
    if (fread(parser->raw_json, 1, (size_t)sz, fp) != (size_t)sz) {
        fclose(fp); free(parser->metadata_path); free(parser->raw_json);
        return SCL_ERR_ALLOC;
    }
    fclose(fp);
    parser->raw_len = (size_t)sz;
    parser->raw_json[sz] = '\0';

    /* Parse JSON */
    scl_parse_json_value_t *root = NULL;
    scl_error_t err = scl_parse_json_parse(parser->raw_json, &root);
    if (err != SCL_OK) {
        free(parser->metadata_path);
        free(parser->raw_json);
        parser->metadata_path = NULL;
        parser->raw_json = NULL;
        return err;
    }

    /* Extract format-version */
    scl_parse_json_value_t *fv = scl_parse_json_object_get(root, "format-version");
    if (fv && scl_parse_json_get_type(fv) == SCL_JSON_INT64)
        parser->format_version = (int)scl_parse_json_get_int(fv);

    /* Extract current-snapshot-id */
    scl_parse_json_value_t *sid = scl_parse_json_object_get(root, "current-snapshot-id");
    if (sid && scl_parse_json_get_type(sid) == SCL_JSON_INT64)
        parser->current_snapshot_id = scl_parse_json_get_int(sid);

    /* Count snapshots */
    scl_parse_json_value_t *snapshots = scl_parse_json_object_get(root, "snapshots");
    if (snapshots && scl_parse_json_get_type(snapshots) == SCL_JSON_ARRAY)
        parser->snapshot_count = (int)scl_parse_json_array_len(snapshots);

    /* Extract schema */
    scl_parse_json_value_t *schemas = scl_parse_json_object_get(root, "schemas");
    if (schemas && scl_parse_json_get_type(schemas) == SCL_JSON_ARRAY &&
        scl_parse_json_array_len(schemas) > 0) {
        scl_parse_json_value_t *first_schema = scl_parse_json_array_get(schemas, 0);
        /* Serialize back to JSON string */
        if (first_schema) {
            const char *s = scl_parse_json_get_string(first_schema);
            if (s) {
                parser->schema_json = strdup(s);
                parser->schema_len = strlen(s);
            }
        }
    }

    /* If no schemas array, try current-schema-id */
    if (!parser->schema_json) {
        scl_parse_json_value_t *cs = scl_parse_json_object_get(root, "schema");
        if (cs) {
            const char *s = scl_parse_json_get_string(cs);
            if (s) {
                parser->schema_json = strdup(s);
                parser->schema_len = strlen(s);
            }
        }
    }

    scl_parse_json_free(root);
    return SCL_OK;
}

scl_error_t scl_parse_icelake_get_snapshot_count(scl_parse_icelake_t *parser, int *out) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    *out = parser->snapshot_count;
    return SCL_OK;
}

scl_error_t scl_parse_icelake_get_schema(scl_parse_icelake_t *parser, const char **out, size_t *out_len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    if (!parser->schema_json) return SCL_ERR_NOT_FOUND;
    *out = parser->schema_json;
    if (out_len) *out_len = parser->schema_len;
    return SCL_OK;
}

scl_error_t scl_parse_icelake_close(scl_parse_icelake_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    free(parser->metadata_path); parser->metadata_path = NULL;
    free(parser->raw_json); parser->raw_json = NULL;
    free(parser->schema_json); parser->schema_json = NULL;
    parser->raw_len = parser->schema_len = 0;
    parser->format_version = 0;
    parser->current_snapshot_id = 0;
    parser->snapshot_count = 0;
    return SCL_OK;
}
