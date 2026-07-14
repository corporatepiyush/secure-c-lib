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

/* XLSX sheet/cell reader. Shared strings, sheet XML, typed cell values
 * (number/string/date/bool). Streaming iterator. */

#ifndef SCL_PARSE_XLSX_H
#define SCL_PARSE_XLSX_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

/* Maximum size of ZIP buffer to load (default 128 MB). */
#ifndef SCL_XLSX_MAX_ZIP_SIZE
#define SCL_XLSX_MAX_ZIP_SIZE ((size_t)128 * 1024 * 1024)
#endif
/* Maximum number of shared strings (default 65536). */
#ifndef SCL_XLSX_MAX_SHARED_STRINGS
#define SCL_XLSX_MAX_SHARED_STRINGS 65536
#endif
/* Maximum number of sheets (default 256). */
#ifndef SCL_XLSX_MAX_SHEETS
#define SCL_XLSX_MAX_SHEETS 256
#endif
/* Maximum size of a single sheet's XML data in memory (default 16 MB). */
#ifndef SCL_XLSX_MAX_SHEET_DATA
#define SCL_XLSX_MAX_SHEET_DATA ((size_t)16 * 1024 * 1024)
#endif

typedef struct {
   scl_allocator_t *alloc;
   char *filename;
   FILE *fp;
   unsigned char *zip_buf;
   size_t zip_size;
   char **shared_strings;
   size_t shared_count;
   size_t sheet_count;
   char **sheet_names;
   char **sheet_data;
} scl_parse_xlsx_t;

scl_error_t scl_parse_xlsx_open(scl_allocator_t *alloc,
                                 scl_parse_xlsx_t *parser, const char *filename);
scl_error_t scl_parse_xlsx_get_sheets_count(scl_parse_xlsx_t *parser, int *out);
scl_error_t scl_parse_xlsx_get_sheet_name(scl_parse_xlsx_t *parser, int index,
                                           const char **out, size_t *out_len);
scl_error_t scl_parse_xlsx_get_cell(scl_parse_xlsx_t *parser, int sheet_idx,
                                     const char *cell_ref, const char **out,
                                     size_t *out_len);
scl_error_t scl_parse_xlsx_close(scl_parse_xlsx_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif