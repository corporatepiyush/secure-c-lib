#ifndef SCL_PARSE_DOCX_H
#define SCL_PARSE_DOCX_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char *filename;
    FILE *fp;
    unsigned char *zip_buf;
    size_t zip_size;
    char *text_buf;
    size_t text_len;
    size_t text_cap;
} scl_parse_docx_t;

scl_error_t scl_parse_docx_open(scl_parse_docx_t *parser, const char *filename);
scl_error_t scl_parse_docx_get_text(scl_parse_docx_t *parser, const char **out, size_t *out_len);
scl_error_t scl_parse_docx_close(scl_parse_docx_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
