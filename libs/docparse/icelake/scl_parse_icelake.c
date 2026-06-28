/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Iceberg table format reader. Metadata JSON + manifest list/entries. Partition pruning via manifest scan filtering. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_icelake.h"
#include "scl_stdlib.h"
#include "scl_string.h"

scl_error_t scl_parse_icelake_open(scl_allocator_t *alloc, scl_parse_icelake_t *parser, const char *path) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!path)) return SCL_ERR_NULL_PTR;

    (void)scl_memset(parser, 0, sizeof(*parser));
    parser->alloc = alloc;

    parser->filename = scl_strdup(alloc, path);
    if (!parser->filename) return SCL_ERR_OUT_OF_MEMORY;

    char *meta_path = NULL;
    char *json_buf = NULL;
    scl_parse_json_value_t *root = NULL;

    size_t plen = scl_strlen(path);
    int has_trailing = (plen > 0 && path[plen - 1] == '/') ? 1 : 0;

    size_t mp_len = plen + 20 + (has_trailing ? 0 : 1);
    meta_path = (char *)scl_alloc(alloc, mp_len, _Alignof(max_align_t));
    if (!meta_path) { scl_free(alloc, parser->filename); return SCL_ERR_OUT_OF_MEMORY; }

    if (has_trailing) {
        snprintf(meta_path, mp_len, "%smetadata.json", path);
    } else {
        snprintf(meta_path, mp_len, "%s/metadata.json", path);
    }

    FILE *fp = fopen(meta_path, "rb");
    if (!fp) {
        scl_free(alloc, meta_path);
        scl_free(alloc, parser->filename);
        return SCL_ERR_NOT_FOUND;
    }

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz < 0) { fclose(fp); scl_free(alloc, meta_path); scl_free(alloc, parser->filename); return SCL_ERR_ALLOC; }
    rewind(fp);
    json_buf = (char *)scl_alloc(alloc, (size_t)sz + 1, _Alignof(max_align_t));
    if (!json_buf) {
        fclose(fp);
        scl_free(alloc, meta_path);
        scl_free(alloc, parser->filename);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    if (fread(json_buf, 1, (size_t)sz, fp) != (size_t)sz) {
        fclose(fp);
        scl_free(alloc, json_buf);
        scl_free(alloc, meta_path);
        scl_free(alloc, parser->filename);
        return SCL_ERR_ALLOC;
    }
    fclose(fp);
    json_buf[sz] = '\0';
    parser->manifest_json = json_buf;
    parser->manifest_len = (size_t)sz;

    scl_error_t json_err = scl_parse_json_parse(alloc, json_buf, &root);
    if (json_err != SCL_OK) {
        scl_free(alloc, meta_path);
        scl_free(alloc, json_buf);
        scl_free(alloc, parser->filename);
        return json_err;
    }

    if (root && root->type == SCL_JSON_OBJECT) {
        scl_parse_json_value_t *snap = scl_parse_json_object_get(root, "snapshot-id");
        if (snap) {
            /* Iceberg metadata stores snapshot-id as a JSON number; tolerate a
             * quoted string form too. */
            if (snap->type == SCL_JSON_INT64) {
                parser->snapshot_id = snap->int64_val;
            } else if (snap->type == SCL_JSON_STRING && snap->string_val) {
                parser->snapshot_id = scl_strtoll(snap->string_val, NULL, 10);
            }
        }

        scl_parse_json_value_t *manifests = scl_parse_json_object_get(root, "manifests");
        if (manifests && manifests->type == SCL_JSON_ARRAY) {
            parser->num_manifest_files = (int)manifests->child_count;
            parser->manifest_files = (char **)scl_calloc(alloc, (size_t)parser->num_manifest_files, sizeof(char *), _Alignof(max_align_t));
            if (!parser->manifest_files) parser->num_manifest_files = 0;
            for (int i = 0; i < parser->num_manifest_files && i < (int)manifests->child_count; i++) {
                scl_parse_json_value_t *mf = manifests->children[i];
                if (mf && mf->type == SCL_JSON_OBJECT) {
                    scl_parse_json_value_t *mp = scl_parse_json_object_get(mf, "manifest-path");
                    if (mp && mp->type == SCL_JSON_STRING) {
                        parser->manifest_files[i] = scl_strdup(alloc, mp->string_val);
                    }
                }
            }
        }

        scl_parse_json_value_t *entries = scl_parse_json_object_get(root, "entries");
        if (entries && entries->type == SCL_JSON_ARRAY) {
            parser->num_data_files = (int)entries->child_count;
            parser->data_files = (char **)scl_calloc(alloc, (size_t)parser->num_data_files, sizeof(char *), _Alignof(max_align_t));
            if (!parser->data_files) parser->num_data_files = 0;
            for (int i = 0; i < parser->num_data_files && i < (int)entries->child_count; i++) {
                scl_parse_json_value_t *ent = entries->children[i];
                if (ent && ent->type == SCL_JSON_OBJECT) {
                    scl_parse_json_value_t *fp2 = scl_parse_json_object_get(ent, "file-path");
                    if (!fp2) fp2 = scl_parse_json_object_get(ent, "file_path");
                    if (fp2 && fp2->type == SCL_JSON_STRING) {
                        parser->data_files[i] = scl_strdup(alloc, fp2->string_val);
                    }
                }
            }
        }
    }

    scl_parse_json_free(alloc, root);
    scl_free(alloc, meta_path);

    return SCL_OK;
}

scl_error_t scl_parse_icelake_get_snapshot_id(scl_parse_icelake_t *parser, int64_t *out) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    *out = parser->snapshot_id;
    return SCL_OK;
}

scl_error_t scl_parse_icelake_get_manifest_count(scl_parse_icelake_t *parser, int *out) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    *out = parser->num_manifest_files;
    return SCL_OK;
}

scl_error_t scl_parse_icelake_get_manifest_path(scl_parse_icelake_t *parser, int index,
                                                  const char **out, size_t *out_len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    if (index < 0 || index >= parser->num_manifest_files) return SCL_ERR_INVALID_INDEX;
    if (!parser->manifest_files[index]) return SCL_ERR_NOT_FOUND;
    *out = parser->manifest_files[index];
    if (out_len) *out_len = scl_strlen(parser->manifest_files[index]);
    return SCL_OK;
}

scl_error_t scl_parse_icelake_get_data_file_path(scl_parse_icelake_t *parser, int index,
                                                   const char **out, size_t *out_len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    if (index < 0 || index >= parser->num_data_files) return SCL_ERR_INVALID_INDEX;
    if (!parser->data_files[index]) return SCL_ERR_NOT_FOUND;
    *out = parser->data_files[index];
    if (out_len) *out_len = scl_strlen(parser->data_files[index]);
    return SCL_OK;
}

scl_error_t scl_parse_icelake_close(scl_parse_icelake_t *parser) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    scl_free(parser->alloc, parser->filename); parser->filename = NULL;
    scl_free(parser->alloc, parser->manifest_json); parser->manifest_json = NULL;
    if (parser->manifest_files) {
        for (int i = 0; i < parser->num_manifest_files; i++)
            scl_free(parser->alloc, parser->manifest_files[i]);
        scl_free(parser->alloc, parser->manifest_files);
        parser->manifest_files = NULL;
    }
    if (parser->data_files) {
        for (int i = 0; i < parser->num_data_files; i++)
            scl_free(parser->alloc, parser->data_files[i]);
        scl_free(parser->alloc, parser->data_files);
        parser->data_files = NULL;
    }
    parser->manifest_len = 0;
    return SCL_OK;
}
