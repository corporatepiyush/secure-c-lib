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

/* Iceberg table format reader. Metadata JSON + manifest list/entries. Partition
 * pruning via manifest scan filtering. */

#ifndef SCL_PARSE_ICELAKE_H
#define SCL_PARSE_ICELAKE_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"
#include "json/scl_parse_json.h"

/* Maximum path length for Iceberg metadata paths (default 4096). */
#ifndef SCL_ICELAKE_MAX_PATH
#define SCL_ICELAKE_MAX_PATH 4096
#endif

/* Maximum size of metadata.json to load (default 16 MB). */
#ifndef SCL_ICELAKE_MAX_METADATA_SIZE
#define SCL_ICELAKE_MAX_METADATA_SIZE ((size_t)16 * 1024 * 1024)
#endif

/* Maximum number of manifest files to parse (default 1024). */
#ifndef SCL_ICELAKE_MAX_MANIFESTS
#define SCL_ICELAKE_MAX_MANIFESTS 1024
#endif

/* Maximum number of data files to parse (default 65536). */
#ifndef SCL_ICELAKE_MAX_DATA_FILES
#define SCL_ICELAKE_MAX_DATA_FILES 65536
#endif

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

scl_error_t scl_parse_icelake_open(scl_allocator_t *alloc,
                                    scl_parse_icelake_t *parser,
                                    const char *path);
scl_error_t scl_parse_icelake_get_snapshot_id(scl_parse_icelake_t *parser,
                                               int64_t *out);
scl_error_t scl_parse_icelake_get_manifest_count(scl_parse_icelake_t *parser,
                                                  int *out);
scl_error_t scl_parse_icelake_get_manifest_path(scl_parse_icelake_t *parser,
                                                 int index, const char **out,
                                                 size_t *out_len);
scl_error_t scl_parse_icelake_get_data_file_path(scl_parse_icelake_t *parser,
                                                  int index, const char **out,
                                                  size_t *out_len);
scl_error_t scl_parse_icelake_close(scl_parse_icelake_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif