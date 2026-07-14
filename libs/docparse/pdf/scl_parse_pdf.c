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

/* PDF 1.7 parser (ISO 32000-1:2008). See scl_parse_pdf.h for the feature
 * matrix. Layout of this file:
 *
 *   1. Bounded cursor + lexical primitives (§7.2 character classes)
 *   2. COS object constructors / destructor
 *   3. Object parser (recursive descent, depth-limited)
 *   4. Stream filters (§7.4) + predictors
 *   5. Cross-reference parsing: tables, streams, /Prev chains, repair scan
 *   6. Indirect object loading (offsets + object streams)
 *   7. Page tree flattening (§7.7.3)
 *   8. Content-stream text extraction (§9.4)
 *   9. Text-string decoding (§7.9.2.2) and public API
 *
 * Everything read from the file is treated as hostile: offsets, lengths and
 * counts are bounds-checked before use, recursion and chains are capped, and
 * decompression output is size-limited. */

#include "scl_parse_pdf.h"
#include "scl_gzip.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

/* ════════════════════════════════════════════════════════════════════
 *  1. Bounded cursor + lexical primitives
 * ════════════════════════════════════════════════════════════════════ */

typedef struct {
  const unsigned char *b;
  size_t n;
  size_t p;
} pdf_cur_t;

static int cur_peek(const pdf_cur_t *c) {
  return c->p < c->n ? c->b[c->p] : -1;
}

static int cur_peek_at(const pdf_cur_t *c, size_t ahead) {
  size_t idx;
  if (scl_add_overflow(c->p, ahead, &idx) || idx >= c->n)
    return -1;
  return c->b[idx];
}

/* PDF whitespace (§7.2.2): NUL, HT, LF, FF, CR, SP. */
static bool pdf_is_ws(int ch) {
  return ch == 0 || ch == 9 || ch == 10 || ch == 12 || ch == 13 || ch == 32;
}

/* PDF delimiters (§7.2.2): ( ) < > [ ] { } / % */
static bool pdf_is_delim(int ch) {
  return ch == '(' || ch == ')' || ch == '<' || ch == '>' || ch == '[' ||
         ch == ']' || ch == '{' || ch == '}' || ch == '/' || ch == '%';
}

static bool pdf_is_regular(int ch) {
  return ch >= 0 && !pdf_is_ws(ch) && !pdf_is_delim(ch);
}

/* Skip whitespace and %-comments (§7.2.3). */
static void pdf_skip_ws(pdf_cur_t *c) {
  for (;;) {
    int ch = cur_peek(c);
    if (pdf_is_ws(ch)) {
      c->p++;
    } else if (ch == '%') {
      while (c->p < c->n && c->b[c->p] != '\n' && c->b[c->p] != '\r')
        c->p++;
    } else {
      return;
    }
  }
}

/* Match `kw` at the cursor; it must be terminated by a non-regular char. */
static bool pdf_match_kw(pdf_cur_t *c, const char *kw) {
  size_t klen = scl_strlen(kw);
  if (!scl_range_in_bounds(c->n, c->p, klen))
    return false;
  if (scl_memcmp(c->b + c->p, kw, klen) != 0)
    return false;
  if (pdf_is_regular(cur_peek_at(c, klen)))
    return false;
  c->p += klen;
  return true;
}

/* Bounded forward substring search; SIZE_MAX when absent. */
static size_t pdf_find(const unsigned char *hay, size_t hay_len, size_t from,
                       const char *needle) {
  size_t nlen = scl_strlen(needle);
  if (nlen == 0 || from >= hay_len || nlen > hay_len - from)
    return SIZE_MAX;
  /* memchr-accelerated scan on the first byte. */
  const unsigned char first = (unsigned char)needle[0];
  size_t i = from;
  while (i + nlen <= hay_len) {
    const unsigned char *hit =
        (const unsigned char *)scl_memchr(hay + i, first, hay_len - nlen - i + 1);
    if (!hit)
      return SIZE_MAX;
    i = (size_t)(hit - hay);
    if (scl_memcmp(hay + i, needle, nlen) == 0)
      return i;
    i++;
  }
  return SIZE_MAX;
}

/* Parse an integer token: [+-]?[0-9]+, clamped to int64 range. */
static bool pdf_parse_int_tok(pdf_cur_t *c, int64_t *out) {
  size_t start = c->p;
  bool neg = false;
  int ch = cur_peek(c);
  if (ch == '+' || ch == '-') {
    neg = (ch == '-');
    c->p++;
  }
  uint64_t acc = 0;
  bool any = false;
  while ((ch = cur_peek(c)) >= '0' && ch <= '9') {
    any = true;
    if (acc < ((uint64_t)1 << 62)) /* clamp instead of overflowing */
      acc = acc * 10 + (uint64_t)(ch - '0');
    c->p++;
  }
  if (!any) {
    c->p = start;
    return false;
  }
  *out = neg ? -(int64_t)acc : (int64_t)acc;
  return true;
}

/* Parse a numeric object token (§7.3.3): integer or real. Reals use the
 * PDF syntax only (no exponents), so we parse manually — this also avoids
 * strtod's locale-dependent decimal point. */
static bool pdf_parse_number_tok(pdf_cur_t *c, bool *is_real, int64_t *ival,
                                 double *rval) {
  size_t start = c->p;
  bool neg = false;
  int ch = cur_peek(c);
  if (ch == '+' || ch == '-') {
    neg = (ch == '-');
    c->p++;
  }
  uint64_t int_acc = 0;
  double val = 0.0;
  bool any = false, real = false;
  while ((ch = cur_peek(c)) >= '0' && ch <= '9') {
    any = true;
    if (int_acc < ((uint64_t)1 << 62))
      int_acc = int_acc * 10 + (uint64_t)(ch - '0');
    val = val * 10.0 + (double)(ch - '0');
    c->p++;
  }
  if (ch == '.') {
    real = true;
    c->p++;
    double scale = 0.1;
    int frac_digits = 0;
    while ((ch = cur_peek(c)) >= '0' && ch <= '9') {
      any = true;
      if (frac_digits < 18) { /* beyond double precision anyway */
        val += (double)(ch - '0') * scale;
        scale *= 0.1;
        frac_digits++;
      }
      c->p++;
    }
  }
  if (!any) {
    c->p = start;
    return false;
  }
  *is_real = real;
  *ival = neg ? -(int64_t)int_acc : (int64_t)int_acc;
  *rval = neg ? -val : val;
  return true;
}

/* ════════════════════════════════════════════════════════════════════
 *  2. COS object constructors / destructor
 * ════════════════════════════════════════════════════════════════════ */

static scl_pdf_obj_t *pdf_obj_new(scl_allocator_t *alloc,
                                  scl_pdf_obj_type_t type) {
  scl_pdf_obj_t *o = (scl_pdf_obj_t *)scl_calloc(alloc, 1, sizeof(*o),
                                                 _Alignof(scl_pdf_obj_t));
  if (o)
    o->type = type;
  return o;
}

/* Recursive free. Safe: parse depth is capped at SCL_PDF_MAX_DEPTH, so the
 * ownership tree can never be deeper than that. NULL children (from
 * ownership transfer during trailer merging) are skipped. */
static void pdf_obj_free(scl_allocator_t *alloc, scl_pdf_obj_t *o) {
  if (!o)
    return;
  switch (o->type) {
  case SCL_PDF_OBJ_STRING:
  case SCL_PDF_OBJ_NAME:
    scl_free(alloc, o->u.string.data);
    break;
  case SCL_PDF_OBJ_ARRAY:
    for (size_t i = 0; i < o->u.array.count; i++)
      pdf_obj_free(alloc, o->u.array.items[i]);
    scl_free(alloc, o->u.array.items);
    break;
  case SCL_PDF_OBJ_DICT:
    for (size_t i = 0; i < o->u.dict.count; i++) {
      scl_free(alloc, o->u.dict.keys[i]);
      pdf_obj_free(alloc, o->u.dict.vals[i]);
    }
    scl_free(alloc, o->u.dict.keys);
    scl_free(alloc, o->u.dict.vals);
    break;
  case SCL_PDF_OBJ_STREAM:
    pdf_obj_free(alloc, o->u.stream.dict);
    break;
  default:
    break;
  }
  scl_free(alloc, o);
}

scl_pdf_obj_t *scl_pdf_dict_get(const scl_pdf_obj_t *obj, const char *key) {
  if (!obj || !key)
    return NULL;
  if (obj->type == SCL_PDF_OBJ_STREAM)
    obj = obj->u.stream.dict;
  if (!obj || obj->type != SCL_PDF_OBJ_DICT)
    return NULL;
  /* Scan from the end: with duplicate keys the last one wins. */
  for (size_t i = obj->u.dict.count; i-- > 0;) {
    if (obj->u.dict.keys[i] && scl_strcmp(obj->u.dict.keys[i], key) == 0)
      return obj->u.dict.vals[i];
  }
  return NULL;
}

/* Convenience getters used throughout. */
static bool pdf_obj_as_int(const scl_pdf_obj_t *o, int64_t *out) {
  if (o && o->type == SCL_PDF_OBJ_INT) {
    *out = o->u.integer;
    return true;
  }
  if (o && o->type == SCL_PDF_OBJ_REAL) {
    *out = (int64_t)o->u.real;
    return true;
  }
  return false;
}

static bool pdf_name_is(const scl_pdf_obj_t *o, const char *name) {
  return o && o->type == SCL_PDF_OBJ_NAME && o->u.string.data &&
         scl_strcmp(o->u.string.data, name) == 0;
}

/* ════════════════════════════════════════════════════════════════════
 *  3. Object parser (recursive descent, §7.3)
 * ════════════════════════════════════════════════════════════════════ */

typedef struct scl_parse_pdf_ctx {
  scl_parse_pdf_t *parser;
  bool in_objstm; /* streams are illegal inside object streams (§7.5.7) */
} pdf_ctx_t;

static scl_error_t pdf_parse_object(pdf_ctx_t *ctx, pdf_cur_t *c, int depth,
                                    scl_pdf_obj_t **out);

/* Grow a pointer array geometrically. */
static scl_error_t pdf_grow_ptrs(scl_allocator_t *alloc, void ***arr,
                                 size_t *cap, size_t need) {
  if (need <= *cap)
    return SCL_OK;
  size_t new_cap = *cap ? *cap * 2 : 8;
  if (new_cap < need)
    new_cap = need;
  size_t bytes, old_bytes;
  if (scl_mul_overflow(new_cap, sizeof(void *), &bytes))
    return SCL_ERR_SIZE_OVERFLOW;
  old_bytes = *cap * sizeof(void *);
  void *nb = scl_realloc(alloc, *arr, old_bytes, bytes, _Alignof(void *));
  if (!nb)
    return SCL_ERR_OUT_OF_MEMORY;
  *arr = (void **)nb;
  *cap = new_cap;
  return SCL_OK;
}

/* Name object (§7.3.5): /Foo with #xx hex escapes. */
static scl_error_t pdf_parse_name(pdf_ctx_t *ctx, pdf_cur_t *c,
                                  scl_pdf_obj_t **out) {
  scl_allocator_t *alloc = ctx->parser->alloc;
  if (cur_peek(c) != '/')
    return SCL_ERR_PARSE;
  c->p++;
  char tmp[1024]; /* spec limit is 127 bytes (Annex C); be lenient */
  size_t len = 0;
  for (;;) {
    int ch = cur_peek(c);
    if (!pdf_is_regular(ch))
      break;
    if (ch == '#') {
      int h1 = cur_peek_at(c, 1), h2 = cur_peek_at(c, 2);
      if (h1 >= 0 && h2 >= 0 && scl_isxdigit((unsigned char)h1) &&
          scl_isxdigit((unsigned char)h2)) {
        int v1 = scl_isdigit((unsigned char)h1) ? h1 - '0'
                                                : (scl_tolower(h1) - 'a' + 10);
        int v2 = scl_isdigit((unsigned char)h2) ? h2 - '0'
                                                : (scl_tolower(h2) - 'a' + 10);
        ch = (v1 << 4) | v2;
        c->p += 3;
      } else {
        c->p++; /* lone '#': keep literally (lenient) */
      }
    } else {
      c->p++;
    }
    if (len + 1 >= sizeof(tmp))
      return SCL_ERR_PARSE; /* name too long: hostile input */
    tmp[len++] = (char)ch;
  }
  scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_NAME);
  if (!o)
    return SCL_ERR_OUT_OF_MEMORY;
  o->u.string.data = (char *)scl_alloc(alloc, len + 1, 1);
  if (!o->u.string.data) {
    scl_free(alloc, o);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  scl_memcpy(o->u.string.data, tmp, len);
  o->u.string.data[len] = '\0';
  o->u.string.len = len;
  *out = o;
  return SCL_OK;
}

