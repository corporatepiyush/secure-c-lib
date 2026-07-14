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

/* Recursive-descent JSON parser to typed tree. Objects, arrays, strings,
 * numbers, booleans, null. UTF-8 validated. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_json.h"
#include "scl_stdlib.h"
#include "scl_string.h"

static void json_skip_ws(const char **p) {
  while (**p && (unsigned char)**p <= ' ')
    (*p)++;
}

static scl_parse_json_value_t *json_new_val(scl_allocator_t *alloc,
                                             scl_parse_json_type_t type) {
  scl_parse_json_value_t *v = (scl_parse_json_value_t *)scl_calloc(
      alloc, 1, sizeof(scl_parse_json_value_t), _Alignof(max_align_t));
  if (!v)
    return NULL;
  v->type = type;
  return v;
}

static int json_add_child(scl_allocator_t *alloc,
                           scl_parse_json_value_t *parent,
                           scl_parse_json_value_t *child) {
  if (parent->child_count >= parent->child_cap) {
    size_t nc = parent->child_cap ? parent->child_cap * 2 : 4;
    scl_parse_json_value_t **ch = (scl_parse_json_value_t **)scl_realloc(
        alloc, parent->children, parent->child_cap * sizeof(void *),
        nc * sizeof(void *), _Alignof(max_align_t));
    if (!ch)
      return -1;
    if (parent->type == SCL_JSON_OBJECT) {
      char **ks = (char **)scl_realloc(
          alloc, parent->keys, parent->child_cap * sizeof(char *),
          nc * sizeof(char *), _Alignof(max_align_t));
      if (!ks)
        return -1;
      parent->keys = ks;
    }
    parent->children = ch;
    parent->child_cap = nc;
  }
  child->parent = parent;
  parent->children[parent->child_count++] = child;
  return 0;
}

static int json_add_child_key(scl_allocator_t *alloc,
                               scl_parse_json_value_t *parent,
                               scl_parse_json_value_t *child, char *key) {
  if (json_add_child(alloc, parent, child) != 0)
    return -1;
  parent->keys[parent->child_count - 1] = key;
  return 0;
}

/* Read exactly 4 hex digits from s. Returns the value or -1 if any of the four
 * is not a hex digit (a NUL counts as non-hex, so this never reads past the
 * input's terminator). */
static int json_hex4(const char *s) {
  int v = 0;
  for (int i = 0; i < 4; i++) {
    char ch = s[i];
    int d;
    if (ch >= '0' && ch <= '9')
      d = ch - '0';
    else if (ch >= 'a' && ch <= 'f')
      d = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
      d = ch - 'A' + 10;
    else
      return -1;
    v = (v << 4) | d;
  }
  return v;
}

/* Encode a Unicode code point as UTF-8 into out (max 4 bytes). Returns the
 * number of bytes written. */
static size_t json_utf8_encode(char *out, unsigned int cp) {
  if (cp < 0x80) {
    out[0] = (char)cp;
    return 1;
  } else if (cp < 0x800) {
    out[0] = (char)(0xC0 | (cp >> 6));
    out[1] = (char)(0x80 | (cp & 0x3F));
    return 2;
  } else if (cp < 0x10000) {
    out[0] = (char)(0xE0 | (cp >> 12));
    out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[2] = (char)(0x80 | (cp & 0x3F));
    return 3;
  }
  out[0] = (char)(0xF0 | (cp >> 18));
  out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
  out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
  out[3] = (char)(0x80 | (cp & 0x3F));
  return 4;
}

/* max_len is the remaining byte budget from current position to end of
 * input.  The parsed string is capped at SCL_JSON_MAX_STRING_LEN bytes. */
static char *json_parse_string(scl_allocator_t *alloc, const char **p,
                                size_t max_len) {
  if (**p != '"')
    return NULL;
  (*p)++;
  size_t cap = 256, len = 0;
  if (cap > SCL_JSON_MAX_STRING_LEN)
    cap = SCL_JSON_MAX_STRING_LEN;
  char *s = (char *)scl_alloc(alloc, cap, _Alignof(max_align_t));
  if (!s)
    return NULL;

  while (**p && **p != '"') {
    if (len >= max_len) {
      scl_free(alloc, s);
      return NULL; /* input exceeds max string length */
    }
    if (**p == '\\') {
      (*p)++;
      char c = **p;
      if (c == '\0')
        break; /* lone trailing backslash: stop before NUL */
      switch (c) {
      case '"':
      case '\\':
      case '/':
        s[len++] = c;
        break;
      case 'b':
        s[len++] = '\b';
        break;
      case 'f':
        s[len++] = '\f';
        break;
      case 'n':
        s[len++] = '\n';
        break;
      case 'r':
        s[len++] = '\r';
        break;
      case 't':
        s[len++] = '\t';
        break;
      case 'u': {
        /* \uXXXX — *p is at 'u'; the 4 hex digits follow. Decode to a
         * code point, combine surrogate pairs, and emit real UTF-8 so
         * non-ASCII text survives instead of being replaced by '?'. */
        int hi = json_hex4(*p + 1);
        if (hi < 0) {
          s[len++] = '?';
          break;
        } /* malformed: leave *p at 'u' */
        *p += 4; /* consume the 4 hex digits */
        unsigned int cp = (unsigned int)hi;
        if (cp >= 0xD800 && cp <= 0xDBFF) { /* high surrogate */
          if ((*p)[1] == '\\' && (*p)[2] == 'u') {
            int lo = json_hex4(*p + 3);
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
              cp = 0x10000u + ((cp - 0xD800u) << 10) +
                   ((unsigned int)lo - 0xDC00u);
              *p += 6; /* consume the \uYYYY low half */
            } else {
              cp = 0xFFFD; /* unpaired high surrogate */
            }
          } else {
            cp = 0xFFFD;
          }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
          cp = 0xFFFD; /* lone low surrogate */
        }
        len += json_utf8_encode(s + len, cp);
        break;
      }
      default:
        s[len++] = c;
        break;
      }
    } else {
      s[len++] = **p;
    }
    /* Keep at least 4 bytes of headroom so a multi-byte UTF-8 emission and
     * the final NUL always fit. */
    if (len + 4 >= cap) {
      if (cap >= SCL_JSON_MAX_STRING_LEN) {
        scl_free(alloc, s);
        return NULL; /* string exceeds max length */
      }
      size_t ncap = cap * 2;
      if (ncap > SCL_JSON_MAX_STRING_LEN)
        ncap = SCL_JSON_MAX_STRING_LEN;
      char *ns =
          (char *)scl_realloc(alloc, s, cap, ncap, _Alignof(max_align_t));
      if (!ns) {
        scl_free(alloc, s);
        return NULL;
      }
      s = ns;
      cap = ncap;
    }
    (*p)++;
  }
  if (**p == '"')
    (*p)++;
  s[len] = '\0';

  char *ns = (char *)scl_realloc(alloc, s, cap, len + 1, _Alignof(max_align_t));
  return ns ? ns : s;
}

static scl_error_t json_parse_number(scl_allocator_t *alloc, const char **p,
                                      scl_parse_json_value_t *val,
                                      const char *end) {
  const char *start = *p;
  int is_float = 0;
  while (*p < end && (scl_isdigit((unsigned char)**p) || **p == '-' || **p == '+' ||
                      **p == '.' || **p == 'e' || **p == 'E')) {
    if (**p == '.' || **p == 'e' || **p == 'E')
      is_float = 1;
    (*p)++;
  }

  size_t nlen = *p - start;
  char *num = (char *)scl_alloc(alloc, nlen + 1, _Alignof(max_align_t));
  if (!num)
    return SCL_ERR_OUT_OF_MEMORY;
  scl_memcpy(num, start, nlen);
  num[nlen] = '\0';

  if (is_float) {
    val->type = SCL_JSON_DOUBLE;
    val->double_val = scl_strtod(num, NULL);
  } else {
    val->type = SCL_JSON_INT64;
    val->int64_val = (int64_t)scl_strtoll(num, NULL, 10);
  }
  scl_free(alloc, num);
  return SCL_OK;
}

