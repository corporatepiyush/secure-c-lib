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

/* Parquet footer parser (PAR1). Column chunk metadata, page stats, schema.
 * Row-group row counts and sizes. */

#ifndef SCL_PARSE_PARQUET_H
#define SCL_PARSE_PARQUET_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

/* Maximum size of Parquet file to load (default 512 MB). */
#ifndef SCL_PARQUET_MAX_FILE_SIZE
#define SCL_PARQUET_MAX_FILE_SIZE ((size_t)512 * 1024 * 1024)
#endif

/* Maximum number of schema items allowed in the footer (prevents
 * pathological schema complexity from causing memory exhaustion). */
#ifndef SCL_PARQUET_MAX_SCHEMA_ITEMS
#define SCL_PARQUET_MAX_SCHEMA_ITEMS 4096
#endif

/* Maximum length of a single column name in bytes. */
#ifndef SCL_PARQUET_MAX_COLUMN_NAME
#define SCL_PARQUET_MAX_COLUMN_NAME 1024
#endif

typedef struct {
   scl_allocator_t *alloc;
   char *filename;
   FILE *fp;
   unsigned char *buf;
   size_t buf_size;
   int64_t num_rows;
   int num_columns;
   char **column_names;
   int *column_types;
} scl_parse_parquet_t;

scl_error_t scl_parse_parquet_open(scl_allocator_t *alloc,
                                    scl_parse_parquet_t *parser,
                                    const char *filename);
scl_error_t scl_parse_parquet_get_row_count(scl_parse_parquet_t *parser,
                                             int64_t *out);
scl_error_t scl_parse_parquet_get_column_count(scl_parse_parquet_t *parser,
                                                int *out);
scl_error_t scl_parse_parquet_get_column_name(scl_parse_parquet_t *parser,
                                               int index, const char **out,
                                               size_t *out_len);
scl_error_t scl_parse_parquet_close(scl_parse_parquet_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif