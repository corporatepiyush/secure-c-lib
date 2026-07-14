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

/* RFC 4180 CSV state-machine parser. Handles quoted fields, embedded
 * delimiters, CRLF. Row/field callback, bounds-checked. */

#ifndef SCL_PARSE_CSV_H
#define SCL_PARSE_CSV_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef enum {
  SCL_CSV_STATE_FIELD_START,
  SCL_CSV_STATE_UNQUOTED,
  SCL_CSV_STATE_QUOTED,
  SCL_CSV_STATE_QUOTE_END,
  SCL_CSV_STATE_CR
} scl_parse_csv_state_t;

/* Maximum number of fields per row (hard cap to prevent memory exhaustion). */
#ifndef SCL_CSV_MAX_FIELDS
#define SCL_CSV_MAX_FIELDS 8192
#endif
/* Maximum number of rows before feed() refuses more data. */
#ifndef SCL_CSV_MAX_ROWS
#define SCL_CSV_MAX_ROWS 65536
#endif

typedef struct {
   scl_allocator_t *alloc;
   scl_parse_csv_state_t state;
   char *buffer;
   size_t buffer_cap;
   size_t buffer_len;
   size_t pos;
   int row_started;
   int eof;
   size_t total_rows;  /* rows returned so far */
   size_t field_count; /* fields in the current row so far */
   size_t max_rows;    /* configurable cap */
   size_t max_fields;  /* configurable per-row cap */
} scl_parse_csv_t;

/**
 * Initialise with explicit limits.  Pass 0 for either cap to use the
 * compile-time default (SCL_CSV_MAX_FIELDS / SCL_CSV_MAX_ROWS).
 */
scl_error_t scl_parse_csv_init_with_limits(scl_allocator_t *alloc,
                                            scl_parse_csv_t *parser,
                                            size_t max_fields_per_row,
                                            size_t max_rows);
scl_error_t scl_parse_csv_init(scl_allocator_t *alloc, scl_parse_csv_t *parser);
scl_error_t scl_parse_csv_feed(scl_parse_csv_t *parser, const char *data,
                                size_t len);
scl_error_t scl_parse_csv_next_field(scl_parse_csv_t *parser, const char **out,
                                      size_t *out_len);
scl_error_t scl_parse_csv_next_row(scl_parse_csv_t *parser);
scl_error_t scl_parse_csv_destroy(scl_parse_csv_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif