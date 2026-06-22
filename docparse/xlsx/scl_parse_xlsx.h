#ifndef SCL_PARSE_XLSX_H
#define SCL_PARSE_XLSX_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "../../common/scl_common.h"

typedef struct {
    scl_allocator_t *alloc;
    char *filename;
    FILE *fp;
    unsigned char *zip_buf;
    size_t zip_size;
    char **shared_strings;
    size_t shared_count;
    size_t sheet_count;
    char **sheet_names;
    char **sheet_data;
} scl_parse_xlsx_t;

scl_error_t scl_parse_xlsx_open(scl_allocator_t *alloc, scl_parse_xlsx_t *parser, const char *filename);
scl_error_t scl_parse_xlsx_get_sheets_count(scl_parse_xlsx_t *parser, int *out);
scl_error_t scl_parse_xlsx_get_sheet_name(scl_parse_xlsx_t *parser, int index, const char **out, size_t *out_len);
scl_error_t scl_parse_xlsx_get_cell(scl_parse_xlsx_t *parser, int sheet_idx, const char *cell_ref, const char **out, size_t *out_len);
scl_error_t scl_parse_xlsx_close(scl_parse_xlsx_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
