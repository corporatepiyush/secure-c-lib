#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_parquet.h"
#include <stdlib.h>
#include <string.h>

#define PARQUET_MAGIC "PAR1"

/* Thrift compact protocol helpers */
static uint64_t parquet_read_varint(const unsigned char **p) {
    uint64_t v = 0;
    int shift = 0;
    while (1) {
        if (shift >= 64) break;
        unsigned char b = *(*p)++;
        v |= (uint64_t)(b & 0x7F) << shift;
        shift += 7;
        if (!(b & 0x80)) return v;
    }
    return v;
}

/* Minimal Thrift compact struct parsing for Parquet metadata */
static int parquet_parse_footer(scl_parse_parquet_t *parser) {
    size_t sz = parser->buf_size;
    unsigned char *buf = parser->buf;

    if (sz < 12) return -1;

    /* Check end magic */
    if (memcmp(buf + sz - 4, PARQUET_MAGIC, 4) != 0)
        return -1;

    /* Read footer length (4 bytes, little-endian) before end magic */
    uint32_t footer_len = (uint32_t)buf[sz - 8] | ((uint32_t)buf[sz - 7] << 8) |
                          ((uint32_t)buf[sz - 6] << 16) | ((uint32_t)buf[sz - 5] << 24);

    if (footer_len == 0 || footer_len > sz - 8) return -1;

    size_t footer_start = sz - 8 - footer_len;
    const unsigned char *fp = buf + footer_start;
    const unsigned char *fend = fp + footer_len;

    if (fp >= fend) return -1;

    /* FileMetaData is a Thrift compact struct
       Fields (ids):
       1: version (i32)
       2: schema (list of SchemaElement)
       3: num_rows (i64)
       4: row_groups (list of RowGroup)
       5: key_value_metadata (optional list)
       6: created_by (string, optional)
    */

    /* Read struct header (field stop at end) */
    /* Compact protocol: first byte is field type or stop */
    while (fp < fend) {
        unsigned char ft = *fp++;
        if (ft == 0x00) break; /* stop field */

        int64_t id = (ft >> 2) & 0x0F;  /* field id from compact header */
        (void)id;

        ft = ft & 0x03; /* field type */
        switch (ft) {
        case 0: /* varint */
            parquet_read_varint(&fp);
            break;
        case 1: /* int64 */
            fp += 8;
            break;
        case 2: /* length-delimited */
        {
            uint64_t len = parquet_read_varint(&fp);
            /* Field 2 = schema */
            /* Field 4 = row_groups */
            /* Field 5 = key_value_metadata */
            if (id == 2) {
                /* schema elements list */
                const unsigned char *list_end = fp + len;
                /* List header: varint count */
                uint64_t elem_count = parquet_read_varint(&fp);
                parser->num_columns = (int)elem_count;
                /* Allocate column names */
                parser->column_names = (char **)calloc((size_t)elem_count, sizeof(char *));
                parser->column_types = (int *)calloc((size_t)elem_count, sizeof(int));

                for (uint64_t ci = 0; ci < elem_count && fp < list_end; ci++) {
                    /* Each element is a struct with its own stop */
                    /* Read name field (id 1) */
                    while (fp < list_end) {
                        unsigned char sft = *fp++;
                        if (sft == 0x00) break;
                        int64_t sid = (sft >> 2) & 0x0F;
                        int stype = sft & 0x03;
                        if (stype == 2 && sid == 1 && ci < elem_count) {
                            uint64_t nlen = parquet_read_varint(&fp);
                            if (fp + nlen <= list_end) {
                                parser->column_names[ci] = (char *)malloc((size_t)nlen + 1);
                                if (parser->column_names[ci]) {
                                    memcpy(parser->column_names[ci], fp, (size_t)nlen);
                                    parser->column_names[ci][nlen] = '\0';
                                }
                                fp += nlen;
                            }
                        } else if (stype == 0) {
                            parquet_read_varint(&fp);
                        } else if (stype == 2) {
                            uint64_t slen = parquet_read_varint(&fp);
                            fp += slen;
                        } else {
                            break;
                        }
                    }
                }
                fp = list_end;
            } else if (id == 3) {
                /* num_rows */
                const unsigned char *old = fp;
                /* It's a length-delimited field containing an i64, but Parquet encodes it differently */
                /* num_rows is actually field 3 of FileMetaData with type = 0 (varint) */
                fp = old + len;
            } else {
                fp += len;
            }
            break;
        }
        case 3: /* struct */
        {
            /* Skip struct fields */
            int depth = 1;
            while (depth > 0 && fp < fend) {
                unsigned char bt = *fp++;
                if (bt == 0x00) depth--;
                else if ((bt & 3) == 3) depth++;
            }
            break;
        }
        default:
            return -1;
        }
    }

    /* Try to find num_rows from field 3 which should be a varint */
    /* Re-parse looking specifically for num_rows */
    fp = buf + footer_start;
    fend = fp + footer_len;
    while (fp < fend) {
        unsigned char ft = *fp++;
        if (ft == 0x00) break;
        if ((ft & 3) == 0) { /* varint */
            int64_t val = (int64_t)parquet_read_varint(&fp);
            int64_t fid = (ft >> 2) & 0x0F;
            if (fid == 3) {
                parser->num_rows = val;
            }
        } else if ((ft & 3) == 1) { /* i64 */
            if (((ft >> 2) & 0x0F) == 3) {
                memcpy(&parser->num_rows, fp, 8);
            }
            fp += 8;
        } else if ((ft & 3) == 2) { /* length-delimited */
            uint64_t len = parquet_read_varint(&fp);
            fp += len;
        } else if ((ft & 3) == 3) { /* struct */
            int depth = 1;
            while (depth > 0 && fp < fend) {
                unsigned char bt = *fp++;
                if (bt == 0x00) depth--;
                else if ((bt & 3) == 3) depth++;
            }
        }
    }

    return 0;
}

