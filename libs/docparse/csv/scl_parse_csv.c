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

/* RFC 4180 CSV state-machine parser. Handles quoted fields, embedded delimiters, CRLF. Row/field callback, bounds-checked. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_csv.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#define CSV_INIT_BUF 4096

scl_error_t scl_parse_csv_init(scl_allocator_t *alloc, scl_parse_csv_t *parser) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    parser->alloc = alloc;
    parser->state = SCL_CSV_STATE_FIELD_START;
    parser->buffer = (char *)scl_alloc(alloc, CSV_INIT_BUF, _Alignof(max_align_t));
    if (scl_unlikely(!parser->buffer)) return SCL_ERR_OUT_OF_MEMORY;
    parser->buffer_cap = CSV_INIT_BUF;
    parser->buffer_len = 0;
    parser->pos = 0;
    parser->row_started = 0;
    parser->eof = 0;
    return SCL_OK;
}

static int csv_ensure_buf(scl_parse_csv_t *parser, size_t needed) {
    if (needed <= parser->buffer_cap) return 0;
    size_t new_cap = parser->buffer_cap;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) { new_cap = needed; break; }
        new_cap *= 2;
    }
    char *nb = (char *)scl_realloc(parser->alloc, parser->buffer, parser->buffer_cap, new_cap, _Alignof(max_align_t));
    if (!nb) return -1;
    parser->buffer = nb;
    parser->buffer_cap = new_cap;
    return 0;
}

scl_error_t scl_parse_csv_feed(scl_parse_csv_t *parser, const char *data, size_t len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!data && len > 0)) return SCL_ERR_NULL_PTR;

    size_t need;
    if (scl_add_overflow(parser->buffer_len, len, &need) ||
        scl_add_overflow(need, 1, &need))
        return SCL_ERR_SIZE_OVERFLOW;
    if (csv_ensure_buf(parser, need) != 0)
        return SCL_ERR_OUT_OF_MEMORY;

    scl_memcpy(parser->buffer + parser->buffer_len, data, len);
    parser->buffer_len += len;
    parser->buffer[parser->buffer_len] = '\0';
    return SCL_OK;
}

scl_error_t scl_parse_csv_next_field(scl_parse_csv_t *parser, const char **out, size_t *out_len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;

    if (parser->pos >= parser->buffer_len) return SCL_ERR_EMPTY;

    size_t start = parser->pos;
    size_t field_start = parser->pos;

    switch (parser->state) {
    case SCL_CSV_STATE_FIELD_START:
        if (parser->buffer[parser->pos] == '"') {
            parser->state = SCL_CSV_STATE_QUOTED;
            parser->pos++;
            start = parser->pos;
            field_start = parser->pos;
            while (parser->pos < parser->buffer_len) {
                if (parser->buffer[parser->pos] == '"') {
                    if (parser->pos + 1 < parser->buffer_len && parser->buffer[parser->pos + 1] == '"') {
                        scl_memmove(&parser->buffer[field_start], &parser->buffer[start], parser->pos - start);
                        field_start += parser->pos - start;
                        parser->pos += 2;
                        start = parser->pos;
                    } else {
                        parser->state = SCL_CSV_STATE_QUOTE_END;
                        parser->pos++;
                        goto done_quoted;
                    }
                } else {
                    parser->pos++;
                }
            }
            scl_memmove(&parser->buffer[field_start], &parser->buffer[start], parser->pos - start);
            field_start += parser->pos - start;
            start = parser->pos;
            *out = &parser->buffer[field_start];
            *out_len = parser->pos - start;
            return SCL_OK;
        }
        parser->state = SCL_CSV_STATE_UNQUOTED;

    case SCL_CSV_STATE_UNQUOTED:
        while (parser->pos < parser->buffer_len) {
            char c = parser->buffer[parser->pos];
            if (c == ',' || c == '\n' || c == '\r') {
                break;
            }
            parser->pos++;
        }
        *out = &parser->buffer[start];
        *out_len = parser->pos - start;
        return SCL_OK;

    case SCL_CSV_STATE_QUOTED:
        while (parser->pos < parser->buffer_len) {
            if (parser->buffer[parser->pos] == '"') {
                if (parser->pos + 1 < parser->buffer_len && parser->buffer[parser->pos + 1] == '"') {
                    scl_memmove(&parser->buffer[field_start], &parser->buffer[start], parser->pos - start);
                    field_start += parser->pos - start;
                    parser->pos += 2;
                    start = parser->pos;
                } else {
                    parser->state = SCL_CSV_STATE_QUOTE_END;
                    parser->pos++;
                    goto done_quoted;
                }
            } else {
                parser->pos++;
            }
        }
        scl_memmove(&parser->buffer[field_start], &parser->buffer[start], parser->pos - start);
        field_start += parser->pos - start;
        *out = &parser->buffer[field_start];
        *out_len = 0;
        return SCL_OK;

    case SCL_CSV_STATE_QUOTE_END:
    case SCL_CSV_STATE_CR:
        break;
    }

    *out = &parser->buffer[start];
    *out_len = 0;
    return SCL_OK;

done_quoted:
    scl_memmove(&parser->buffer[field_start], &parser->buffer[start], parser->pos - start);
    field_start += parser->pos - start;

    if (parser->state == SCL_CSV_STATE_QUOTE_END) {
        if (parser->pos < parser->buffer_len) {
            if (parser->buffer[parser->pos] == ',') {
                parser->state = SCL_CSV_STATE_FIELD_START;
                parser->pos++;
            } else if (parser->buffer[parser->pos] == '\r') {
                parser->state = SCL_CSV_STATE_CR;
                parser->pos++;
            } else if (parser->buffer[parser->pos] == '\n') {
                parser->state = SCL_CSV_STATE_FIELD_START;
                parser->pos++;
                parser->row_started = 0;
            }
        }
    }

    *out = &parser->buffer[field_start];
    *out_len = parser->pos - start;
    return SCL_OK;
}

scl_error_t scl_parse_csv_next_row(scl_parse_csv_t *parser) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;

    parser->row_started = 1;

    if (parser->state == SCL_CSV_STATE_CR) {
        if (parser->pos < parser->buffer_len && parser->buffer[parser->pos] == '\n')
            parser->pos++;
        parser->state = SCL_CSV_STATE_FIELD_START;
        return SCL_OK;
    }

    while (parser->pos < parser->buffer_len) {
        char c = parser->buffer[parser->pos];
        if (c == '\n') {
            parser->pos++;
            parser->state = SCL_CSV_STATE_FIELD_START;
            return SCL_OK;
        }
        if (c == '\r') {
            parser->state = SCL_CSV_STATE_CR;
            parser->pos++;
            return SCL_OK;
        }
        parser->pos++;
    }
    return SCL_ERR_EMPTY;
}

scl_error_t scl_parse_csv_destroy(scl_parse_csv_t *parser) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    scl_free(parser->alloc, parser->buffer);
    parser->buffer = NULL;
    parser->buffer_cap = 0;
    parser->buffer_len = 0;
    parser->pos = 0;
    return SCL_OK;
}