#define JSON_STACK_MAX 256

typedef enum {
  JS_OBJ_START,
  JS_OBJ_COLON,
  JS_OBJ_VAL,
  JS_OBJ_NEXT,
  JS_ARR_VAL,
  JS_ARR_NEXT
} json_state_t;

typedef struct {
  scl_parse_json_value_t *container;
  json_state_t state;
  char *key;
} json_frame_t;

static int json_attach_value(json_frame_t *stack, int sp,
                              scl_allocator_t *alloc,
                              scl_parse_json_value_t *val,
                              scl_parse_json_value_t **root) {
  if (sp < 0) {
    if (!*root)
      *root = val;
    return 0;
  }
  json_frame_t *f = &stack[sp];
  if (f->state == JS_OBJ_VAL) {
    if (json_add_child_key(alloc, f->container, val, f->key) != 0)
      return -1;
    f->key = NULL;
    f->state = JS_OBJ_NEXT;
  } else if (f->state == JS_ARR_VAL) {
    if (json_add_child(alloc, f->container, val) != 0)
      return -1;
    f->state = JS_ARR_NEXT;
  } else {
    /* A value appeared where a separator or close was expected (e.g. a
     * missing comma). Reject it instead of silently dropping/leaking. */
    return -2;
  }
  return 1;
}

static scl_error_t json_parse_with_len(scl_allocator_t *alloc,
                                        const char *json_str, size_t len,
                                        scl_parse_json_value_t **out_root);

scl_error_t scl_parse_json_parse(scl_allocator_t *alloc, const char *json_str,
                                  scl_parse_json_value_t **out_root) {
  if (!json_str)
    return SCL_ERR_NULL_PTR;
  return json_parse_with_len(alloc, json_str, scl_strlen(json_str), out_root);
}

scl_error_t scl_parse_json_parse_with_len(scl_allocator_t *alloc,
                                           const char *json_str, size_t len,
                                           scl_parse_json_value_t **out_root) {
  if (!json_str)
    return SCL_ERR_NULL_PTR;
  if (!out_root)
    return SCL_ERR_NULL_PTR;
  /* Reject absurd input lengths up front. */
  if (len > SCL_JSON_MAX_STRING_LEN * JSON_STACK_MAX)
    return SCL_ERR_SIZE_OVERFLOW;
  return json_parse_with_len(alloc, json_str, len, out_root);
}

