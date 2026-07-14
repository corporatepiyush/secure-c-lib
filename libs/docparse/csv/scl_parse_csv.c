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

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_csv.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#define CSV_INIT_BUF 4096

scl_error_t scl_parse_csv_init(scl_allocator_t *alloc,
                                scl_parse_csv_t *parser) {
  return scl_parse_csv_init_with_limits(alloc, parser, 0, 0);
}

scl_error_t scl_parse_csv_init_with_limits(scl_allocator_t *alloc,
                                            scl_parse_csv_t *parser,
                                            size_t max_fields_per_row,
                                            size_t max_rows) {
  if (scl_unlikely(!parser))
    return SCL_ERR_NULL_PTR;
  parser->alloc = alloc;
  parser->state = SCL_CSV_STATE_FIELD_START;
  parser->max_fields = max_fields_per_row ? max_fields_per_row : SCL_CSV_MAX_FIELDS;
  parser->max_rows = max_rows ? max_rows : SCL_CSV_MAX_ROWS;
  parser->buffer =
      (char *)scl_alloc(alloc, CSV_INIT_BUF, _Alignof(max_align_t));
  if (scl_unlikely(!parser->buffer))
    return SCL_ERR_OUT_OF_MEMORY;
  parser->buffer_cap = CSV_INIT_BUF;
  parser->buffer_len = 0;
  parser->pos = 0;
  parser->row_started = 0;
  parser->eof = 0;
  parser->total_rows = 0;
  parser->field_count = 0;
  return SCL_OK;
}

static int csv_ensure_buf(scl_parse_csv_t *parser, size_t needed) {
  if (needed <= parser->buffer_cap)
    return 0;
  /* Cap buffer growth to max_rows * max_fields as a coarse heuristic for
   * legitimate CSV size. This prevents a malicious feed from growing the
   * buffer beyond all reasonable limits. */
  if (parser->max_rows > 0 && parser->max_fields > 0) {
    size_t rough_limit = parser->max_rows * parser->max_fields * 64U;
    if (needed > rough_limit)
      return -1;
  }
  size_t new_cap = parser->buffer_cap;
  while (new_cap < needed) {
    if (new_cap > SIZE_MAX / 2) {
      new_cap = needed;
      break;
    }
    new_cap *= 2;
  }
  char *nb =
      (char *)scl_realloc(parser->alloc, parser->buffer, parser->buffer_cap,
                          new_cap, _Alignof(max_align_t));
  if (!nb)
    return -1;
  parser->buffer = nb;
  parser->buffer_cap = new_cap;
  return 0;
}

scl_error_t scl_parse_csv_feed(scl_parse_csv_t *parser, const char *data,
                                size_t len) {
  if (scl_unlikely(!parser))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!data && len > 0))
    return SCL_ERR_NULL_PTR;
  if (parser->eof)
    return SCL_ERR_SIZE_OVERFLOW;

  /* Reject if total rows already at cap. */
  if (parser->max_rows && parser->total_rows >= parser->max_rows)
    return SCL_ERR_SIZE_OVERFLOW;

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

/*
 * Iteration contract
 * ──────────────────
 *   do {
 *       while (scl_parse_csv_next_field(&p, &f, &n) == SCL_OK) { use f[0..n) }
 *   } while (scl_parse_csv_next_row(&p) == SCL_OK);
 *
 * next_field returns one field of the current record and SCL_ERR_EMPTY once the
 * record is exhausted; next_row consumes the record terminator and returns
 * SCL_ERR_EMPTY at end of input. Field pointers alias the parser's buffer and
 * stay valid until the next feed()/destroy(). For records that contain quoted
 * fields with embedded newlines, drain the fields with next_field before
 * calling next_row (next_row's terminator scan does not look inside quotes).
 */

/* Parse one field starting at parser->pos. Quoted fields are unescaped in
 * place ("" -> a single "). Leaves pos at the terminator (',', CR, LF or EOF)
 * without consuming it. The output always lies within [original_start, pos),
 * so it can never point past the populated buffer. */