/* Append one byte to a growing string buffer. */
static scl_error_t pdf_str_push(scl_allocator_t *alloc, char **buf,
                                size_t *len, size_t *cap, int ch) {
  if (*len + 1 >= *cap) {
    if (*len + 1 >= SCL_PDF_MAX_STRING_LEN)
      return SCL_ERR_SIZE_OVERFLOW;
    size_t new_cap = *cap ? *cap * 2 : 32;
    void *nb = scl_realloc(alloc, *buf, *cap, new_cap, 1);
    if (!nb)
      return SCL_ERR_OUT_OF_MEMORY;
    *buf = (char *)nb;
    *cap = new_cap;
  }
  (*buf)[(*len)++] = (char)ch;
  return SCL_OK;
}

/* Literal string (§7.3.4.2): ( ... ) with backslash escapes and balanced
 * nested parentheses. */
static scl_error_t pdf_parse_lit_string(pdf_ctx_t *ctx, pdf_cur_t *c,
                                        scl_pdf_obj_t **out) {
  scl_allocator_t *alloc = ctx->parser->alloc;
  if (cur_peek(c) != '(')
    return SCL_ERR_PARSE;
  c->p++;
  char *buf = NULL;
  size_t len = 0, cap = 0;
  int paren_depth = 1;
  scl_error_t err = SCL_OK;

  while (c->p < c->n) {
    int ch = c->b[c->p++];
    if (ch == '\\') {
      int esc = cur_peek(c);
      if (esc < 0)
        break;
      c->p++;
      switch (esc) {
      case 'n':
        err = pdf_str_push(alloc, &buf, &len, &cap, '\n');
        break;
      case 'r':
        err = pdf_str_push(alloc, &buf, &len, &cap, '\r');
        break;
      case 't':
        err = pdf_str_push(alloc, &buf, &len, &cap, '\t');
        break;
      case 'b':
        err = pdf_str_push(alloc, &buf, &len, &cap, '\b');
        break;
      case 'f':
        err = pdf_str_push(alloc, &buf, &len, &cap, '\f');
        break;
      case '(':
      case ')':
      case '\\':
        err = pdf_str_push(alloc, &buf, &len, &cap, esc);
        break;
      case '\r': /* line continuation: \<EOL> consumed */
        if (cur_peek(c) == '\n')
          c->p++;
        break;
      case '\n':
        break;
      default:
        if (esc >= '0' && esc <= '7') { /* \ooo — up to 3 octal digits */
          int v = esc - '0';
          for (int k = 0; k < 2; k++) {
            int d = cur_peek(c);
            if (d < '0' || d > '7')
              break;
            v = (v << 3) | (d - '0');
            c->p++;
          }
          err = pdf_str_push(alloc, &buf, &len, &cap, v & 0xFF);
        } else {
          /* Unknown escape: backslash is dropped per spec. */
          err = pdf_str_push(alloc, &buf, &len, &cap, esc);
        }
        break;
      }
    } else if (ch == '(') {
      paren_depth++;
      if (paren_depth > (int)SCL_PDF_MAX_DEPTH * 8) {
        err = SCL_ERR_PARSE; /* pathological nesting */
      } else {
        err = pdf_str_push(alloc, &buf, &len, &cap, ch);
      }
    } else if (ch == ')') {
      paren_depth--;
      if (paren_depth == 0)
        goto done;
      err = pdf_str_push(alloc, &buf, &len, &cap, ch);
    } else {
      err = pdf_str_push(alloc, &buf, &len, &cap, ch);
    }
    if (err != SCL_OK) {
      scl_free(alloc, buf);
      return err;
    }
  }
  /* EOF before closing ')': hostile/truncated input. */
  scl_free(alloc, buf);
  return SCL_ERR_PARSE;

done:
  if (!buf) { /* empty string () */
    buf = (char *)scl_alloc(alloc, 1, 1);
    if (!buf)
      return SCL_ERR_OUT_OF_MEMORY;
  }
  buf[len] = '\0';
  scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_STRING);
  if (!o) {
    scl_free(alloc, buf);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  o->u.string.data = buf;
  o->u.string.len = len;
  *out = o;
  return SCL_OK;
}

/* Hex string (§7.3.4.3): < hex digits, whitespace ignored, odd count is
 * padded with 0 >. */
static scl_error_t pdf_parse_hex_string(pdf_ctx_t *ctx, pdf_cur_t *c,
                                        scl_pdf_obj_t **out) {
  scl_allocator_t *alloc = ctx->parser->alloc;
  if (cur_peek(c) != '<')
    return SCL_ERR_PARSE;
  c->p++;
  char *buf = NULL;
  size_t len = 0, cap = 0;
  int hi = -1;
  scl_error_t err;

  while (c->p < c->n) {
    int ch = c->b[c->p++];
    if (ch == '>') {
      if (hi >= 0) { /* odd digit count: pad low nibble with 0 */
        err = pdf_str_push(alloc, &buf, &len, &cap, hi << 4);
        if (err != SCL_OK) {
          scl_free(alloc, buf);
          return err;
        }
      }
      if (!buf) {
        buf = (char *)scl_alloc(alloc, 1, 1);
        if (!buf)
          return SCL_ERR_OUT_OF_MEMORY;
      }
      buf[len] = '\0';
      scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_STRING);
      if (!o) {
        scl_free(alloc, buf);
        return SCL_ERR_OUT_OF_MEMORY;
      }
      o->u.string.data = buf;
      o->u.string.len = len;
      *out = o;
      return SCL_OK;
    }
    if (pdf_is_ws(ch))
      continue;
    int v;
    if (ch >= '0' && ch <= '9')
      v = ch - '0';
    else if (ch >= 'a' && ch <= 'f')
      v = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
      v = ch - 'A' + 10;
    else {
      scl_free(alloc, buf);
      return SCL_ERR_PARSE;
    }
    if (hi < 0) {
      hi = v;
    } else {
      err = pdf_str_push(alloc, &buf, &len, &cap, (hi << 4) | v);
      if (err != SCL_OK) {
        scl_free(alloc, buf);
        return err;
      }
      hi = -1;
    }
  }
  scl_free(alloc, buf);
  return SCL_ERR_PARSE; /* EOF before '>' */
}

/* Array (§7.3.6). */
static scl_error_t pdf_parse_array(pdf_ctx_t *ctx, pdf_cur_t *c, int depth,
                                   scl_pdf_obj_t **out) {
  scl_allocator_t *alloc = ctx->parser->alloc;
  if (cur_peek(c) != '[')
    return SCL_ERR_PARSE;
  c->p++;
  scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_ARRAY);
  if (!o)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t cap = 0;
  scl_error_t err = SCL_OK;

  for (;;) {
    pdf_skip_ws(c);
    int ch = cur_peek(c);
    if (ch < 0) {
      err = SCL_ERR_PARSE;
      break;
    }
    if (ch == ']') {
      c->p++;
      *out = o;
      return SCL_OK;
    }
    if (o->u.array.count >= SCL_PDF_MAX_CONTAINER_LEN) {
      err = SCL_ERR_SIZE_OVERFLOW;
      break;
    }
    scl_pdf_obj_t *item = NULL;
    err = pdf_parse_object(ctx, c, depth + 1, &item);
    if (err != SCL_OK)
      break;
    err = pdf_grow_ptrs(alloc, (void ***)&o->u.array.items, &cap,
                        o->u.array.count + 1);
    if (err != SCL_OK) {
      pdf_obj_free(alloc, item);
      break;
    }
    o->u.array.items[o->u.array.count++] = item;
  }
  pdf_obj_free(alloc, o);
  return err;
}

/* Dictionary (§7.3.7) and, when followed by `stream`, a stream (§7.3.8). */
static scl_error_t pdf_parse_dict(pdf_ctx_t *ctx, pdf_cur_t *c, int depth,
                                  scl_pdf_obj_t **out) {
  scl_allocator_t *alloc = ctx->parser->alloc;
  if (cur_peek(c) != '<' || cur_peek_at(c, 1) != '<')
    return SCL_ERR_PARSE;
  c->p += 2;
  scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_DICT);
  if (!o)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t kcap = 0, vcap = 0;
  scl_error_t err = SCL_OK;

  for (;;) {
    pdf_skip_ws(c);
    int ch = cur_peek(c);
    if (ch < 0) {
      err = SCL_ERR_PARSE;
      goto fail;
    }
    if (ch == '>' && cur_peek_at(c, 1) == '>') {
      c->p += 2;
      break;
    }
    if (ch != '/') { /* keys must be names (§7.3.7) */
      err = SCL_ERR_PARSE;
      goto fail;
    }
    if (o->u.dict.count >= SCL_PDF_MAX_CONTAINER_LEN) {
      err = SCL_ERR_SIZE_OVERFLOW;
      goto fail;
    }
    scl_pdf_obj_t *key = NULL, *val = NULL;
    err = pdf_parse_name(ctx, c, &key);
    if (err != SCL_OK)
      goto fail;
    err = pdf_parse_object(ctx, c, depth + 1, &val);
    if (err != SCL_OK) {
      pdf_obj_free(alloc, key);
      goto fail;
    }
    err = pdf_grow_ptrs(alloc, (void ***)&o->u.dict.keys, &kcap,
                        o->u.dict.count + 1);
    if (err == SCL_OK)
      err = pdf_grow_ptrs(alloc, (void ***)&o->u.dict.vals, &vcap,
                          o->u.dict.count + 1);
    if (err != SCL_OK) {
      pdf_obj_free(alloc, key);
      pdf_obj_free(alloc, val);
      goto fail;
    }
    /* Steal the name's payload as the key string. */
    o->u.dict.keys[o->u.dict.count] = key->u.string.data;
    key->u.string.data = NULL;
    pdf_obj_free(alloc, key);
    o->u.dict.vals[o->u.dict.count] = val;
    o->u.dict.count++;
  }

  /* Stream? (§7.3.8): dict followed by the `stream` keyword. */
  {
    size_t save = c->p;
    pdf_skip_ws(c);
    if (scl_range_in_bounds(c->n, c->p, 6) &&
        scl_memcmp(c->b + c->p, "stream", 6) == 0) {
      if (ctx->in_objstm) {
        /* Streams may not live inside object streams (§7.5.7). */
        err = SCL_ERR_PARSE;
        goto fail;
      }
      c->p += 6;
      /* Spec: CRLF or LF after `stream` (not lone CR); accept CR too. */
      if (cur_peek(c) == '\r')
        c->p++;
      if (cur_peek(c) == '\n')
        c->p++;
      size_t data_off = c->p;

      /* Bound the raw data. Trust a direct in-bounds /Length that is
       * followed by `endstream`; otherwise scan for the keyword. */
      size_t data_len = SIZE_MAX;
      scl_pdf_obj_t *len_obj = scl_pdf_dict_get(o, "Length");
      int64_t li;
      if (len_obj && pdf_obj_as_int(len_obj, &li) && li >= 0 &&
          scl_range_in_bounds(c->n, data_off, (size_t)li)) {
        pdf_cur_t probe = {c->b, c->n, data_off + (size_t)li};
        pdf_skip_ws(&probe);
        if (scl_range_in_bounds(c->n, probe.p, 9) &&
            scl_memcmp(c->b + probe.p, "endstream", 9) == 0)
          data_len = (size_t)li;
      }
      if (data_len == SIZE_MAX) {
        size_t es = pdf_find(c->b, c->n, data_off, "endstream");
        if (es == SIZE_MAX) {
          err = SCL_ERR_PARSE;
          goto fail;
        }
        data_len = es - data_off;
        /* Trim the EOL that belongs to `endstream`, not the data. */
        if (data_len > 0 && c->b[data_off + data_len - 1] == '\n')
          data_len--;
        if (data_len > 0 && c->b[data_off + data_len - 1] == '\r')
          data_len--;
      }

      /* Advance past endstream. */
      pdf_cur_t after = {c->b, c->n, data_off + data_len};
      pdf_skip_ws(&after);
      if (scl_range_in_bounds(c->n, after.p, 9) &&
          scl_memcmp(c->b + after.p, "endstream", 9) == 0)
        after.p += 9;
      c->p = after.p;

      scl_pdf_obj_t *s = pdf_obj_new(alloc, SCL_PDF_OBJ_STREAM);
      if (!s) {
        err = SCL_ERR_OUT_OF_MEMORY;
        goto fail;
      }
      s->u.stream.dict = o;
      s->u.stream.data_off = data_off;
      s->u.stream.data_len = data_len;
      *out = s;
      return SCL_OK;
    }
    c->p = save;
  }

  *out = o;
  return SCL_OK;

fail:
  pdf_obj_free(alloc, o);
  return err;
}