static scl_error_t json_parse_with_len(scl_allocator_t *alloc,
                                        const char *json_str, size_t len,
                                        scl_parse_json_value_t **out_root) {
  const char *p = json_str;
  const char *end = json_str + len;
  json_skip_ws(&p);
  if (p >= end)
    return SCL_ERR_EMPTY;

  scl_parse_json_value_t *root = NULL;
  scl_parse_json_value_t *cur_val = NULL;
  json_frame_t stack[JSON_STACK_MAX];
  int sp = -1;
  scl_error_t err = SCL_ERR_INVALID_ARG;

  while (p < end) {
    json_skip_ws(&p);
    if (p >= end)
      break;

    char c = *p;

    if (c == '{') {
      p++;
      scl_parse_json_value_t *obj = json_new_val(alloc, SCL_JSON_OBJECT);
      if (!obj) {
        err = SCL_ERR_OUT_OF_MEMORY;
        goto cleanup;
      }

      json_skip_ws(&p);
      if (p < end && *p == '}') {
        p++;
        cur_val = obj;
      } else {
        if (sp + 1 >= JSON_STACK_MAX) {
          cur_val = obj;
          err = SCL_ERR_SIZE_OVERFLOW;
          goto cleanup;
        }
        sp++;
        stack[sp].container = obj;
        stack[sp].state = JS_OBJ_START;
        stack[sp].key = NULL;
        continue;
      }
    } else if (c == '[') {
      p++;
      scl_parse_json_value_t *arr = json_new_val(alloc, SCL_JSON_ARRAY);
      if (!arr) {
        err = SCL_ERR_OUT_OF_MEMORY;
        goto cleanup;
      }

      json_skip_ws(&p);
      if (p < end && *p == ']') {
        p++;
        cur_val = arr;
      } else {
        if (sp + 1 >= JSON_STACK_MAX) {
          cur_val = arr;
          err = SCL_ERR_SIZE_OVERFLOW;
          goto cleanup;
        }
        sp++;
        stack[sp].container = arr;
        stack[sp].state = JS_ARR_VAL;
        stack[sp].key = NULL;
        continue;
      }
    } else if (c == '"') {
      size_t max_str_len = (size_t)(end - p);
      if (sp >= 0 && stack[sp].state == JS_OBJ_START) {
        char *key = json_parse_string(alloc, &p, max_str_len);
        if (!key) {
          err = SCL_ERR_PARSE;
          goto cleanup;
        }
        stack[sp].key = key;
        stack[sp].state = JS_OBJ_COLON;
        continue;
      } else {
        cur_val = json_new_val(alloc, SCL_JSON_STRING);
        if (!cur_val) {
          err = SCL_ERR_OUT_OF_MEMORY;
          goto cleanup;
        }
        cur_val->string_val = json_parse_string(alloc, &p, max_str_len);
        if (!cur_val->string_val) {
          err = SCL_ERR_PARSE;
          goto cleanup;
        }
      }
    } else if (c == 't') {
      if (p + 4 <= end && scl_strncmp(p, "true", 4) == 0) {
        cur_val = json_new_val(alloc, SCL_JSON_BOOL);
        if (!cur_val) {
          err = SCL_ERR_OUT_OF_MEMORY;
          goto cleanup;
        }
        cur_val->bool_val = 1;
        p += 4;
      } else {
        err = SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
    } else if (c == 'f') {
      if (p + 5 <= end && scl_strncmp(p, "false", 5) == 0) {
        cur_val = json_new_val(alloc, SCL_JSON_BOOL);
        if (!cur_val) {
          err = SCL_ERR_OUT_OF_MEMORY;
          goto cleanup;
        }
        cur_val->bool_val = 0;
        p += 5;
      } else {
        err = SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
    } else if (c == 'n') {
      if (p + 4 <= end && scl_strncmp(p, "null", 4) == 0) {
        cur_val = json_new_val(alloc, SCL_JSON_NULL);
        if (!cur_val) {
          err = SCL_ERR_OUT_OF_MEMORY;
          goto cleanup;
        }
        p += 4;
      } else {
        err = SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
    } else if (c == '-' || scl_isdigit((unsigned char)c)) {
      cur_val = json_new_val(alloc, SCL_JSON_INT64);
      if (!cur_val) {
        err = SCL_ERR_OUT_OF_MEMORY;
        goto cleanup;
      }
      scl_error_t nerr = json_parse_number(alloc, &p, cur_val, end);
      if (nerr != SCL_OK) {
        err = nerr;
        goto cleanup;
      }
    } else if (c == ':') {
      if (sp >= 0 && stack[sp].state == JS_OBJ_COLON) {
        p++;
        stack[sp].state = JS_OBJ_VAL;
        continue;
      }
      err = SCL_ERR_INVALID_ARG;
      goto cleanup;
    } else if (c == ',') {
      p++;
      if (sp >= 0) {
        if (stack[sp].state == JS_OBJ_NEXT) {
          stack[sp].state = JS_OBJ_START;
        } else if (stack[sp].state == JS_ARR_NEXT) {
          stack[sp].state = JS_ARR_VAL;
        } else {
          err = SCL_ERR_INVALID_ARG;
          goto cleanup;
        }
      } else {
        err = SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
      continue;
    } else if (c == '}') {
      p++;
      if (sp >= 0 && stack[sp].container->type == SCL_JSON_OBJECT &&
          (stack[sp].state == JS_OBJ_NEXT || stack[sp].state == JS_OBJ_START)) {
        cur_val = stack[sp].container;
        stack[sp].container = NULL;
        sp--;
      } else {
        err = SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
    } else if (c == ']') {
      p++;
      if (sp >= 0 && stack[sp].container->type == SCL_JSON_ARRAY) {
        cur_val = stack[sp].container;
        stack[sp].container = NULL;
        sp--;
      } else {
        err = SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
    } else {
      err = SCL_ERR_INVALID_ARG;
      goto cleanup;
    }

    if (cur_val) {
      int ret = json_attach_value(stack, sp, alloc, cur_val, &root);
      if (ret < 0) {
        err = (ret == -1) ? SCL_ERR_OUT_OF_MEMORY : SCL_ERR_INVALID_ARG;
        goto cleanup;
      }
      cur_val = NULL;
      if (ret == 0 && sp < 0) {
        json_skip_ws(&p);
        if (p < end) {
          err = SCL_ERR_INVALID_ARG;
          goto cleanup;
        }
        *out_root = root;
        return SCL_OK;
      }
    }
  }

  if (sp >= 0 || !root) {
    err = SCL_ERR_INVALID_ARG;
    goto cleanup;
  }
  *out_root = root;
  return SCL_OK;

cleanup:
  for (int i = 0; i <= sp; i++) {
    scl_parse_json_free(alloc, stack[i].container);
    scl_free(alloc, stack[i].key);
  }
  scl_parse_json_free(alloc, cur_val);
  scl_parse_json_free(alloc, root);
  return err;
}

scl_parse_json_type_t
scl_parse_json_get_type(const scl_parse_json_value_t *val) {
  if (!val)
    return SCL_JSON_NULL;
  return val->type;
}

int64_t scl_parse_json_get_int(const scl_parse_json_value_t *val) {
  if (!val)
    return 0;
  return val->int64_val;
}

double scl_parse_json_get_double(const scl_parse_json_value_t *val) {
  if (!val)
    return 0.0;
  if (val->type == SCL_JSON_INT64)
    return (double)val->int64_val;
  return val->double_val;
}

const char *scl_parse_json_get_string(const scl_parse_json_value_t *val) {
  if (!val)
    return NULL;
  return val->string_val;
}

int scl_parse_json_get_bool(const scl_parse_json_value_t *val) {
  if (!val)
    return 0;
  return val->bool_val;
}

scl_parse_json_value_t *
scl_parse_json_object_get(const scl_parse_json_value_t *obj, const char *key) {
  if (!obj || !key)
    return NULL;
  if (obj->type != SCL_JSON_OBJECT)
    return NULL;
  for (size_t i = 0; i < obj->child_count; i++) {
    if (obj->keys && obj->keys[i] && scl_strcmp(obj->keys[i], key) == 0)
      return obj->children[i];
  }
  return NULL;
}

scl_parse_json_value_t *
scl_parse_json_array_get(const scl_parse_json_value_t *arr, size_t index) {
  if (!arr)
    return NULL;
  if (arr->type != SCL_JSON_ARRAY)
    return NULL;
  if (index >= arr->child_count)
    return NULL;
  return arr->children[index];
}

size_t scl_parse_json_array_len(const scl_parse_json_value_t *arr) {
  if (!arr || arr->type != SCL_JSON_ARRAY)
    return 0;
  return arr->child_count;
}

size_t scl_parse_json_object_len(const scl_parse_json_value_t *obj) {
  if (!obj || obj->type != SCL_JSON_OBJECT)
    return 0;
  return obj->child_count;
}

void scl_parse_json_free(scl_allocator_t *alloc, scl_parse_json_value_t *node) {
  if (!node)
    return;

  for (size_t i = 0; i < node->child_count; i++)
    scl_parse_json_free(alloc, node->children[i]);

  if (node->type == SCL_JSON_STRING && node->string_val)
    scl_free(alloc, node->string_val);
  if (node->keys) {
    for (size_t j = 0; j < node->child_count; j++)
      scl_free(alloc, node->keys[j]);
    scl_free(alloc, node->keys);
  }
  scl_free(alloc, node->children);
  scl_free(alloc, node);
}