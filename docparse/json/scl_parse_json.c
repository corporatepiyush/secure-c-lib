#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_json.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

static void json_skip_ws(const char **p) {
    while (**p && (unsigned char)**p <= ' ') (*p)++;
}

static scl_parse_json_value_t *json_new_val(scl_parse_json_type_t type) {
    scl_parse_json_value_t *v = (scl_parse_json_value_t *)calloc(1, sizeof(scl_parse_json_value_t));
    if (!v) return NULL;
    v->type = type;
    return v;
}

static int json_add_child(scl_parse_json_value_t *parent, scl_parse_json_value_t *child) {
    if (parent->child_count >= parent->child_cap) {
        size_t nc = parent->child_cap ? parent->child_cap * 2 : 4;
        scl_parse_json_value_t **ch = (scl_parse_json_value_t **)realloc(parent->children, nc * sizeof(void *));
        if (!ch) return -1;
        if (parent->type == SCL_JSON_OBJECT) {
            char **ks = (char **)realloc(parent->keys, nc * sizeof(char *));
            if (!ks) return -1;
            parent->keys = ks;
        }
        parent->children = ch;
        parent->child_cap = nc;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    return 0;
}

static int json_add_child_key(scl_parse_json_value_t *parent, scl_parse_json_value_t *child, char *key) {
    if (json_add_child(parent, child) != 0) return -1;
    parent->keys[parent->child_count - 1] = key;
    return 0;
}

static scl_error_t json_parse_value(const char **p, scl_parse_json_value_t **out);

static char *json_parse_string(const char **p) {
    if (**p != '"') return NULL;
    (*p)++;
    size_t cap = 256, len = 0;
    char *s = (char *)malloc(cap);
    if (!s) return NULL;

    while (**p && **p != '"') {
        if (**p == '\\') {
            (*p)++;
            char c = **p;
            switch (c) {
            case '"':  case '\\': case '/':
                s[len++] = c;
                break;
            case 'b':  s[len++] = '\b'; break;
            case 'f':  s[len++] = '\f'; break;
            case 'n':  s[len++] = '\n'; break;
            case 'r':  s[len++] = '\r'; break;
            case 't':  s[len++] = '\t'; break;
            case 'u': {
                // Basic unicode parse - store raw
                char hex[5] = {0};
                for (int i = 0; i < 4; i++) { (*p)++; hex[i] = **p; }
                unsigned long cp = strtoul(hex, NULL, 16);
                if (cp < 128) { s[len++] = (char)cp; }
                else {
                    s[len++] = '?'; // simplified
                }
                break;
            }
            default:   s[len++] = c; break;
            }
        } else {
            s[len++] = **p;
        }
        if (len + 4 >= cap) {
            cap *= 2;
            char *ns = (char *)realloc(s, cap);
            if (!ns) { free(s); return NULL; }
            s = ns;
        }
        (*p)++;
    }
    if (**p == '"') (*p)++;
    s[len] = '\0';

    char *ns = (char *)realloc(s, len + 1);
    return ns ? ns : s;
}

static scl_error_t json_parse_number(const char **p, scl_parse_json_value_t *val) {
    char *end;
    errno = 0;
    const char *start = *p;

    int is_float = 0;
    while (**p && (isdigit((unsigned char)**p) || **p == '-' || **p == '+' || **p == '.' || **p == 'e' || **p == 'E')) {
        if (**p == '.' || **p == 'e' || **p == 'E') is_float = 1;
        (*p)++;
    }

    size_t nlen = *p - start;
    char *num = (char *)malloc(nlen + 1);
    if (!num) return SCL_ERR_OUT_OF_MEMORY;
    memcpy(num, start, nlen);
    num[nlen] = '\0';

    if (is_float) {
        val->type = SCL_JSON_DOUBLE;
        val->double_val = strtod(num, &end);
    } else {
        val->type = SCL_JSON_INT64;
        val->int64_val = (int64_t)strtoll(num, &end, 10);
    }
    free(num);
    return SCL_OK;
}

static scl_error_t json_parse_object(const char **p, scl_parse_json_value_t *val) {
    val->type = SCL_JSON_OBJECT;
    (*p)++; // skip '{'
    json_skip_ws(p);
    if (**p == '}') { (*p)++; return SCL_OK; }

    while (1) {
        json_skip_ws(p);
        char *key = json_parse_string(p);
        if (!key) return SCL_ERR_INVALID_ARG;
        json_skip_ws(p);
        if (**p != ':') { free(key); return SCL_ERR_INVALID_ARG; }
        (*p)++;
        json_skip_ws(p);
        scl_parse_json_value_t *child = NULL;
        scl_error_t err = json_parse_value(p, &child);
        if (err != SCL_OK || !child) { free(key); return err; }
        if (json_add_child_key(val, child, key) != 0) {
            free(key); scl_parse_json_free(child); return SCL_ERR_OUT_OF_MEMORY;
        }
        json_skip_ws(p);
        if (**p == '}') { (*p)++; return SCL_OK; }
        if (**p != ',') return SCL_ERR_INVALID_ARG;
        (*p)++;
    }
}

static scl_error_t json_parse_array(const char **p, scl_parse_json_value_t *val) {
    val->type = SCL_JSON_ARRAY;
    (*p)++;
    json_skip_ws(p);
    if (**p == ']') { (*p)++; return SCL_OK; }

    while (1) {
        scl_parse_json_value_t *child = NULL;
        scl_error_t err = json_parse_value(p, &child);
        if (err != SCL_OK) return err;
        if (!child) return SCL_ERR_INVALID_ARG;
        if (json_add_child(val, child) != 0) {
            scl_parse_json_free(child); return SCL_ERR_OUT_OF_MEMORY;
        }
        json_skip_ws(p);
        if (**p == ']') { (*p)++; return SCL_OK; }
        if (**p != ',') return SCL_ERR_INVALID_ARG;
        (*p)++;
    }
}

static scl_error_t json_parse_value(const char **p, scl_parse_json_value_t **out) {
    json_skip_ws(p);
    if (!**p) return SCL_ERR_EMPTY;

    scl_parse_json_value_t *val = NULL;

    switch (**p) {
    case '{':
        val = json_new_val(SCL_JSON_OBJECT);
        if (!val) return SCL_ERR_OUT_OF_MEMORY;
        return json_parse_object(p, val);
    case '[':
        val = json_new_val(SCL_JSON_ARRAY);
        if (!val) return SCL_ERR_OUT_OF_MEMORY;
        return json_parse_array(p, val);
    case '"':
        val = json_new_val(SCL_JSON_STRING);
        if (!val) return SCL_ERR_OUT_OF_MEMORY;
        val->string_val = json_parse_string(p);
        if (!val->string_val) { free(val); return SCL_ERR_OUT_OF_MEMORY; }
        *out = val;
        return SCL_OK;
    case 't':
        if (strncmp(*p, "true", 4) == 0) {
            val = json_new_val(SCL_JSON_BOOL);
            if (!val) return SCL_ERR_OUT_OF_MEMORY;
            val->bool_val = 1;
            *p += 4;
            *out = val;
            return SCL_OK;
        }
        return SCL_ERR_INVALID_ARG;
    case 'f':
        if (strncmp(*p, "false", 5) == 0) {
            val = json_new_val(SCL_JSON_BOOL);
            if (!val) return SCL_ERR_OUT_OF_MEMORY;
            val->bool_val = 0;
            *p += 5;
            *out = val;
            return SCL_OK;
        }
        return SCL_ERR_INVALID_ARG;
    case 'n':
        if (strncmp(*p, "null", 4) == 0) {
            val = json_new_val(SCL_JSON_NULL);
            if (!val) return SCL_ERR_OUT_OF_MEMORY;
            *p += 4;
            *out = val;
            return SCL_OK;
        }
        return SCL_ERR_INVALID_ARG;
    default:
        if (**p == '-' || isdigit((unsigned char)**p)) {
            val = json_new_val(SCL_JSON_INT64);
            if (!val) return SCL_ERR_OUT_OF_MEMORY;
            scl_error_t err = json_parse_number(p, val);
            if (err != SCL_OK) { scl_parse_json_free(val); return err; }
            *out = val;
            return SCL_OK;
        }
        return SCL_ERR_INVALID_ARG;
    }
}

scl_error_t scl_parse_json_parse(const char *json_str, scl_parse_json_value_t **out_root) {
    if (__builtin_expect(!json_str, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out_root, 0)) return SCL_ERR_NULL_PTR;

    const char *p = json_str;
    scl_parse_json_value_t *root = NULL;
    scl_error_t err = json_parse_value(&p, &root);
    if (err != SCL_OK) return err;
    if (!root) return SCL_ERR_INVALID_ARG;
    *out_root = root;
    return SCL_OK;
}

scl_parse_json_type_t scl_parse_json_get_type(const scl_parse_json_value_t *val) {
    if (!val) return SCL_JSON_NULL;
    return val->type;
}

int64_t scl_parse_json_get_int(const scl_parse_json_value_t *val) {
    if (!val) return 0;
    return val->int64_val;
}

double scl_parse_json_get_double(const scl_parse_json_value_t *val) {
    if (!val) return 0.0;
    if (val->type == SCL_JSON_INT64) return (double)val->int64_val;
    return val->double_val;
}

const char *scl_parse_json_get_string(const scl_parse_json_value_t *val) {
    if (!val) return NULL;
    return val->string_val;
}

int scl_parse_json_get_bool(const scl_parse_json_value_t *val) {
    if (!val) return 0;
    return val->bool_val;
}

scl_parse_json_value_t *scl_parse_json_object_get(const scl_parse_json_value_t *obj, const char *key) {
    if (!obj || !key) return NULL;
    if (obj->type != SCL_JSON_OBJECT) return NULL;
    for (size_t i = 0; i < obj->child_count; i++) {
        if (obj->keys && obj->keys[i] && strcmp(obj->keys[i], key) == 0)
            return obj->children[i];
    }
    return NULL;
}

scl_parse_json_value_t *scl_parse_json_array_get(const scl_parse_json_value_t *arr, size_t index) {
    if (!arr) return NULL;
    if (arr->type != SCL_JSON_ARRAY) return NULL;
    if (index >= arr->child_count) return NULL;
    return arr->children[index];
}

size_t scl_parse_json_array_len(const scl_parse_json_value_t *arr) {
    if (!arr || arr->type != SCL_JSON_ARRAY) return 0;
    return arr->child_count;
}

size_t scl_parse_json_object_len(const scl_parse_json_value_t *obj) {
    if (!obj || obj->type != SCL_JSON_OBJECT) return 0;
    return obj->child_count;
}

void scl_parse_json_free(scl_parse_json_value_t *root) {
    if (!root) return;
    for (size_t i = 0; i < root->child_count; i++) {
        scl_parse_json_free(root->children[i]);
    }
    if (root->type == SCL_JSON_STRING && root->string_val)
        free(root->string_val);
    if (root->keys) {
        for (size_t i = 0; i < root->child_count; i++)
            free(root->keys[i]);
        free(root->keys);
    }
    free(root->children);
    free(root);
}