/* Any object (§7.3). `depth` guards recursion. */
static scl_error_t pdf_parse_object(pdf_ctx_t *ctx, pdf_cur_t *c, int depth,
                                    scl_pdf_obj_t **out) {
  scl_allocator_t *alloc = ctx->parser->alloc;
  *out = NULL;
  if (depth > SCL_PDF_MAX_DEPTH)
    return SCL_ERR_PARSE;
  pdf_skip_ws(c);
  int ch = cur_peek(c);
  if (ch < 0)
    return SCL_ERR_PARSE;

  switch (ch) {
  case '/':
    return pdf_parse_name(ctx, c, out);
  case '(':
    return pdf_parse_lit_string(ctx, c, out);
  case '[':
    return pdf_parse_array(ctx, c, depth, out);
  case '<':
    if (cur_peek_at(c, 1) == '<')
      return pdf_parse_dict(ctx, c, depth, out);
    return pdf_parse_hex_string(ctx, c, out);
  case 't':
    if (pdf_match_kw(c, "true")) {
      scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_BOOL);
      if (!o)
        return SCL_ERR_OUT_OF_MEMORY;
      o->u.boolean = true;
      *out = o;
      return SCL_OK;
    }
    return SCL_ERR_PARSE;
  case 'f':
    if (pdf_match_kw(c, "false")) {
      scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_BOOL);
      if (!o)
        return SCL_ERR_OUT_OF_MEMORY;
      o->u.boolean = false;
      *out = o;
      return SCL_OK;
    }
    return SCL_ERR_PARSE;
  case 'n':
    if (pdf_match_kw(c, "null")) {
      scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_NULL);
      if (!o)
        return SCL_ERR_OUT_OF_MEMORY;
      *out = o;
      return SCL_OK;
    }
    return SCL_ERR_PARSE;
  default:
    break;
  }

  /* Number — or an indirect reference "N G R" (§7.3.10). */
  bool is_real;
  int64_t ival;
  double rval;
  size_t start = c->p;
  if (!pdf_parse_number_tok(c, &is_real, &ival, &rval))
    return SCL_ERR_PARSE;

  if (!is_real && ival >= 0 && ival <= INT32_MAX) {
    /* Lookahead for "G R". */
    size_t after_num = c->p;
    pdf_skip_ws(c);
    int64_t gen;
    if (pdf_parse_int_tok(c, &gen) && gen >= 0 && gen <= 65535) {
      pdf_skip_ws(c);
      if (cur_peek(c) == 'R' && !pdf_is_regular(cur_peek_at(c, 1))) {
        c->p++;
        scl_pdf_obj_t *o = pdf_obj_new(alloc, SCL_PDF_OBJ_REF);
        if (!o)
          return SCL_ERR_OUT_OF_MEMORY;
        o->u.ref.num = (int)ival;
        o->u.ref.gen = (int)gen;
        *out = o;
        return SCL_OK;
      }
    }
    c->p = after_num;
  }
  (void)start;

  scl_pdf_obj_t *o =
      pdf_obj_new(alloc, is_real ? SCL_PDF_OBJ_REAL : SCL_PDF_OBJ_INT);
  if (!o)
    return SCL_ERR_OUT_OF_MEMORY;
  if (is_real)
    o->u.real = rval;
  else
    o->u.integer = ival;
  *out = o;
  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  4. Stream filters (§7.4)
 * ════════════════════════════════════════════════════════════════════ */

/* ASCIIHexDecode (§7.4.2). */
static scl_error_t pdf_ahx_decode(scl_allocator_t *alloc,
                                  const unsigned char *src, size_t len,
                                  unsigned char **out, size_t *out_len) {
  unsigned char *buf = (unsigned char *)scl_alloc(alloc, len / 2 + 1, 1);
  if (!buf)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t n = 0;
  int hi = -1;
  for (size_t i = 0; i < len; i++) {
    int ch = src[i];
    if (ch == '>')
      break;
    if (pdf_is_ws(ch))
      continue;
    int v;
    if (ch >= '0' && ch <= '9')
      v = ch - '0';
    else if (ch >= 'a' && ch <= 'f')
      v = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
      v = ch - 'A' + 10;
    else {
      scl_free(alloc, buf);
      return SCL_ERR_PARSE;
    }
    if (hi < 0)
      hi = v;
    else {
      buf[n++] = (unsigned char)((hi << 4) | v);
      hi = -1;
    }
  }
  if (hi >= 0)
    buf[n++] = (unsigned char)(hi << 4); /* odd count: pad with 0 */
  *out = buf;
  *out_len = n;
  return SCL_OK;
}

/* ASCII85Decode (§7.4.3). */
static scl_error_t pdf_a85_decode(scl_allocator_t *alloc,
                                  const unsigned char *src, size_t len,
                                  unsigned char **out, size_t *out_len) {
  /* Worst case is 4 output bytes per 5-char group, but each 'z' short
   * form emits 4 bytes from a single char — count them up front so a
   * 'z'-heavy (all-zero) stream isn't rejected as oversized. */
  size_t zcount = 0;
  for (size_t k = 0; k < len; k++)
    if (src[k] == 'z')
      zcount++;
  size_t cap = (len / 5 + 1 + zcount) * 4 + 4;
  unsigned char *buf = (unsigned char *)scl_alloc(alloc, cap, 1);
  if (!buf)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t n = 0;
  uint32_t group = 0;
  int gcount = 0;
  size_t i = 0;
  /* Optional <~ prefix (Adobe convention, not in ISO 32000). */
  if (len >= 2 && src[0] == '<' && src[1] == '~')
    i = 2;
  for (; i < len; i++) {
    int ch = src[i];
    if (pdf_is_ws(ch))
      continue;
    if (ch == '~') /* ~> EOD */
      break;
    if (ch == 'z' && gcount == 0) {
      if (n + 4 > cap)
        goto bad;
      buf[n] = buf[n + 1] = buf[n + 2] = buf[n + 3] = 0;
      n += 4;
      continue;
    }
    if (ch < '!' || ch > 'u')
      goto bad;
    /* group * 85 + digit, checked against uint32 wrap. */
    if (group > (0xFFFFFFFFu - (uint32_t)(ch - '!')) / 85)
      goto bad;
    group = group * 85 + (uint32_t)(ch - '!');
    if (++gcount == 5) {
      if (n + 4 > cap)
        goto bad;
      buf[n++] = (unsigned char)(group >> 24);
      buf[n++] = (unsigned char)(group >> 16);
      buf[n++] = (unsigned char)(group >> 8);
      buf[n++] = (unsigned char)group;
      group = 0;
      gcount = 0;
    }
  }
  if (gcount == 1)
    goto bad; /* a single trailing char is invalid */
  if (gcount > 1) {
    /* Pad with 'u' (84) and keep gcount-1 output bytes. */
    for (int k = gcount; k < 5; k++) {
      if (group > (0xFFFFFFFFu - 84u) / 85)
        goto bad;
      group = group * 85 + 84;
    }
    if (n + (size_t)(gcount - 1) > cap)
      goto bad;
    for (int k = 0; k < gcount - 1; k++)
      buf[n++] = (unsigned char)(group >> (24 - 8 * k));
  }
  *out = buf;
  *out_len = n;
  return SCL_OK;
bad:
  scl_free(alloc, buf);
  return SCL_ERR_PARSE;
}

/* RunLengthDecode (§7.4.5). */
static scl_error_t pdf_rl_decode(scl_allocator_t *alloc,
                                 const unsigned char *src, size_t len,
                                 unsigned char **out, size_t *out_len,
                                 size_t max_out) {
  unsigned char *buf = NULL;
  size_t n = 0, cap = 0;
  size_t i = 0;
  while (i < len) {
    unsigned l = src[i++];
    if (l == 128)
      break; /* EOD */
    size_t run = (l < 128) ? (size_t)l + 1 : 257 - (size_t)l;
    size_t need = n + run;
    if (need > max_out) {
      scl_free(alloc, buf);
      return SCL_ERR_SIZE_OVERFLOW;
    }
    if (need > cap) {
      size_t new_cap = cap ? cap * 2 : 256;
      while (new_cap < need)
        new_cap *= 2;
      void *nb = scl_realloc(alloc, buf, cap, new_cap, 1);
      if (!nb) {
        scl_free(alloc, buf);
        return SCL_ERR_OUT_OF_MEMORY;
      }
      buf = (unsigned char *)nb;
      cap = new_cap;
    }
    if (l < 128) {
      if (i + run > len) {
        scl_free(alloc, buf);
        return SCL_ERR_PARSE;
      }
      scl_memcpy(buf + n, src + i, run);
      i += run;
    } else {
      if (i >= len) {
        scl_free(alloc, buf);
        return SCL_ERR_PARSE;
      }
      scl_memset(buf + n, src[i++], run);
    }
    n += run;
  }
  if (!buf) {
    buf = (unsigned char *)scl_alloc(alloc, 1, 1);
    if (!buf)
      return SCL_ERR_OUT_OF_MEMORY;
  }
  *out = buf;
  *out_len = n;
  return SCL_OK;
}

/* LZWDecode (§7.4.4): 9→12-bit variable-width codes, MSB-first. */
#define PDF_LZW_TABLE 4096
#define PDF_LZW_CLEAR 256
#define PDF_LZW_EOD 257

static scl_error_t pdf_lzw_decode(scl_allocator_t *alloc,
                                  const unsigned char *src, size_t len,
                                  unsigned char **out, size_t *out_len,
                                  int early_change, size_t max_out) {
  uint16_t *prefix = (uint16_t *)scl_alloc(
      alloc, PDF_LZW_TABLE * sizeof(uint16_t), _Alignof(uint16_t));
  unsigned char *suffix = (unsigned char *)scl_alloc(alloc, PDF_LZW_TABLE, 1);
  unsigned char *first = (unsigned char *)scl_alloc(alloc, PDF_LZW_TABLE, 1);
  unsigned char *stack = (unsigned char *)scl_alloc(alloc, PDF_LZW_TABLE, 1);
  unsigned char *buf = NULL;
  size_t n = 0, cap = 0;
  scl_error_t err = SCL_ERR_PARSE;

  if (!prefix || !suffix || !first || !stack) {
    err = SCL_ERR_OUT_OF_MEMORY;
    goto out;
  }
  for (unsigned k = 0; k < 256; k++) {
    prefix[k] = 0xFFFF;
    suffix[k] = (unsigned char)k;
    first[k] = (unsigned char)k;
  }

  {
    size_t bitpos = 0;
    unsigned width = 9;
    unsigned next = PDF_LZW_EOD + 1;
    int prev = -1;
    size_t total_bits = len * 8;

    for (;;) {
      if (bitpos + width > total_bits)
        break; /* ran out of input: treat as EOD (lenient) */
      unsigned code = 0;
      for (unsigned k = 0; k < width; k++) {
        size_t bp = bitpos + k;
        code = (code << 1) | ((src[bp >> 3] >> (7 - (bp & 7))) & 1u);
      }
      bitpos += width;

      if (code == PDF_LZW_CLEAR) {
        width = 9;
        next = PDF_LZW_EOD + 1;
        prev = -1;
        continue;
      }
      if (code == PDF_LZW_EOD)
        break;

      unsigned emit_code;
      unsigned char kchar;
      if (code < next && code != PDF_LZW_CLEAR && code != PDF_LZW_EOD) {
        emit_code = code;
        kchar = first[code];
      } else if (code == next && prev >= 0) {
        emit_code = (unsigned)prev; /* KwKwK case */
        kchar = first[prev];
      } else {
        goto out; /* code beyond table: corrupt stream */
      }

      /* Expand emit_code by walking the prefix chain (acyclic: prefixes
       * always reference earlier entries). */
      size_t sp = 0;
      unsigned cc = emit_code;
      while (cc != 0xFFFF && sp < PDF_LZW_TABLE) {
        stack[sp++] = suffix[cc];
        cc = (cc < 256) ? 0xFFFF : prefix[cc];
      }
      size_t run = sp + (code == next ? 1 : 0);
      size_t need = n + run;
      if (need > max_out) {
        err = SCL_ERR_SIZE_OVERFLOW;
        goto out;
      }
      if (need > cap) {
        size_t new_cap = cap ? cap * 2 : 1024;
        while (new_cap < need)
          new_cap *= 2;
        void *nb = scl_realloc(alloc, buf, cap, new_cap, 1);
        if (!nb) {
          err = SCL_ERR_OUT_OF_MEMORY;
          goto out;
        }
        buf = (unsigned char *)nb;
        cap = new_cap;
      }
      while (sp > 0)
        buf[n++] = stack[--sp];
      if (code == next)
        buf[n++] = kchar;

      if (prev >= 0 && next < PDF_LZW_TABLE) {
        prefix[next] = (uint16_t)prev;
        suffix[next] = kchar;
        first[next] = first[prev];
        next++;
      }
      prev = (int)code;
      /* Width bump (EarlyChange=1 bumps one code early, §7.4.4.2). */
      if (next + (unsigned)early_change >= (1u << width) && width < 12)
        width++;
    }
  }

  if (!buf) {
    buf = (unsigned char *)scl_alloc(alloc, 1, 1);
    if (!buf) {
      err = SCL_ERR_OUT_OF_MEMORY;
      goto out;
    }
  }
  *out = buf;
  *out_len = n;
  buf = NULL;
  err = SCL_OK;
out:
  scl_free(alloc, buf);
  scl_free(alloc, prefix);
  scl_free(alloc, suffix);
  scl_free(alloc, first);
  scl_free(alloc, stack);
  return err;
}

