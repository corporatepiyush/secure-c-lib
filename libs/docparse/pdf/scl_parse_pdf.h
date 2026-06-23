#ifndef SCL_PARSE_PDF_H
#define SCL_PARSE_PDF_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

#define SCL_PDF_MAX_OBJECTS 4096

typedef struct {
    int obj_num;
    int gen_num;
    size_t offset;
    int in_use;
} scl_parse_pdf_xref_entry_t;

typedef struct {
    scl_allocator_t *alloc;
    char *filename;
    FILE *fp;
    unsigned char *buf;
    size_t buf_size;
    int version_major;
    int version_minor;
    size_t xref_offset;
    int xref_count;
    scl_parse_pdf_xref_entry_t xref_table[SCL_PDF_MAX_OBJECTS];
    int root_obj;
    int root_gen;
    int info_obj;
    int info_gen;
    int pages_obj;
    int pages_gen;
    int page_count;
    char *metadata;
    size_t metadata_len;
} scl_parse_pdf_t;

scl_error_t scl_parse_pdf_open(scl_allocator_t *alloc, scl_parse_pdf_t *parser, const char *filename);
scl_error_t scl_parse_pdf_get_page_count(scl_parse_pdf_t *parser, int *out);
scl_error_t scl_parse_pdf_get_info(scl_parse_pdf_t *parser, const char *key, char *out, size_t *out_len);
scl_error_t scl_parse_pdf_close(scl_parse_pdf_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