static void csv_parse_field(scl_parse_csv_t *p, const char **out,
                             size_t *out_len) {
  char *buf = p->buffer;
  size_t len = p->buffer_len;
  size_t pos = p->pos;

  if (pos < len && buf[pos] == '"') {
    pos++; /* skip opening quote */
    size_t content0 = pos, w = pos, run = pos;
    int closed = 0;
    while (pos < len) {
      if (buf[pos] == '"') {
        if (pos + 1 < len && buf[pos + 1] == '"') {
          if (pos > run) {
            scl_memmove(buf + w, buf + run, pos - run);
            w += pos - run;
          }
          buf[w++] = '"'; /* "" collapses to one quote */
          pos += 2;
          run = pos;
        } else {
          if (pos > run) {
            scl_memmove(buf + w, buf + run, pos - run);
            w += pos - run;
          }
          pos++; /* consume closing quote */
          closed = 1;
          break;
        }
      } else {
        pos++;
      }
    }
    if (!closed && pos > run) {
      scl_memmove(buf + w, buf + run, pos - run);
      w += pos - run;
    }
    /* Skip any stray bytes between a closing quote and the terminator. */
    if (closed)
      while (pos < len && buf[pos] != ',' && buf[pos] != '\r' &&
             buf[pos] != '\n')
        pos++;
    *out = buf + content0;
    *out_len = w - content0;
    p->pos = pos;
    return;
  }

  size_t start = pos;
  while (pos < len && buf[pos] != ',' && buf[pos] != '\r' && buf[pos] != '\n')
    pos++;
  *out = buf + start;
  *out_len = pos - start;
  p->pos = pos;
}

scl_error_t scl_parse_csv_next_field(scl_parse_csv_t *parser, const char **out,
                                      size_t *out_len) {
  if (scl_unlikely(!parser || !out || !out_len))
    return SCL_ERR_NULL_PTR;
  if (parser->pos >= parser->buffer_len)
    return SCL_ERR_EMPTY;

  /* If the previous row was completed, reset field count for the new row. */
  if (!parser->row_started)
    parser->field_count = 0;

  char c = parser->buffer[parser->pos];

  if (!parser->row_started) {
    /* Start of a record: a bare line terminator means an empty line, which
     * carries no fields. */
    if (c == '\r' || c == '\n')
      return SCL_ERR_EMPTY;
    parser->row_started = 1;
    parser->total_rows++;
    csv_parse_field(parser, out, out_len);
    parser->field_count++;
    return SCL_OK;
  }

  /* Mid record. A line terminator ends the record; a delimiter introduces
   * the next field (which may be empty). */
  if (c == '\r' || c == '\n')
    return SCL_ERR_EMPTY;
  if (c == ',')
    parser->pos++;

  /* Enforce per-row field cap. Return SCL_ERR_SIZE_OVERFLOW if the caller
   * tries to read more fields than max_fields allows. This prevents a
   * maliciously wide row from exhausting memory. */
  if (parser->max_fields && parser->field_count >= parser->max_fields)
    return SCL_ERR_SIZE_OVERFLOW;

  csv_parse_field(parser, out, out_len);
  parser->field_count++;
  return SCL_OK;
}

scl_error_t scl_parse_csv_next_row(scl_parse_csv_t *parser) {
  if (scl_unlikely(!parser))
    return SCL_ERR_NULL_PTR;
  char *buf = parser->buffer;
  size_t len = parser->buffer_len;

  /* Skip to the current record's terminator (the caller is expected to have
   * drained the fields already, in which case pos is already there). */
  while (parser->pos < len && buf[parser->pos] != '\r' &&
         buf[parser->pos] != '\n')
    parser->pos++;

  if (parser->pos >= len) {
    parser->eof = 1;
    return SCL_ERR_EMPTY; /* no terminator: end of input */
  }

  if (buf[parser->pos] == '\r') {
    parser->pos++;
    if (parser->pos < len && buf[parser->pos] == '\n')
      parser->pos++; /* CRLF */
  } else {
    parser->pos++; /* LF */
  }
  parser->state = SCL_CSV_STATE_FIELD_START;
  parser->row_started = 0;

  /* Enforce total row cap. */
  if (parser->max_rows && parser->total_rows >= parser->max_rows) {
    parser->eof = 1;
    return SCL_ERR_SIZE_OVERFLOW;
  }

  /* A terminator at the very end of input is not a new record. */
  return parser->pos < len ? SCL_OK : SCL_ERR_EMPTY;
}

scl_error_t scl_parse_csv_destroy(scl_parse_csv_t *parser) {
  if (scl_unlikely(!parser))
    return SCL_ERR_NULL_PTR;
  scl_free(parser->alloc, parser->buffer);
  parser->buffer = NULL;
  parser->buffer_cap = 0;
  parser->buffer_len = 0;
  parser->pos = 0;
  parser->total_rows = 0;
  parser->field_count = 0;
  parser->eof = 0;
  return SCL_OK;
}