/* Predictor post-processing (§7.4.4.4): PNG predictors 10–15 and TIFF
 * predictor 2. Operates in place on the decoded buffer; PNG output is
 * one byte per row shorter than input (the filter-type byte). */
static int pdf_paeth(int a, int b, int c) {
  int p = a + b - c;
  int pa = p > a ? p - a : a - p;
  int pb = p > b ? p - b : b - p;
  int pc = p > c ? p - c : c - p;
  if (pa <= pb && pa <= pc)
    return a;
  return pb <= pc ? b : c;
}

static scl_error_t pdf_apply_predictor(scl_allocator_t *alloc, int predictor,
                                       int colors, int bpc, int columns,
                                       unsigned char **data, size_t *len) {
  if (predictor <= 1)
    return SCL_OK;
  if (colors < 1 || colors > 64)
    return SCL_ERR_PARSE;
  if (bpc != 1 && bpc != 2 && bpc != 4 && bpc != 8 && bpc != 16)
    return SCL_ERR_PARSE;
  if (columns < 1 || columns > (1 << 24))
    return SCL_ERR_PARSE;

  size_t bits;
  if (scl_mul_overflow((size_t)colors * (size_t)bpc, (size_t)columns, &bits))
    return SCL_ERR_SIZE_OVERFLOW;
  size_t rowlen = (bits + 7) / 8;
  if (rowlen == 0 || rowlen > SCL_PDF_MAX_STREAM_SIZE)
    return SCL_ERR_PARSE;
  size_t bpp = ((size_t)colors * (size_t)bpc + 7) / 8;

  if (predictor == 2) {
    /* TIFF horizontal differencing; only the 8-bit case is common. */
    if (bpc != 8)
      return SCL_ERR_UNSUPPORTED;
    unsigned char *d = *data;
    size_t rows = *len / rowlen;
    for (size_t r = 0; r < rows; r++) {
      unsigned char *row = d + r * rowlen;
      for (size_t i = bpp; i < rowlen; i++)
        row[i] = (unsigned char)(row[i] + row[i - bpp]);
    }
    return SCL_OK;
  }
  if (predictor < 10 || predictor > 15)
    return SCL_ERR_UNSUPPORTED;

  /* PNG predictors: stride = rowlen + 1 (leading filter-type byte). */
  size_t stride = rowlen + 1;
  size_t rows = *len / stride;
  if (rows == 0) {
    *len = 0;
    return SCL_OK;
  }
  unsigned char *out = (unsigned char *)scl_alloc(alloc, rows * rowlen, 1);
  if (!out)
    return SCL_ERR_OUT_OF_MEMORY;
  const unsigned char *in = *data;
  unsigned char *prev_row = NULL;

  for (size_t r = 0; r < rows; r++) {
    unsigned ft = in[r * stride];
    const unsigned char *src = in + r * stride + 1;
    unsigned char *dst = out + r * rowlen;
    for (size_t i = 0; i < rowlen; i++) {
      int left = i >= bpp ? dst[i - bpp] : 0;
      int up = prev_row ? prev_row[i] : 0;
      int ul = (prev_row && i >= bpp) ? prev_row[i - bpp] : 0;
      int x = src[i];
      switch (ft) {
      case 0:
        break;
      case 1:
        x += left;
        break;
      case 2:
        x += up;
        break;
      case 3:
        x += (left + up) / 2;
        break;
      case 4:
        x += pdf_paeth(left, up, ul);
        break;
      default:
        scl_free(alloc, out);
        return SCL_ERR_PARSE;
      }
      dst[i] = (unsigned char)x;
    }
    prev_row = dst;
  }
  scl_free(alloc, *data);
  *data = out;
  *len = rows * rowlen;
  return SCL_OK;
}

/* Forward declarations used by the filter pipeline. */
static scl_error_t pdf_get_object_internal(scl_parse_pdf_t *parser, int num,
                                           scl_pdf_obj_t **out);

/* Cache sentinel marking an object whose load is in progress — a second
 * request for it while loading means a reference cycle in the file. */
static scl_pdf_obj_t pdf_loading_sentinel;

/* Resolve an object that may be an indirect reference. Returns NULL on
 * unresolvable refs (callers treat as absent). */
static scl_pdf_obj_t *pdf_resolve(scl_parse_pdf_t *parser, scl_pdf_obj_t *o) {
  for (int hops = 0; o && o->type == SCL_PDF_OBJ_REF && hops < 32; hops++) {
    scl_pdf_obj_t *next = NULL;
    if (pdf_get_object_internal(parser, o->u.ref.num, &next) != SCL_OK)
      return NULL;
    o = next;
  }
  return (o && o->type == SCL_PDF_OBJ_REF) ? NULL : o;
}

/* Decode a stream through its /Filter chain. Does NOT check /Encrypt —
 * xref streams and object streams are never encrypted; the public wrapper
 * enforces the encryption policy for document data. */
