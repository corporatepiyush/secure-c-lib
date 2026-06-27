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

/* TSV streaming parser. Tab-delimited, escape handling. Row-by-row callback, zero inter-row buffering. */

#ifndef SCL_PARSE_TSV_H
#define SCL_PARSE_TSV_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

typedef enum {
    SCL_TSV_STATE_FIELD_START,
    SCL_TSV_STATE_FIELD,
    SCL_TSV_STATE_CR
} scl_parse_tsv_state_t;

typedef struct {
    scl_allocator_t *alloc;
    scl_parse_tsv_state_t state;
    char *buffer;
    size_t buffer_cap;
    size_t buffer_len;
    size_t pos;
    int eof;
} scl_parse_tsv_t;

scl_error_t scl_parse_tsv_init(scl_allocator_t *alloc, scl_parse_tsv_t *parser);
scl_error_t scl_parse_tsv_feed(scl_parse_tsv_t *parser, const char *data, size_t len);
scl_error_t scl_parse_tsv_next_field(scl_parse_tsv_t *parser, const char **out, size_t *out_len);
scl_error_t scl_parse_tsv_next_row(scl_parse_tsv_t *parser);
scl_error_t scl_parse_tsv_destroy(scl_parse_tsv_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