scl_error_t scl_parse_parquet_open(scl_parse_parquet_t *parser, const char *filename) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!filename, 0)) return SCL_ERR_NULL_PTR;

    memset(parser, 0, sizeof(*parser));

    parser->filename = strdup(filename);
    if (!parser->filename) return SCL_ERR_OUT_OF_MEMORY;

    FILE *fp = fopen(filename, "rb");
    if (!fp) { free(parser->filename); return SCL_ERR_NOT_FOUND; }
    parser->fp = fp;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz < 0) { scl_parse_parquet_close(parser); return SCL_ERR_ALLOC; }
    rewind(fp);
    parser->buf_size = (size_t)sz;
    parser->buf = (unsigned char *)malloc((size_t)sz);
    if (!parser->buf) { scl_parse_parquet_close(parser); return SCL_ERR_OUT_OF_MEMORY; }
    if (fread(parser->buf, 1, (size_t)sz, fp) != (size_t)sz) {
        scl_parse_parquet_close(parser); return SCL_ERR_ALLOC;
    }

    /* Verify magic */
    if (parser->buf_size < 8 || memcmp(parser->buf, PARQUET_MAGIC, 4) != 0) {
        scl_parse_parquet_close(parser); return SCL_ERR_INVALID_ARG;
    }

    /* Parse footer metadata */
    parquet_parse_footer(parser);

    return SCL_OK;
}

scl_error_t scl_parse_parquet_get_row_count(scl_parse_parquet_t *parser, int64_t *out) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    *out = parser->num_rows;
    return SCL_OK;
}

scl_error_t scl_parse_parquet_get_column_count(scl_parse_parquet_t *parser, int *out) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    *out = parser->num_columns;
    return SCL_OK;
}

scl_error_t scl_parse_parquet_get_column_name(scl_parse_parquet_t *parser, int index,
                                               const char **out, size_t *out_len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    if (index < 0 || index >= parser->num_columns) return SCL_ERR_INVALID_INDEX;
    if (!parser->column_names[index]) return SCL_ERR_NOT_FOUND;
    *out = parser->column_names[index];
    if (out_len) *out_len = strlen(parser->column_names[index]);
    return SCL_OK;
}

scl_error_t scl_parse_parquet_close(scl_parse_parquet_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (parser->fp) fclose(parser->fp);
    parser->fp = NULL;
    free(parser->filename); parser->filename = NULL;
    free(parser->buf); parser->buf = NULL;
    if (parser->column_names) {
        for (int i = 0; i < parser->num_columns; i++)
            free(parser->column_names[i]);
        free(parser->column_names);
        parser->column_names = NULL;
    }
    free(parser->column_types);
    parser->column_types = NULL;
    parser->buf_size = 0;
    return SCL_OK;
}