static scl_error_t pdf_stream_decode(scl_parse_pdf_t *parser,
                                     scl_pdf_obj_t *stream,
                                     unsigned char **out, size_t *out_len) {
  scl_allocator_t *alloc = parser->alloc;
  if (!stream || stream->type != SCL_PDF_OBJ_STREAM)
    return SCL_ERR_INVALID_ARG;
  scl_pdf_obj_t *dict = stream->u.stream.dict;

  /* Raw extent: prefer /Length (resolving refs), fall back to the scan
   * length captured at parse time. */
  size_t raw_off = stream->u.stream.data_off;
  size_t raw_len = stream->u.stream.data_len;
  scl_pdf_obj_t *len_obj = pdf_resolve(parser, scl_pdf_dict_get(dict, "Length"));
  int64_t li;
  if (len_obj && pdf_obj_as_int(len_obj, &li) && li >= 0 &&
      scl_range_in_bounds(parser->buf_size, raw_off, (size_t)li))
    raw_len = (size_t)li;
  if (!scl_range_in_bounds(parser->buf_size, raw_off, raw_len))
    return SCL_ERR_PARSE;

  /* Collect the filter chain (name or array of names). */
  scl_pdf_obj_t *filters[SCL_PDF_MAX_FILTERS];
  scl_pdf_obj_t *parms[SCL_PDF_MAX_FILTERS];
  size_t nfilters = 0;
  scl_pdf_obj_t *f = pdf_resolve(parser, scl_pdf_dict_get(dict, "Filter"));
  scl_pdf_obj_t *dp = pdf_resolve(parser, scl_pdf_dict_get(dict, "DecodeParms"));
  if (!dp)
    dp = pdf_resolve(parser, scl_pdf_dict_get(dict, "DP")); /* §7.3.8 alias */

  if (f && f->type == SCL_PDF_OBJ_NAME) {
    filters[0] = f;
    parms[0] = dp;
    nfilters = 1;
  } else if (f && f->type == SCL_PDF_OBJ_ARRAY) {
    if (f->u.array.count > SCL_PDF_MAX_FILTERS)
      return SCL_ERR_UNSUPPORTED;
    for (size_t i = 0; i < f->u.array.count; i++) {
      filters[nfilters] = pdf_resolve(parser, f->u.array.items[i]);
      parms[nfilters] =
          (dp && dp->type == SCL_PDF_OBJ_ARRAY && i < dp->u.array.count)
              ? pdf_resolve(parser, dp->u.array.items[i])
              : NULL;
      nfilters++;
    }
  }

  /* Run the cascade. `cur` starts as a borrowed view of the file buffer. */
  unsigned char *cur = (unsigned char *)parser->buf + raw_off;
  size_t cur_len = raw_len;
  bool owned = false;
  scl_error_t err = SCL_OK;

  for (size_t i = 0; i < nfilters && err == SCL_OK; i++) {
    scl_pdf_obj_t *fn = filters[i];
    if (!fn || fn->type != SCL_PDF_OBJ_NAME) {
      err = SCL_ERR_PARSE;
      break;
    }
    const char *name = fn->u.string.data;
    unsigned char *next = NULL;
    size_t next_len = 0;

    if (scl_strcmp(name, "FlateDecode") == 0 || scl_strcmp(name, "Fl") == 0) {
      void *o = NULL;
      err = scl_zlib_decompress(alloc, cur, cur_len, &o, &next_len,
                                SCL_PDF_MAX_STREAM_SIZE);
      next = (unsigned char *)o;
    } else if (scl_strcmp(name, "LZWDecode") == 0 ||
               scl_strcmp(name, "LZW") == 0) {
      int early = 1;
      scl_pdf_obj_t *ec =
          pdf_resolve(parser, scl_pdf_dict_get(parms[i], "EarlyChange"));
      int64_t ecv;
      if (ec && pdf_obj_as_int(ec, &ecv) && (ecv == 0 || ecv == 1))
        early = (int)ecv;
      err = pdf_lzw_decode(alloc, cur, cur_len, &next, &next_len, early,
                           SCL_PDF_MAX_STREAM_SIZE);
    } else if (scl_strcmp(name, "ASCIIHexDecode") == 0 ||
               scl_strcmp(name, "AHx") == 0) {
      err = pdf_ahx_decode(alloc, cur, cur_len, &next, &next_len);
    } else if (scl_strcmp(name, "ASCII85Decode") == 0 ||
               scl_strcmp(name, "A85") == 0) {
      err = pdf_a85_decode(alloc, cur, cur_len, &next, &next_len);
    } else if (scl_strcmp(name, "RunLengthDecode") == 0 ||
               scl_strcmp(name, "RL") == 0) {
      err = pdf_rl_decode(alloc, cur, cur_len, &next, &next_len,
                          SCL_PDF_MAX_STREAM_SIZE);
    } else if (scl_strcmp(name, "Crypt") == 0) {
      /* /Crypt with /Name /Identity is a no-op (§7.4.10). */
      scl_pdf_obj_t *cn =
          pdf_resolve(parser, scl_pdf_dict_get(parms[i], "Name"));
      if (cn && !pdf_name_is(cn, "Identity")) {
        err = SCL_ERR_UNSUPPORTED;
        break;
      }
      continue; /* keep cur as-is */
    } else {
      /* DCTDecode, JPXDecode, CCITTFaxDecode, JBIG2Decode, unknown. */
      err = SCL_ERR_UNSUPPORTED;
      break;
    }

    if (err == SCL_OK &&
        (scl_strcmp(name, "FlateDecode") == 0 || scl_strcmp(name, "Fl") == 0 ||
         scl_strcmp(name, "LZWDecode") == 0 || scl_strcmp(name, "LZW") == 0)) {
      /* Predictor applies to Flate and LZW output only (§7.4.4.4). */
      scl_pdf_obj_t *pr =
          pdf_resolve(parser, scl_pdf_dict_get(parms[i], "Predictor"));
      int64_t pv = 1, cv = 1, bv = 8, colv = 1;
      if (pr && pdf_obj_as_int(pr, &pv) && pv > 1) {
        scl_pdf_obj_t *t;
        t = pdf_resolve(parser, scl_pdf_dict_get(parms[i], "Colors"));
        if (t)
          pdf_obj_as_int(t, &cv);
        t = pdf_resolve(parser, scl_pdf_dict_get(parms[i], "BitsPerComponent"));
        if (t)
          pdf_obj_as_int(t, &bv);
        t = pdf_resolve(parser, scl_pdf_dict_get(parms[i], "Columns"));
        if (t)
          pdf_obj_as_int(t, &colv);
        err = pdf_apply_predictor(alloc, (int)pv, (int)cv, (int)bv, (int)colv,
                                  &next, &next_len);
        if (err != SCL_OK) {
          scl_free(alloc, next);
          next = NULL;
        }
      }
    }

    if (owned)
      scl_free(alloc, cur);
    cur = next;
    cur_len = next_len;
    owned = true;
  }

  if (err != SCL_OK) {
    if (owned)
      scl_free(alloc, cur);
    return err;
  }
  if (!owned) {
    /* Zero filters: hand back a copy so the caller can always scl_free. */
    unsigned char *copy = (unsigned char *)scl_alloc(alloc, cur_len + 1, 1);
    if (!copy)
      return SCL_ERR_OUT_OF_MEMORY;
    scl_memcpy(copy, cur, cur_len);
    copy[cur_len] = '\0';
    cur = copy;
  }
  *out = cur;
  *out_len = cur_len;
  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  5. Cross-reference parsing (§7.5)
 * ════════════════════════════════════════════════════════════════════ */

#define PDF_XREF_UNSET 0xFF /* entry not yet claimed by any section */

/* Grow the xref + object-cache arrays to hold `size` entries. New
 * entries start UNSET so that newest-section-wins merging works. */
static scl_error_t pdf_xref_ensure(scl_parse_pdf_t *parser, size_t size) {
  if (size > SCL_PDF_MAX_OBJECTS)
    size = SCL_PDF_MAX_OBJECTS;
  if (size <= parser->xref_size)
    return SCL_OK;
  size_t bytes, old_bytes, cbytes, old_cbytes;
  if (scl_mul_overflow(size, sizeof(scl_parse_pdf_xref_entry_t), &bytes) ||
      scl_mul_overflow(size, sizeof(scl_pdf_obj_t *), &cbytes))
    return SCL_ERR_SIZE_OVERFLOW;
  old_bytes = parser->xref_size * sizeof(scl_parse_pdf_xref_entry_t);
  old_cbytes = parser->xref_size * sizeof(scl_pdf_obj_t *);

  void *nx = scl_realloc(parser->alloc, parser->xref, old_bytes, bytes,
                         _Alignof(scl_parse_pdf_xref_entry_t));
  if (!nx)
    return SCL_ERR_OUT_OF_MEMORY;
  parser->xref = (scl_parse_pdf_xref_entry_t *)nx;
  void *nc = scl_realloc(parser->alloc, parser->obj_cache, old_cbytes, cbytes,
                         _Alignof(scl_pdf_obj_t *));
  if (!nc)
    return SCL_ERR_OUT_OF_MEMORY;
  parser->obj_cache = (scl_pdf_obj_t **)nc;

  for (size_t i = parser->xref_size; i < size; i++) {
    parser->xref[i].type = PDF_XREF_UNSET;
    parser->xref[i].field2 = 0;
    parser->xref[i].field3 = 0;
    parser->obj_cache[i] = NULL;
  }
  parser->xref_size = size;
  return SCL_OK;
}

/* Record an xref entry. `newest_wins`: entries from newer sections (seen
 * first) must not be overwritten by older /Prev sections. */
static void pdf_xref_set(scl_parse_pdf_t *parser, int64_t num, uint8_t type,
                         uint64_t f2, uint32_t f3, bool overwrite) {
  if (num < 0 || (size_t)num >= parser->xref_size)
    return;
  if (!overwrite && parser->xref[num].type != PDF_XREF_UNSET)
    return;
  parser->xref[num].type = type;
  parser->xref[num].field2 = f2;
  parser->xref[num].field3 = f3;
}

/* Move entries of `src` (a trailer dict) into `merged` unless the key is
 * already present (newest section wins). Moved slots are NULLed in src. */
static scl_error_t pdf_trailer_merge(scl_parse_pdf_t *parser,
                                     scl_pdf_obj_t *merged, size_t *kcap,
                                     size_t *vcap, scl_pdf_obj_t *src) {
  if (!src || src->type != SCL_PDF_OBJ_DICT)
    return SCL_OK;
  scl_allocator_t *alloc = parser->alloc;
  for (size_t i = 0; i < src->u.dict.count; i++) {
    if (!src->u.dict.keys[i])
      continue;
    if (scl_pdf_dict_get(merged, src->u.dict.keys[i]))
      continue;
    scl_error_t err = pdf_grow_ptrs(alloc, (void ***)&merged->u.dict.keys,
                                    kcap, merged->u.dict.count + 1);
    if (err == SCL_OK)
      err = pdf_grow_ptrs(alloc, (void ***)&merged->u.dict.vals, vcap,
                          merged->u.dict.count + 1);
    if (err != SCL_OK)
      return err;
    merged->u.dict.keys[merged->u.dict.count] = src->u.dict.keys[i];
    merged->u.dict.vals[merged->u.dict.count] = src->u.dict.vals[i];
    merged->u.dict.count++;
    src->u.dict.keys[i] = NULL;
    src->u.dict.vals[i] = NULL;
  }
  return SCL_OK;
}

/* Parse a classic xref table (§7.5.4) at the cursor (past "xref").
 * Returns the trailer dict (caller owns) via *trailer_out. */
static scl_error_t pdf_parse_xref_table(scl_parse_pdf_t *parser, pdf_cur_t *c,
                                        scl_pdf_obj_t **trailer_out) {
  pdf_ctx_t ctx = {parser, false};
  size_t total_entries = 0;

  for (;;) {
    pdf_skip_ws(c);
    if (pdf_match_kw(c, "trailer")) {
      pdf_skip_ws(c);
      return pdf_parse_object(&ctx, c, 0, trailer_out);
    }
    int64_t first, count;
    if (!pdf_parse_int_tok(c, &first))
      return SCL_ERR_PARSE;
    pdf_skip_ws(c);
    if (!pdf_parse_int_tok(c, &count))
      return SCL_ERR_PARSE;
    if (first < 0 || count < 0 || first > (int64_t)SCL_PDF_MAX_OBJECTS ||
        count > (int64_t)SCL_PDF_MAX_OBJECTS)
      return SCL_ERR_PARSE;
    total_entries += (size_t)count;
    if (total_entries > SCL_PDF_MAX_OBJECTS)
      return SCL_ERR_PARSE;
    scl_error_t err = pdf_xref_ensure(parser, (size_t)(first + count));
    if (err != SCL_OK)
      return err;

    for (int64_t i = 0; i < count; i++) {
      pdf_skip_ws(c);
      int64_t off, gen;
      if (!pdf_parse_int_tok(c, &off))
        return SCL_ERR_PARSE;
      pdf_skip_ws(c);
      if (!pdf_parse_int_tok(c, &gen))
        return SCL_ERR_PARSE;
      pdf_skip_ws(c);
      int tch = cur_peek(c);
      if (tch != 'n' && tch != 'f')
        return SCL_ERR_PARSE;
      c->p++;
      if (off >= 0 && gen >= 0 && gen <= 65535)
        pdf_xref_set(parser, first + i,
                     tch == 'n' ? SCL_PDF_XREF_OFFSET : SCL_PDF_XREF_FREE,
                     (uint64_t)off, (uint32_t)gen, false);
    }
  }
}

/* Parse an xref *stream* section (§7.5.8) whose stream object has already
 * been parsed. Populates entries; trailer info lives in the stream dict. */
static scl_error_t pdf_parse_xref_stream(scl_parse_pdf_t *parser,
                                         scl_pdf_obj_t *stream) {
  scl_allocator_t *alloc = parser->alloc;
  scl_pdf_obj_t *dict = stream->u.stream.dict;
  scl_pdf_obj_t *w = pdf_resolve(parser, scl_pdf_dict_get(dict, "W"));
  scl_pdf_obj_t *sz = pdf_resolve(parser, scl_pdf_dict_get(dict, "Size"));
  if (!w || w->type != SCL_PDF_OBJ_ARRAY || w->u.array.count < 3 || !sz)
    return SCL_ERR_PARSE;

  int64_t size64;
  if (!pdf_obj_as_int(sz, &size64) || size64 < 0)
    return SCL_ERR_PARSE;
  if (size64 > (int64_t)SCL_PDF_MAX_OBJECTS)
    size64 = SCL_PDF_MAX_OBJECTS;
  scl_error_t err = pdf_xref_ensure(parser, (size_t)size64);
  if (err != SCL_OK)
    return err;

  int64_t w1, w2, w3;
  if (!pdf_obj_as_int(pdf_resolve(parser, w->u.array.items[0]), &w1) ||
      !pdf_obj_as_int(pdf_resolve(parser, w->u.array.items[1]), &w2) ||
      !pdf_obj_as_int(pdf_resolve(parser, w->u.array.items[2]), &w3))
    return SCL_ERR_PARSE;
  if (w1 < 0 || w1 > 8 || w2 < 0 || w2 > 8 || w3 < 0 || w3 > 8 ||
      w1 + w2 + w3 == 0)
    return SCL_ERR_PARSE;
  size_t esize = (size_t)(w1 + w2 + w3);

  /* /Index: [start count ...]; default [0 Size]. */
  int64_t index_pairs[2 * 64];
  size_t npairs = 0;
  scl_pdf_obj_t *idx = pdf_resolve(parser, scl_pdf_dict_get(dict, "Index"));
  if (idx && idx->type == SCL_PDF_OBJ_ARRAY) {
    if (idx->u.array.count > 2 * 64 || (idx->u.array.count & 1))
      return SCL_ERR_PARSE;
    for (size_t i = 0; i < idx->u.array.count; i++) {
      if (!pdf_obj_as_int(pdf_resolve(parser, idx->u.array.items[i]),
                          &index_pairs[i]))
        return SCL_ERR_PARSE;
    }
    npairs = idx->u.array.count / 2;
  } else {
    index_pairs[0] = 0;
    index_pairs[1] = size64;
    npairs = 1;
  }

  unsigned char *data = NULL;
  size_t data_len = 0;
  err = pdf_stream_decode(parser, stream, &data, &data_len);
  if (err != SCL_OK)
    return err;

  size_t pos = 0;
  for (size_t pair = 0; pair < npairs; pair++) {
    int64_t start = index_pairs[2 * pair];
    int64_t count = index_pairs[2 * pair + 1];
    if (start < 0 || count < 0 || count > (int64_t)SCL_PDF_MAX_OBJECTS)
      continue;
    for (int64_t i = 0; i < count; i++) {
      if (!scl_range_in_bounds(data_len, pos, esize))
        goto done; /* stream shorter than /Index promises: stop */
      uint64_t f1 = 0, f2 = 0, f3 = 0;
      for (int64_t k = 0; k < w1; k++)
        f1 = (f1 << 8) | data[pos++];
      for (int64_t k = 0; k < w2; k++)
        f2 = (f2 << 8) | data[pos++];
      for (int64_t k = 0; k < w3; k++)
        f3 = (f3 << 8) | data[pos++];
      if (w1 == 0)
        f1 = 1; /* default type 1 (§7.5.8.3) */
      if (f1 <= 2)
        pdf_xref_set(parser, start + i, (uint8_t)f1, f2,
                     (uint32_t)(f3 & 0xFFFFFFFFu), false);
    }
  }
done:
  scl_free(alloc, data);
  return SCL_OK;
}

/* Load the whole xref chain starting at `offset` (startxref target).
 * Builds parser->trailer as the merged trailer dictionary. */
static scl_error_t pdf_load_xref_chain(scl_parse_pdf_t *parser,
                                       uint64_t offset) {
  scl_allocator_t *alloc = parser->alloc;
  pdf_ctx_t ctx = {parser, false};
  uint64_t visited[SCL_PDF_MAX_XREF_CHAIN];
  size_t nvisited = 0;

  scl_pdf_obj_t *merged = pdf_obj_new(alloc, SCL_PDF_OBJ_DICT);
  if (!merged)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t kcap = 0, vcap = 0;
  scl_error_t err = SCL_OK;

  /* Work queue: at most 2 pending offsets (next /Prev + one /XRefStm). */
  uint64_t queue[2 * SCL_PDF_MAX_XREF_CHAIN];
  size_t qhead = 0, qtail = 0;
  queue[qtail++] = offset;

  while (qhead < qtail && nvisited < SCL_PDF_MAX_XREF_CHAIN) {
    uint64_t off = queue[qhead++];
    if (off >= parser->buf_size) {
      err = SCL_ERR_PARSE;
      break;
    }
    bool seen = false;
    for (size_t i = 0; i < nvisited; i++)
      if (visited[i] == off)
        seen = true;
    if (seen)
      continue; /* loop in /Prev chain */
    visited[nvisited++] = off;

    pdf_cur_t cur = {parser->buf, parser->buf_size, (size_t)off};
    pdf_skip_ws(&cur);
    scl_pdf_obj_t *trailer = NULL;
    scl_pdf_obj_t *xref_stream_obj = NULL;

    if (pdf_match_kw(&cur, "xref")) {
      err = pdf_parse_xref_table(parser, &cur, &trailer);
    } else {
      /* Expect "N G obj" introducing an xref stream. */
      int64_t num, gen;
      if (pdf_parse_int_tok(&cur, &num)) {
        pdf_skip_ws(&cur);
        if (pdf_parse_int_tok(&cur, &gen)) {
          pdf_skip_ws(&cur);
          if (pdf_match_kw(&cur, "obj")) {
            err = pdf_parse_object(&ctx, &cur, 0, &xref_stream_obj);
            if (err == SCL_OK &&
                (!xref_stream_obj ||
                 xref_stream_obj->type != SCL_PDF_OBJ_STREAM))
              err = SCL_ERR_PARSE;
            if (err == SCL_OK)
              err = pdf_parse_xref_stream(parser, xref_stream_obj);
          } else {
            err = SCL_ERR_PARSE;
          }
        } else {
          err = SCL_ERR_PARSE;
        }
      } else {
        err = SCL_ERR_PARSE;
      }
    }
    if (err != SCL_OK) {
      pdf_obj_free(alloc, trailer);
      pdf_obj_free(alloc, xref_stream_obj);
      break;
    }

    scl_pdf_obj_t *tdict =
        trailer ? trailer
                : (xref_stream_obj ? xref_stream_obj->u.stream.dict : NULL);

    /* Queue /XRefStm (hybrid files, §7.5.8.4) and /Prev before merging
     * steals the values. */
    int64_t next_off;
    scl_pdf_obj_t *t = scl_pdf_dict_get(tdict, "XRefStm");
    if (t && pdf_obj_as_int(t, &next_off) && next_off >= 0 &&
        qtail < 2 * SCL_PDF_MAX_XREF_CHAIN)
      queue[qtail++] = (uint64_t)next_off;
    t = scl_pdf_dict_get(tdict, "Prev");
    if (t && pdf_obj_as_int(t, &next_off) && next_off >= 0 &&
        qtail < 2 * SCL_PDF_MAX_XREF_CHAIN)
      queue[qtail++] = (uint64_t)next_off;

    err = pdf_trailer_merge(parser, merged, &kcap, &vcap, tdict);
    pdf_obj_free(alloc, trailer);
    pdf_obj_free(alloc, xref_stream_obj);
    if (err != SCL_OK)
      break;
  }

  if (err != SCL_OK || merged->u.dict.count == 0) {
    pdf_obj_free(alloc, merged);
    return err != SCL_OK ? err : SCL_ERR_PARSE;
  }
  parser->trailer = merged;
  return SCL_OK;
}

/* ── Repair scan ─────────────────────────────────────────────────
 *
 * Rebuild the xref by scanning the raw bytes for "N G obj" headers (the
 * approach every hardened reader falls back to — real-world files have
 * broken offsets remarkably often). Later definitions overwrite earlier
 * ones, matching incremental-update semantics. */
static scl_error_t pdf_repair(scl_parse_pdf_t *parser) {
  scl_allocator_t *alloc = parser->alloc;
  const unsigned char *b = parser->buf;
  size_t n = parser->buf_size;
  parser->repaired = true;

  size_t pos = 0;
  while (pos < n) {
    size_t hit = pdf_find(b, n, pos, "obj");
    if (hit == SIZE_MAX)
      break;
    pos = hit + 3;
    /* The keyword must be delimited on both sides. */
    if (pdf_is_regular(hit + 3 < n ? b[hit + 3] : -1))
      continue;
    /* Walk backwards: ws, gen digits, ws, num digits. */
    size_t q = hit;
    while (q > 0 && pdf_is_ws(b[q - 1]))
      q--;
    size_t gen_end = q;
    while (q > 0 && b[q - 1] >= '0' && b[q - 1] <= '9')
      q--;
    size_t gen_start = q;
    if (gen_start == gen_end)
      continue;
    while (q > 0 && pdf_is_ws(b[q - 1]))
      q--;
    size_t num_end = q;
    while (q > 0 && b[q - 1] >= '0' && b[q - 1] <= '9')
      q--;
    size_t num_start = q;
    if (num_start == num_end || num_end == gen_start)
      continue;
    if (q > 0 && pdf_is_regular(b[q - 1]))
      continue; /* e.g. "endobj" */
    if (num_end - num_start > 9 || gen_end - gen_start > 5)
      continue;

    int64_t num = 0, gen = 0;
    for (size_t i = num_start; i < num_end; i++)
      num = num * 10 + (b[i] - '0');
    for (size_t i = gen_start; i < gen_end; i++)
      gen = gen * 10 + (b[i] - '0');
    if (num < 0 || num >= (int64_t)SCL_PDF_MAX_OBJECTS || gen > 65535)
      continue;

    scl_error_t err = pdf_xref_ensure(parser, (size_t)num + 1);
    if (err != SCL_OK)
      return err;
    /* Later occurrence wins (incremental updates append). */
    pdf_xref_set(parser, num, SCL_PDF_XREF_OFFSET, (uint64_t)num_start,
                 (uint32_t)gen, true);
    /* Invalidate any cached object parsed from a stale offset (but never
     * touch the static loading sentinel). */
    if (parser->obj_cache[num] &&
        parser->obj_cache[num] != &pdf_loading_sentinel) {
      pdf_obj_free(alloc, parser->obj_cache[num]);
      parser->obj_cache[num] = NULL;
    }
  }

  /* Recover a trailer if the chain didn't produce one: take the LAST
   * trailer dict carrying /Root. */
  if (!parser->trailer || !scl_pdf_dict_get(parser->trailer, "Root")) {
    pdf_ctx_t ctx = {parser, false};
    size_t tpos = 0;
    for (int guard = 0; guard < 256; guard++) {
      size_t hit = pdf_find(b, n, tpos, "trailer");
      if (hit == SIZE_MAX)
        break;
      tpos = hit + 7;
      pdf_cur_t cur = {b, n, tpos};
      scl_pdf_obj_t *t = NULL;
      if (pdf_parse_object(&ctx, &cur, 0, &t) == SCL_OK && t &&
          t->type == SCL_PDF_OBJ_DICT && scl_pdf_dict_get(t, "Root")) {
        pdf_obj_free(alloc, parser->trailer);
        parser->trailer = t;
      } else {
        pdf_obj_free(alloc, t);
      }
    }
  }
  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  6. Indirect object loading (§7.3.10, §7.5.7)
 * ════════════════════════════════════════════════════════════════════ */

/* Parse the object body at a classic xref offset. Verifies the "N G obj"
 * header actually names object `num`. */
static scl_error_t pdf_load_at_offset(scl_parse_pdf_t *parser, int num,
                                      uint64_t offset, scl_pdf_obj_t **out) {
  if (offset >= parser->buf_size)
    return SCL_ERR_PARSE;
  pdf_ctx_t ctx = {parser, false};
  pdf_cur_t cur = {parser->buf, parser->buf_size, (size_t)offset};
  pdf_skip_ws(&cur);
  int64_t hdr_num, hdr_gen;
  if (!pdf_parse_int_tok(&cur, &hdr_num))
    return SCL_ERR_NOT_FOUND; /* not an object header: offset is stale */
  pdf_skip_ws(&cur);
  if (!pdf_parse_int_tok(&cur, &hdr_gen))
    return SCL_ERR_NOT_FOUND;
  pdf_skip_ws(&cur);
  if (!pdf_match_kw(&cur, "obj"))
    return SCL_ERR_NOT_FOUND;
  if (hdr_num != (int64_t)num)
    return SCL_ERR_NOT_FOUND; /* xref lies: triggers repair upstream */
  return pdf_parse_object(&ctx, &cur, 0, out);
}

/* Load an object stream (§7.5.7) and cache EVERY object it contains in
 * one pass, so N objects cost one decompression instead of N. */
static scl_error_t pdf_load_objstm(scl_parse_pdf_t *parser, int cnum) {
  scl_allocator_t *alloc = parser->alloc;
  scl_pdf_obj_t *container = NULL;
  scl_error_t err = pdf_get_object_internal(parser, cnum, &container);
  if (err != SCL_OK)
    return err;
  if (!container || container->type != SCL_PDF_OBJ_STREAM)
    return SCL_ERR_PARSE;
  scl_pdf_obj_t *dict = container->u.stream.dict;
  if (!pdf_name_is(pdf_resolve(parser, scl_pdf_dict_get(dict, "Type")),
                   "ObjStm"))
    return SCL_ERR_PARSE;

  int64_t count, first;
  if (!pdf_obj_as_int(pdf_resolve(parser, scl_pdf_dict_get(dict, "N")),
                      &count) ||
      !pdf_obj_as_int(pdf_resolve(parser, scl_pdf_dict_get(dict, "First")),
                      &first))
    return SCL_ERR_PARSE;
  if (count < 0 || count > (int64_t)SCL_PDF_MAX_CONTAINER_LEN || first < 0)
    return SCL_ERR_PARSE;

  unsigned char *data = NULL;
  size_t data_len = 0;
  err = pdf_stream_decode(parser, container, &data, &data_len);
  if (err != SCL_OK)
    return err;
  if ((uint64_t)first > data_len) {
    scl_free(alloc, data);
    return SCL_ERR_PARSE;
  }

  /* Header: N pairs of "objnum offset" relative to /First. */
  pdf_ctx_t ctx = {parser, true};
  pdf_cur_t hdr = {data, (size_t)first, 0};
  for (int64_t i = 0; i < count; i++) {
    pdf_skip_ws(&hdr);
    int64_t onum, ooff;
    if (!pdf_parse_int_tok(&hdr, &onum))
      break;
    pdf_skip_ws(&hdr);
    if (!pdf_parse_int_tok(&hdr, &ooff))
      break;
    if (onum < 0 || (size_t)onum >= parser->xref_size || ooff < 0)
      continue;
    /* Only claim slots that the xref maps into THIS container and that
     * aren't populated yet (the requested object holds the sentinel). */
    if (parser->xref[onum].type != SCL_PDF_XREF_IN_OBJSTM ||
        parser->xref[onum].field2 != (uint64_t)cnum)
      continue;
    scl_pdf_obj_t *slot = parser->obj_cache[onum];
    if (slot != NULL && slot != &pdf_loading_sentinel)
      continue;
    size_t obj_off;
    if (scl_add_overflow((size_t)first, (size_t)ooff, &obj_off) ||
        obj_off >= data_len)
      continue;
    pdf_cur_t body = {data, data_len, obj_off};
    scl_pdf_obj_t *o = NULL;
    if (pdf_parse_object(&ctx, &body, 0, &o) == SCL_OK)
      parser->obj_cache[onum] = o;
    else
      parser->obj_cache[onum] = NULL;
  }
  scl_free(alloc, data);
  return SCL_OK;
}

static scl_error_t pdf_get_object_internal(scl_parse_pdf_t *parser, int num,
                                           scl_pdf_obj_t **out) {
  *out = NULL;
  if (num < 0 || (size_t)num >= parser->xref_size)
    return SCL_ERR_NOT_FOUND;
  scl_pdf_obj_t *cached = parser->obj_cache[num];
  if (cached == &pdf_loading_sentinel)
    return SCL_ERR_PARSE; /* reference cycle while loading */
  if (cached) {
    *out = cached;
    return SCL_OK;
  }

  scl_parse_pdf_xref_entry_t e = parser->xref[num];
  if (e.type == SCL_PDF_XREF_FREE || e.type == PDF_XREF_UNSET)
    return SCL_ERR_NOT_FOUND;

  parser->obj_cache[num] = &pdf_loading_sentinel;
  scl_error_t err;
  scl_pdf_obj_t *obj = NULL;

  if (e.type == SCL_PDF_XREF_OFFSET) {
    err = pdf_load_at_offset(parser, num, e.field2, &obj);
    if (err == SCL_ERR_NOT_FOUND && !parser->repaired) {
      /* Stale/corrupt offset: rebuild the xref by scanning, then retry. */
      parser->obj_cache[num] = NULL;
      err = pdf_repair(parser);
      if (err != SCL_OK)
        return err;
      return pdf_get_object_internal(parser, num, out);
    }
  } else { /* SCL_PDF_XREF_IN_OBJSTM */
    int cnum = (int)e.field2;
    /* The container itself must be a regular top-level object; a
     * container "inside" another objstm would recurse unboundedly. */
    if (cnum < 0 || (size_t)cnum >= parser->xref_size || cnum == num ||
        parser->xref[cnum].type != SCL_PDF_XREF_OFFSET) {
      parser->obj_cache[num] = NULL;
      return SCL_ERR_PARSE;
    }
    err = pdf_load_objstm(parser, cnum);
    if (err == SCL_OK) {
      obj = parser->obj_cache[num];
      if (obj == &pdf_loading_sentinel || obj == NULL) {
        obj = NULL;
        err = SCL_ERR_NOT_FOUND;
      }
    }
  }

  if (err != SCL_OK || !obj) {
    if (parser->obj_cache[num] == &pdf_loading_sentinel)
      parser->obj_cache[num] = NULL;
    return err != SCL_OK ? err : SCL_ERR_PARSE;
  }
  parser->obj_cache[num] = obj;
  *out = obj;
  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  7. Page tree flattening (§7.7.3)
 * ════════════════════════════════════════════════════════════════════ */

static scl_error_t pdf_flatten_pages(scl_parse_pdf_t *parser) {
  scl_allocator_t *alloc = parser->alloc;
  parser->page_count = 0;
  if (parser->root_obj <= 0)
    return SCL_OK;

  scl_pdf_obj_t *catalog = NULL;
  if (pdf_get_object_internal(parser, parser->root_obj, &catalog) != SCL_OK)
    return SCL_OK; /* unresolvable catalog: zero pages, not fatal */
  catalog = pdf_resolve(parser, catalog);
  scl_pdf_obj_t *pages_ref = scl_pdf_dict_get(catalog, "Pages");
  if (!pages_ref || pages_ref->type != SCL_PDF_OBJ_REF)
    return SCL_OK;

  /* Iterative DFS with a visited bitmap: immune to Kids cycles. */
  size_t stack_cap = 1024;
  int *stack = (int *)scl_alloc(alloc, stack_cap * sizeof(int), _Alignof(int));
  unsigned char *visited =
      (unsigned char *)scl_calloc(alloc, parser->xref_size, 1, 1);
  if (!stack || !visited) {
    scl_free(alloc, stack);
    scl_free(alloc, visited);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  size_t sp = 0;
  stack[sp++] = pages_ref->u.ref.num;

  size_t pages_cap = 0;
  scl_error_t err = SCL_OK;

  while (sp > 0) {
    int num = stack[--sp];
    if (num < 0 || (size_t)num >= parser->xref_size || visited[num])
      continue;
    visited[num] = 1;

    scl_pdf_obj_t *node = NULL;
    if (pdf_get_object_internal(parser, num, &node) != SCL_OK)
      continue;
    node = pdf_resolve(parser, node);
    if (!node || (node->type != SCL_PDF_OBJ_DICT))
      continue;

    scl_pdf_obj_t *type = pdf_resolve(parser, scl_pdf_dict_get(node, "Type"));
    if (pdf_name_is(type, "Page")) {
      if (parser->pages_count >= (int)SCL_PDF_MAX_PAGES)
        break;
      if ((size_t)parser->pages_count + 1 > pages_cap) {
        size_t new_cap = pages_cap ? pages_cap * 2 : 16;
        void *nb = scl_realloc(alloc, parser->pages, pages_cap * sizeof(int),
                               new_cap * sizeof(int), _Alignof(int));
        if (!nb) {
          err = SCL_ERR_OUT_OF_MEMORY;
          break;
        }
        parser->pages = (int *)nb;
        pages_cap = new_cap;
      }
      parser->pages[parser->pages_count++] = num;
      continue;
    }
    if (!pdf_name_is(type, "Pages"))
      continue;

    scl_pdf_obj_t *kids = pdf_resolve(parser, scl_pdf_dict_get(node, "Kids"));
    if (!kids || kids->type != SCL_PDF_OBJ_ARRAY)
      continue;
    /* Push in reverse so pops come out in document order. */
    for (size_t i = kids->u.array.count; i-- > 0;) {
      scl_pdf_obj_t *kid = kids->u.array.items[i];
      if (!kid || kid->type != SCL_PDF_OBJ_REF)
        continue;
      if (sp >= stack_cap) {
        if (stack_cap >= SCL_PDF_MAX_OBJECTS)
          break;
        size_t new_cap = stack_cap * 2;
        void *nb = scl_realloc(alloc, stack, stack_cap * sizeof(int),
                               new_cap * sizeof(int), _Alignof(int));
        if (!nb) {
          err = SCL_ERR_OUT_OF_MEMORY;
          goto out;
        }
        stack = (int *)nb;
        stack_cap = new_cap;
      }
      stack[sp++] = kid->u.ref.num;
    }
  }
out:
  scl_free(alloc, stack);
  scl_free(alloc, visited);
  parser->page_count = parser->pages_count;
  return err;
}

/* ════════════════════════════════════════════════════════════════════
 *  8. Text-string decoding (§7.9.2.2) + text extraction (§9.4)
 * ════════════════════════════════════════════════════════════════════ */

/* Append a Unicode code point as UTF-8. */
static size_t pdf_utf8_put(char *dst, uint32_t cp) {
  if (cp < 0x80) {
    dst[0] = (char)cp;
    return 1;
  }
  if (cp < 0x800) {
    dst[0] = (char)(0xC0 | (cp >> 6));
    dst[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  }
  if (cp < 0x10000) {
    dst[0] = (char)(0xE0 | (cp >> 12));
    dst[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    dst[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  dst[0] = (char)(0xF0 | (cp >> 18));
  dst[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  dst[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  dst[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

/* Decode a PDF text string (§7.9.2.2) to UTF-8: UTF-16BE when it starts
 * with the FE FF BOM, PDFDocEncoding otherwise (approximated by Latin-1
 * for the printable range; PDFDoc control-region bytes are dropped). */
static scl_error_t pdf_text_to_utf8(scl_allocator_t *alloc,
                                    const unsigned char *s, size_t len,
                                    char **out, size_t *out_len) {
  size_t cap = len * 4 + 8; /* worst case */
  if (cap < len)
    return SCL_ERR_SIZE_OVERFLOW;
  char *buf = (char *)scl_alloc(alloc, cap, 1);
  if (!buf)
    return SCL_ERR_OUT_OF_MEMORY;
  size_t n = 0;

  if (len >= 2 && s[0] == 0xFE && s[1] == 0xFF) {
    for (size_t i = 2; i + 1 < len; i += 2) {
      uint32_t unit = ((uint32_t)s[i] << 8) | s[i + 1];
      if (unit >= 0xD800 && unit <= 0xDBFF && i + 3 < len) {
        uint32_t lo = ((uint32_t)s[i + 2] << 8) | s[i + 3];
        if (lo >= 0xDC00 && lo <= 0xDFFF) {
          uint32_t cp = 0x10000 + ((unit - 0xD800) << 10) + (lo - 0xDC00);
          n += pdf_utf8_put(buf + n, cp);
          i += 2;
          continue;
        }
      }
      if (unit >= 0xD800 && unit <= 0xDFFF)
        continue; /* unpaired surrogate: drop */
      n += pdf_utf8_put(buf + n, unit);
    }
  } else {
    for (size_t i = 0; i < len; i++) {
      unsigned char ch = s[i];
      if (ch == '\n' || ch == '\r' || ch == '\t' ||
          (ch >= 0x20 && ch <= 0x7E)) {
        buf[n++] = (char)ch;
      } else if (ch >= 0xA1) {
        n += pdf_utf8_put(buf + n, ch); /* Latin-1 approximation */
      } /* else: control / PDFDoc escape region — drop */
    }
  }
  buf[n] = '\0';
  *out = buf;
  *out_len = n;
  return SCL_OK;
}

/* Growable output for extracted text, capped at SCL_PDF_MAX_TEXT_LEN. */
typedef struct {
  char *buf;
  size_t len, cap;
  bool truncated;
} pdf_text_out_t;

static scl_error_t pdf_text_out_append(scl_allocator_t *alloc,
                                       pdf_text_out_t *t, const char *s,
                                       size_t n) {
  if (t->truncated)
    return SCL_OK;
  if (n > SCL_PDF_MAX_TEXT_LEN - t->len) {
    n = SCL_PDF_MAX_TEXT_LEN - t->len;
    t->truncated = true;
  }
  if (t->len + n + 1 > t->cap) {
    size_t new_cap = t->cap ? t->cap * 2 : 256;
    while (new_cap < t->len + n + 1)
      new_cap *= 2;
    void *nb = scl_realloc(alloc, t->buf, t->cap, new_cap, 1);
    if (!nb)
      return SCL_ERR_OUT_OF_MEMORY;
    t->buf = (char *)nb;
    t->cap = new_cap;
  }
  scl_memcpy(t->buf + t->len, s, n);
  t->len += n;
  t->buf[t->len] = '\0';
  return SCL_OK;
}

/* Skip an inline image (§8.9.7): BI <dict entries> ID <binary> EI. */
static void pdf_skip_inline_image(pdf_cur_t *c) {
  /* Find "ID" at a token boundary. */
  while (c->p < c->n) {
    size_t hit = pdf_find(c->b, c->n, c->p, "ID");
    if (hit == SIZE_MAX) {
      c->p = c->n;
      return;
    }
    c->p = hit + 2;
    if (!pdf_is_regular(hit + 2 < c->n ? c->b[hit + 2] : -1) &&
        (hit == 0 || !pdf_is_regular(c->b[hit - 1])))
      break;
  }
  if (c->p < c->n && pdf_is_ws(c->b[c->p]))
    c->p++; /* single whitespace after ID */
  /* Binary data until "EI" delimited by whitespace. */
  while (c->p < c->n) {
    size_t hit = pdf_find(c->b, c->n, c->p, "EI");
    if (hit == SIZE_MAX) {
      c->p = c->n;
      return;
    }
    c->p = hit + 2;
    bool ok_before = hit == 0 || pdf_is_ws(c->b[hit - 1]);
    bool ok_after = hit + 2 >= c->n || !pdf_is_regular(c->b[hit + 2]);
    if (ok_before && ok_after)
      return;
  }
}

/* Scan one decoded content stream, appending shown text (§9.4.3). */
static scl_error_t pdf_scan_content(scl_parse_pdf_t *parser,
                                    const unsigned char *data, size_t len,
                                    pdf_text_out_t *out) {
  scl_allocator_t *alloc = parser->alloc;
  pdf_ctx_t ctx = {parser, true}; /* no streams inside content */
  pdf_cur_t c = {data, len, 0};
  pdf_text_out_t pending = {NULL, 0, 0, false};
  scl_error_t err = SCL_OK;

  while (err == SCL_OK && !out->truncated) {
    pdf_skip_ws(&c);
    int ch = cur_peek(&c);
    if (ch < 0)
      break;

    if (ch == '(' || (ch == '<' && cur_peek_at(&c, 1) != '<')) {
      scl_pdf_obj_t *s = NULL;
      err = (ch == '(') ? pdf_parse_lit_string(&ctx, &c, &s)
                        : pdf_parse_hex_string(&ctx, &c, &s);
      if (err != SCL_OK) {
        c.p++; /* tolerate malformed strings: resync */
        err = SCL_OK;
        continue;
      }
      char *u8 = NULL;
      size_t u8n = 0;
      err = pdf_text_to_utf8(alloc, (const unsigned char *)s->u.string.data,
                             s->u.string.len, &u8, &u8n);
      if (err == SCL_OK) {
        err = pdf_text_out_append(alloc, &pending, u8, u8n);
        scl_free(alloc, u8);
      }
      pdf_obj_free(alloc, s);
    } else if (ch == '<') { /* << dict >> (e.g. BDC property list) */
      scl_pdf_obj_t *d = NULL;
      if (pdf_parse_dict(&ctx, &c, 0, &d) == SCL_OK)
        pdf_obj_free(alloc, d);
      else
        c.p += 2;
    } else if (ch == '[' || ch == ']' || ch == '{' || ch == '}') {
      c.p++; /* transparent: TJ arrays, PostScript braces */
    } else if (ch == '/') {
      scl_pdf_obj_t *nm = NULL;
      if (pdf_parse_name(&ctx, &c, &nm) == SCL_OK)
        pdf_obj_free(alloc, nm);
      else
        c.p++;
    } else if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-' ||
               ch == '.') {
      bool is_real;
      int64_t iv;
      double rv;
      if (!pdf_parse_number_tok(&c, &is_real, &iv, &rv))
        c.p++;
    } else if (ch == ')' || ch == '>') {
      c.p++; /* stray delimiter: resync */
    } else {
      /* Operator keyword. */
      char kw[8];
      size_t kn = 0;
      while (pdf_is_regular(cur_peek(&c))) {
        if (kn + 1 < sizeof(kw))
          kw[kn++] = (char)cur_peek(&c);
        c.p++;
      }
      kw[kn] = '\0';
      if (kn == 0) {
        c.p++;
        continue;
      }

      bool show = scl_strcmp(kw, "Tj") == 0 || scl_strcmp(kw, "TJ") == 0;
      bool show_nl = scl_strcmp(kw, "'") == 0 || scl_strcmp(kw, "\"") == 0;
      bool newline = scl_strcmp(kw, "Td") == 0 || scl_strcmp(kw, "TD") == 0 ||
                     scl_strcmp(kw, "T*") == 0 || scl_strcmp(kw, "ET") == 0;

      if (show || show_nl) {
        if (show_nl && out->len > 0 && out->buf[out->len - 1] != '\n')
          err = pdf_text_out_append(alloc, out, "\n", 1);
        if (err == SCL_OK && pending.len > 0)
          err = pdf_text_out_append(alloc, out, pending.buf, pending.len);
      } else if (newline) {
        if (out->len > 0 && out->buf[out->len - 1] != '\n')
          err = pdf_text_out_append(alloc, out, "\n", 1);
      } else if (scl_strcmp(kw, "BI") == 0) {
        pdf_skip_inline_image(&c);
      }
      pending.len = 0; /* every operator consumes its operands */
      if (pending.buf)
        pending.buf[0] = '\0';
    }
  }
  scl_free(alloc, pending.buf);
  return err;
}

/* ════════════════════════════════════════════════════════════════════
 *  9. Document bootstrap + public API
 * ════════════════════════════════════════════════════════════════════ */

/* Parse "%PDF-x.y" (§7.5.2); the header may be preceded by junk within
 * the first 1024 bytes (Adobe implementation note). */
static bool pdf_parse_header(scl_parse_pdf_t *parser) {
  size_t limit = parser->buf_size < 1024 ? parser->buf_size : 1024;
  size_t hit = pdf_find(parser->buf, limit, 0, "%PDF-");
  if (hit == SIZE_MAX || hit + 7 >= parser->buf_size)
    return false;
  unsigned char maj = parser->buf[hit + 5];
  unsigned char min_c = parser->buf[hit + 7];
  if (maj < '0' || maj > '9' || parser->buf[hit + 6] != '.' || min_c < '0' ||
      min_c > '9')
    return false;
  parser->version_major = maj - '0';
  parser->version_minor = min_c - '0';
  return true;
}

/* Locate the LAST "startxref" (§7.5.5) near EOF and return its operand. */
static bool pdf_find_startxref(scl_parse_pdf_t *parser, uint64_t *out) {
  size_t tail = parser->buf_size > 2048 ? parser->buf_size - 2048 : 0;
  size_t last = SIZE_MAX;
  size_t pos = tail;
  for (;;) {
    size_t hit = pdf_find(parser->buf, parser->buf_size, pos, "startxref");
    if (hit == SIZE_MAX)
      break;
    last = hit;
    pos = hit + 9;
  }
  if (last == SIZE_MAX)
    return false;
  pdf_cur_t cur = {parser->buf, parser->buf_size, last + 9};
  pdf_skip_ws(&cur);
  int64_t off;
  if (!pdf_parse_int_tok(&cur, &off) || off < 0)
    return false;
  *out = (uint64_t)off;
  return true;
}

/* Shared init once parser->buf/buf_size are populated. */
static scl_error_t pdf_init_document(scl_parse_pdf_t *parser) {
  if (!pdf_parse_header(parser))
    return SCL_ERR_PARSE;

  uint64_t sx = 0;
  scl_error_t err = SCL_ERR_PARSE;
  if (pdf_find_startxref(parser, &sx))
    err = pdf_load_xref_chain(parser, sx);
  if (err != SCL_OK || !parser->trailer ||
      !scl_pdf_dict_get(parser->trailer, "Root")) {
    err = pdf_repair(parser);
    if (err != SCL_OK)
      return err;
  }

  if (parser->trailer) {
    scl_pdf_obj_t *t = scl_pdf_dict_get(parser->trailer, "Root");
    if (t && t->type == SCL_PDF_OBJ_REF)
      parser->root_obj = t->u.ref.num;
    t = scl_pdf_dict_get(parser->trailer, "Info");
    if (t && t->type == SCL_PDF_OBJ_REF)
      parser->info_obj = t->u.ref.num;
    parser->encrypted = scl_pdf_dict_get(parser->trailer, "Encrypt") != NULL;
  }

  /* Catalog /Version overrides the header when newer (§7.7.2). */
  if (parser->root_obj > 0) {
    scl_pdf_obj_t *cat = NULL;
    if (pdf_get_object_internal(parser, parser->root_obj, &cat) == SCL_OK) {
      scl_pdf_obj_t *v = pdf_resolve(parser, scl_pdf_dict_get(cat, "Version"));
      if (v && v->type == SCL_PDF_OBJ_NAME && v->u.string.len == 3 &&
          scl_isdigit((unsigned char)v->u.string.data[0]) &&
          v->u.string.data[1] == '.' &&
          scl_isdigit((unsigned char)v->u.string.data[2])) {
        int maj = v->u.string.data[0] - '0';
        int min_v = v->u.string.data[2] - '0';
        if (maj > parser->version_major ||
            (maj == parser->version_major && min_v > parser->version_minor)) {
          parser->version_major = maj;
          parser->version_minor = min_v;
        }
      }
    }
  }

  return pdf_flatten_pages(parser);
}

static void pdf_free_all(scl_parse_pdf_t *parser) {
  scl_allocator_t *alloc = parser->alloc;
  if (parser->obj_cache) {
    for (size_t i = 0; i < parser->xref_size; i++)
      if (parser->obj_cache[i] != &pdf_loading_sentinel)
        pdf_obj_free(alloc, parser->obj_cache[i]);
    scl_free(alloc, parser->obj_cache);
  }
  scl_free(alloc, parser->xref);
  scl_free(alloc, parser->pages);
  pdf_obj_free(alloc, parser->trailer);
  if (parser->owns_buf)
    scl_free(alloc, parser->buf);
  scl_free(alloc, parser->filename);
  parser->obj_cache = NULL;
  parser->xref = NULL;
  parser->pages = NULL;
  parser->trailer = NULL;
  parser->buf = NULL;
  parser->filename = NULL;
  parser->buf_size = 0;
  parser->xref_size = 0;
}

scl_error_t scl_parse_pdf_open_mem(scl_allocator_t *alloc,
                                   scl_parse_pdf_t *parser, const void *data,
                                   size_t len) {
  if (scl_unlikely(!parser || !data))
    return SCL_ERR_NULL_PTR;
  if (len == 0 || len > SCL_PDF_MAX_FILE_SIZE)
    return SCL_ERR_INVALID_ARG;
  if (!alloc)
    alloc = scl_allocator_default();
  (void)scl_memset(parser, 0, sizeof(*parser));
  parser->alloc = alloc;

  parser->buf = (unsigned char *)scl_alloc(alloc, len + 1, 1);
  if (!parser->buf)
    return SCL_ERR_OUT_OF_MEMORY;
  scl_memcpy(parser->buf, data, len);
  parser->buf[len] = '\0';
  parser->buf_size = len;
  parser->owns_buf = true;

  scl_error_t err = pdf_init_document(parser);
  if (err != SCL_OK) {
    pdf_free_all(parser);
    return err;
  }
  return SCL_OK;
}

scl_error_t scl_parse_pdf_open(scl_allocator_t *alloc, scl_parse_pdf_t *parser,
                               const char *filename) {
  if (scl_unlikely(!parser || !filename))
    return SCL_ERR_NULL_PTR;
  if (!alloc)
    alloc = scl_allocator_default();
  (void)scl_memset(parser, 0, sizeof(*parser));
  parser->alloc = alloc;

  int fd = open(filename, O_RDONLY | O_CLOEXEC);
  if (fd < 0)
    return SCL_ERR_NOT_FOUND;
  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 0) {
    close(fd);
    return SCL_ERR_IO;
  }
  if ((uint64_t)st.st_size > SCL_PDF_MAX_FILE_SIZE || st.st_size == 0) {
    close(fd);
    return SCL_ERR_INVALID_ARG;
  }
  size_t size = (size_t)st.st_size;

  parser->buf = (unsigned char *)scl_alloc(alloc, size + 1, 1);
  if (!parser->buf) {
    close(fd);
    return SCL_ERR_OUT_OF_MEMORY;
  }
  size_t got = 0;
  while (got < size) {
    ssize_t r = read(fd, parser->buf + got, size - got);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      break;
    }
    if (r == 0)
      break;
    got += (size_t)r;
  }
  close(fd);
  if (got != size) {
    scl_free(alloc, parser->buf);
    parser->buf = NULL;
    return SCL_ERR_IO;
  }
  parser->buf[size] = '\0';
  parser->buf_size = size;
  parser->owns_buf = true;

  parser->filename = scl_strdup(alloc, filename);
  if (!parser->filename) {
    scl_free(alloc, parser->buf);
    parser->buf = NULL;
    return SCL_ERR_OUT_OF_MEMORY;
  }

  scl_error_t err = pdf_init_document(parser);
  if (err != SCL_OK) {
    pdf_free_all(parser);
    return err;
  }
  return SCL_OK;
}

scl_error_t scl_parse_pdf_close(scl_parse_pdf_t *parser) {
  if (scl_unlikely(!parser))
    return SCL_ERR_NULL_PTR;
  if (parser->alloc)
    pdf_free_all(parser);
  return SCL_OK;
}

scl_error_t scl_parse_pdf_get_page_count(scl_parse_pdf_t *parser, int *out) {
  if (scl_unlikely(!parser || !out))
    return SCL_ERR_NULL_PTR;
  *out = parser->page_count;
  return SCL_OK;
}

scl_error_t scl_parse_pdf_get_version(scl_parse_pdf_t *parser, int *major,
                                      int *minor) {
  if (scl_unlikely(!parser || !major || !minor))
    return SCL_ERR_NULL_PTR;
  *major = parser->version_major;
  *minor = parser->version_minor;
  return SCL_OK;
}

bool scl_parse_pdf_is_encrypted(const scl_parse_pdf_t *parser) {
  return parser ? parser->encrypted : false;
}

scl_error_t scl_parse_pdf_get_object(scl_parse_pdf_t *parser, int num, int gen,
                                     scl_pdf_obj_t **out) {
  if (scl_unlikely(!parser || !out))
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  if (num < 0 || (size_t)num >= parser->xref_size)
    return SCL_ERR_NOT_FOUND;
  if (gen >= 0 && parser->xref[num].type == SCL_PDF_XREF_OFFSET &&
      parser->xref[num].field3 != (uint32_t)gen)
    return SCL_ERR_NOT_FOUND;
  return pdf_get_object_internal(parser, num, out);
}

scl_error_t scl_parse_pdf_resolve(scl_parse_pdf_t *parser, scl_pdf_obj_t *obj,
                                  scl_pdf_obj_t **out) {
  if (scl_unlikely(!parser || !out))
    return SCL_ERR_NULL_PTR;
  *out = pdf_resolve(parser, obj);
  return *out ? SCL_OK : SCL_ERR_NOT_FOUND;
}

scl_error_t scl_parse_pdf_get_stream_data(scl_parse_pdf_t *parser,
                                          scl_pdf_obj_t *stream, void **out,
                                          size_t *out_len) {
  if (scl_unlikely(!parser || !stream || !out || !out_len))
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  *out_len = 0;
  if (parser->encrypted)
    return SCL_ERR_UNSUPPORTED; /* payloads are ciphertext (§7.6) */
  unsigned char *data = NULL;
  size_t n = 0;
  scl_error_t err = pdf_stream_decode(parser, stream, &data, &n);
  if (err != SCL_OK)
    return err;
  *out = data;
  *out_len = n;
  return SCL_OK;
}

scl_error_t scl_parse_pdf_get_page(scl_parse_pdf_t *parser, int index,
                                   scl_pdf_obj_t **out) {
  if (scl_unlikely(!parser || !out))
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  if (index < 0 || index >= parser->pages_count)
    return SCL_ERR_INVALID_INDEX;
  return pdf_get_object_internal(parser, parser->pages[index], out);
}

scl_error_t scl_parse_pdf_extract_text(scl_parse_pdf_t *parser, int index,
                                       char **out, size_t *out_len) {
  if (scl_unlikely(!parser || !out || !out_len))
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  *out_len = 0;
  if (parser->encrypted)
    return SCL_ERR_UNSUPPORTED;
  scl_pdf_obj_t *page = NULL;
  scl_error_t err = scl_parse_pdf_get_page(parser, index, &page);
  if (err != SCL_OK)
    return err;

  scl_allocator_t *alloc = parser->alloc;
  scl_pdf_obj_t *contents =
      pdf_resolve(parser, scl_pdf_dict_get(page, "Contents"));
  pdf_text_out_t text = {NULL, 0, 0, false};

  /* /Contents: one stream or an array of streams (§7.7.3.3). */
  scl_pdf_obj_t *single[1] = {contents};
  scl_pdf_obj_t **list = single;
  size_t count = contents ? 1 : 0;
  if (contents && contents->type == SCL_PDF_OBJ_ARRAY) {
    list = contents->u.array.items;
    count = contents->u.array.count;
  }

  for (size_t i = 0; i < count && err == SCL_OK; i++) {
    scl_pdf_obj_t *cs = pdf_resolve(parser, list[i]);
    if (!cs || cs->type != SCL_PDF_OBJ_STREAM)
      continue;
    unsigned char *data = NULL;
    size_t dlen = 0;
    if (pdf_stream_decode(parser, cs, &data, &dlen) != SCL_OK)
      continue; /* unsupported filter (e.g. image codec): skip stream */
    err = pdf_scan_content(parser, data, dlen, &text);
    scl_free(alloc, data);
  }
  if (err != SCL_OK) {
    scl_free(alloc, text.buf);
    return err;
  }
  if (!text.buf) {
    text.buf = (char *)scl_alloc(alloc, 1, 1);
    if (!text.buf)
      return SCL_ERR_OUT_OF_MEMORY;
    text.buf[0] = '\0';
  }
  *out = text.buf;
  *out_len = text.len;
  return SCL_OK;
}

scl_error_t scl_parse_pdf_get_info(scl_parse_pdf_t *parser, const char *key,
                                   char *out, size_t *out_len) {
  if (scl_unlikely(!parser || !key || !out))
    return SCL_ERR_NULL_PTR;
  if (scl_unlikely(!out_len || *out_len == 0))
    return SCL_ERR_INVALID_ARG;
  if (parser->info_obj <= 0)
    return SCL_ERR_NOT_FOUND;

  scl_pdf_obj_t *info = NULL;
  if (pdf_get_object_internal(parser, parser->info_obj, &info) != SCL_OK)
    return SCL_ERR_NOT_FOUND;
  scl_pdf_obj_t *val = pdf_resolve(parser, scl_pdf_dict_get(info, key));
  if (!val || val->type != SCL_PDF_OBJ_STRING)
    return SCL_ERR_NOT_FOUND;

  char *u8 = NULL;
  size_t u8n = 0;
  scl_error_t err =
      pdf_text_to_utf8(parser->alloc, (const unsigned char *)val->u.string.data,
                       val->u.string.len, &u8, &u8n);
  if (err != SCL_OK)
    return err;
  size_t to_copy = u8n < *out_len ? u8n : *out_len - 1;
  scl_memcpy(out, u8, to_copy);
  out[to_copy] = '\0';
  *out_len = to_copy + 1;
  scl_free(parser->alloc, u8);
  return SCL_OK;
}
