#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_tsv.h"
#include <stdlib.h>
#include <string.h>

#define TSV_INIT_BUF 4096

scl_error_t scl_parse_tsv_init(scl_parse_tsv_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    parser->state = SCL_TSV_STATE_FIELD_START;
    parser->buffer = (char *)malloc(TSV_INIT_BUF);
    if (__builtin_expect(!parser->buffer, 0)) return SCL_ERR_OUT_OF_MEMORY;
    parser->buffer_cap = TSV_INIT_BUF;
    parser->buffer_len = 0;
    parser->pos = 0;
    parser->eof = 0;
    return SCL_OK;
}

static int tsv_ensure_buf(scl_parse_tsv_t *parser, size_t needed) {
    if (needed <= parser->buffer_cap) return 0;
    size_t new_cap = parser->buffer_cap * 2;
    while (new_cap < needed) new_cap *= 2;
    char *nb = (char *)realloc(parser->buffer, new_cap);
    if (!nb) return -1;
    parser->buffer = nb;
    parser->buffer_cap = new_cap;
    return 0;
}

scl_error_t scl_parse_tsv_feed(scl_parse_tsv_t *parser, const char *data, size_t len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!data && len > 0, 0)) return SCL_ERR_NULL_PTR;

    if (tsv_ensure_buf(parser, parser->buffer_len + len + 1) != 0)
        return SCL_ERR_OUT_OF_MEMORY;

    memcpy(parser->buffer + parser->buffer_len, data, len);
    parser->buffer_len += len;
    parser->buffer[parser->buffer_len] = '\0';
    return SCL_OK;
}

scl_error_t scl_parse_tsv_next_field(scl_parse_tsv_t *parser, const char **out, size_t *out_len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;

    if (parser->pos >= parser->buffer_len) return SCL_ERR_EMPTY;

    size_t start = parser->pos;
    while (parser->pos < parser->buffer_len) {
        char c = parser->buffer[parser->pos];
        if (c == '\t' || c == '\n' || c == '\r') break;
        parser->pos++;
    }

    *out = &parser->buffer[start];
    *out_len = parser->pos - start;

    if (parser->pos < parser->buffer_len) {
        if (parser->buffer[parser->pos] == '\t') {
            parser->state = SCL_TSV_STATE_FIELD;
            parser->pos++;
        }
    }
    return SCL_OK;
}

scl_error_t scl_parse_tsv_next_row(scl_parse_tsv_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;

    while (parser->pos < parser->buffer_len) {
        char c = parser->buffer[parser->pos];
        if (c == '\n') {
            parser->pos++;
            parser->state = SCL_TSV_STATE_FIELD_START;
            return SCL_OK;
        }
        if (c == '\r') {
            parser->state = SCL_TSV_STATE_CR;
            parser->pos++;
            return SCL_OK;
        }
        parser->pos++;
    }

    if (parser->state == SCL_TSV_STATE_CR && parser->pos < parser->buffer_len && parser->buffer[parser->pos] == '\n') {
        parser->pos++;
    }
    parser->state = SCL_TSV_STATE_FIELD_START;
    return SCL_ERR_EMPTY;
}

scl_error_t scl_parse_tsv_destroy(scl_parse_tsv_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    free(parser->buffer);
    parser->buffer = NULL;
    parser->buffer_cap = 0;
    parser->buffer_len = 0;
    parser->pos = 0;
    return SCL_OK;
}
