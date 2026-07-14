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

/* TSV streaming parser. Tab-delimited, escape handling. Row-by-row callback,
 * zero inter-row buffering. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_tsv.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#define TSV_INIT_BUF 4096

scl_error_t scl_parse_tsv_init(scl_allocator_t *alloc,
                                scl_parse_tsv_t *parser) {
  return scl_parse_tsv_init_with_limits(alloc, parser, 0, 0);
}

scl_error_t scl_parse_tsv_init_with_limits(scl_allocator_t *alloc,
                                            scl_parse_tsv_t *parser,
                                            size_t max_fields_per_row,
                                            size_t max_rows) {
  if (scl_unlikely(!parser))
    return SCL_ERR_NULL_PTR;
  parser->alloc = alloc;
  parser->state = SCL_TSV_STATE_FIELD_START;
  parser->max_fields = max_fields_per_row ? max_fields_per_row : SCL_TSV_MAX_FIELDS;
  parser->max_rows = max_rows ? max_rows : SCL_TSV_MAX_ROWS;
  parser->buffer =
      (char *)scl_alloc(alloc, TSV_INIT_BUF, _Alignof(max_align_t));
  if (scl_unlikely(!parser->buffer))
    return SCL_ERR_OUT_OF_MEMORY;
  parser->buffer_cap = TSV_INIT_BUF;
  parser->buffer_len = 0;
  parser->pos = 0;
  parser->row_started = 0;
  parser->eof = 0;
  parser->total_rows = 0;
  parser->field_count = 0;
  return SCL_OK;
}

static int tsv_ensure_buf(scl_parse_tsv_t *parser, size_t needed) {
  if (needed <= parser->buffer_cap)
    return 0;
  /* Cap buffer growth to max_rows * max_fields as a coarse heuristic for
   * legitimate TSV size. This prevents a malicious feed from growing the
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

scl_error_t scl_parse_tsv_feed(scl_parse_tsv_t *parser, const char *data,
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
  if (tsv_ensure_buf(parser, need) != 0)
    return SCL_ERR_OUT_OF_MEMORY;

  scl_memcpy(parser->buffer + parser->buffer_len, data, len);
  parser->buffer_len += len;
  parser->buffer[parser->buffer_len] = '\0';
  return SCL_OK;
}

/*
 * Iteration contract mirrors the CSV parser:
 *
 *   do {
 *       while (scl_parse_tsv_next_field(&p, &f, &n) == SCL_OK) { use f[0..n) }
 *   } while (scl_parse_tsv_next_row(&p) == SCL_OK);
 *
 * Backslash escapes inside a field are decoded in place per the IANA
 * text/tab-separated-values convention: \t \n \r \\ -> TAB LF CR backslash.
 */

/* Parse one field at parser->pos, decoding backslash escapes in place. Leaves
 * pos at the terminator (TAB, CR, LF or EOF) without consuming it. */
static void tsv_parse_field(scl_parse_tsv_t *p, const char **out,
                             size_t *out_len) {
  char *buf = p->buffer;
  size_t len = p->buffer_len;
  size_t pos = p->pos;
  size_t start = pos, w = pos; /* w <= pos: decoding only shrinks */

  while (pos < len) {
    char c = buf[pos];
    if (c == '\t' || c == '\n' || c == '\r')
      break;
    if (c == '\\' && pos + 1 < len) {
      char dec = 0;
      switch (buf[pos + 1]) {
      case 't':
        dec = '\t';
        break;
      case 'n':
        dec = '\n';
        break;
      case 'r':
        dec = '\r';
        break;
      case '\\':
        dec = '\\';
        break;
      default:
        break;
      }
      if (dec) {
        buf[w++] = dec;
        pos += 2;
        continue;
      }
    }
    buf[w++] = c;
    pos++;
  }
  *out = buf + start;
  *out_len = w - start;
  p->pos = pos;
}

scl_error_t scl_parse_tsv_next_field(scl_parse_tsv_t *parser, const char **out,
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
    if (c == '\r' || c == '\n')
      return SCL_ERR_EMPTY; /* empty line */
    parser->row_started = 1;
    parser->total_rows++;
    tsv_parse_field(parser, out, out_len);
    parser->field_count++;
    return SCL_OK;
  }

  if (c == '\r' || c == '\n')
    return SCL_ERR_EMPTY; /* record exhausted */
  if (c == '\t')
    parser->pos++; /* consume delimiter */

  /* Enforce per-row field cap. */
  if (parser->max_fields && parser->field_count >= parser->max_fields)
    return SCL_ERR_SIZE_OVERFLOW;

  tsv_parse_field(parser, out, out_len);
  parser->field_count++;
  return SCL_OK;
}

scl_error_t scl_parse_tsv_next_row(scl_parse_tsv_t *parser) {
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
    return SCL_ERR_EMPTY;
  }

  if (buf[parser->pos] == '\r') {
    parser->pos++;
    if (parser->pos < len && buf[parser->pos] == '\n')
      parser->pos++; /* CRLF */
  } else {
    parser->pos++; /* LF */
  }
  parser->state = SCL_TSV_STATE_FIELD_START;
  parser->row_started = 0;

  /* Enforce total row cap. */
  if (parser->max_rows && parser->total_rows >= parser->max_rows) {
    parser->eof = 1;
    return SCL_ERR_SIZE_OVERFLOW;
  }

  /* A terminator at the very end of input is not a new record. */
  return parser->pos < len ? SCL_OK : SCL_ERR_EMPTY;
}

scl_error_t scl_parse_tsv_destroy(scl_parse_tsv_t *parser) {
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