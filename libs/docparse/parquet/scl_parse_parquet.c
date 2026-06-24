#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_parquet.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#define PARQUET_MAGIC "PAR1"

/* Bounds-checked varint reader: never reads at or past `end`. */
static uint64_t parquet_read_varint(const unsigned char **p, const unsigned char *end) {
    uint64_t v = 0;
    int shift = 0;
    while (*p < end && shift < 64) {
        unsigned char b = *(*p)++;
        v |= (uint64_t)(b & 0x7F) << shift;
        shift += 7;
        if (!(b & 0x80)) return v;
    }
    return v;
}

/* Advance `p` by `n` bytes, clamped so it never moves past `end`. */
static const unsigned char *parquet_clamp(const unsigned char *p, uint64_t n,
                                          const unsigned char *end) {
    return (n <= (uint64_t)(end - p)) ? p + n : end;
}

static int parquet_parse_footer(scl_parse_parquet_t *parser) {
    size_t sz = parser->buf_size;
    unsigned char *buf = parser->buf;

    if (sz < 12) return -1;

    if (scl_memcmp(buf + sz - 4, PARQUET_MAGIC, 4) != 0)
        return -1;

    uint32_t footer_len = (uint32_t)buf[sz - 8] | ((uint32_t)buf[sz - 7] << 8) |
                          ((uint32_t)buf[sz - 6] << 16) | ((uint32_t)buf[sz - 5] << 24);

    if (footer_len == 0 || footer_len > sz - 8) return -1;

    size_t footer_start = sz - 8 - footer_len;
    const unsigned char *fp = buf + footer_start;
    const unsigned char *fend = fp + footer_len;

    if (fp >= fend) return -1;

    while (fp < fend) {
        unsigned char ft = *fp++;
        if (ft == 0x00) break;

        int64_t id = (ft >> 2) & 0x0F;
        (void)id;

        ft = ft & 0x03;
        switch (ft) {
        case 0:
            parquet_read_varint(&fp, fend);
            break;
        case 1:
            fp = parquet_clamp(fp, 8, fend);
            break;
        case 2:
        {
            uint64_t len = parquet_read_varint(&fp, fend);
            if (id == 2) {
                const unsigned char *list_end = parquet_clamp(fp, len, fend);
                uint64_t elem_count = parquet_read_varint(&fp, list_end);
                /* Cap allocation to what the buffer could possibly describe
                 * (one byte minimum per element) to reject absurd counts. */
                if (elem_count > (uint64_t)(list_end - fp)) elem_count = (uint64_t)(list_end - fp);
                parser->num_columns = (int)elem_count;
                parser->column_names = (char **)scl_calloc(parser->alloc, (size_t)elem_count, sizeof(char *), _Alignof(max_align_t));
                parser->column_types = (int *)scl_calloc(parser->alloc, (size_t)elem_count, sizeof(int), _Alignof(max_align_t));
                if (elem_count > 0 && (!parser->column_names || !parser->column_types)) {
                    scl_free(parser->alloc, parser->column_names);
                    scl_free(parser->alloc, parser->column_types);
                    parser->column_names = NULL; parser->column_types = NULL;
                    parser->num_columns = 0;
                    return SCL_ERR_OUT_OF_MEMORY;
                }

                for (uint64_t ci = 0; ci < elem_count && fp < list_end; ci++) {
                    while (fp < list_end) {
                        unsigned char sft = *fp++;
                        if (sft == 0x00) break;
                        int64_t sid = (sft >> 2) & 0x0F;
                        int stype = sft & 0x03;
                        if (stype == 2 && sid == 1 && ci < elem_count) {
                            uint64_t nlen = parquet_read_varint(&fp, list_end);
                            if (nlen <= (uint64_t)(list_end - fp)) {
                                parser->column_names[ci] = (char *)scl_alloc(parser->alloc, (size_t)nlen + 1, _Alignof(max_align_t));
                                if (parser->column_names[ci]) {
                                    scl_memcpy(parser->column_names[ci], fp, (size_t)nlen);
                                    parser->column_names[ci][nlen] = '\0';
                                }
                                fp += nlen;
                            } else {
                                fp = list_end;
                            }
                        } else if (stype == 0) {
                            parquet_read_varint(&fp, list_end);
                        } else if (stype == 2) {
                            uint64_t slen = parquet_read_varint(&fp, list_end);
                            fp = parquet_clamp(fp, slen, list_end);
                        } else {
                            break;
                        }
                    }
                }
                fp = list_end;
            } else {
                fp = parquet_clamp(fp, len, fend);
            }
            break;
        }
        case 3:
        {
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

    fp = buf + footer_start;
    fend = fp + footer_len;
    while (fp < fend) {
        unsigned char ft = *fp++;
        if (ft == 0x00) break;
        if ((ft & 3) == 0) {
            int64_t val = (int64_t)parquet_read_varint(&fp, fend);
            int64_t fid = (ft >> 2) & 0x0F;
            if (fid == 3) {
                parser->num_rows = val;
            }
        } else if ((ft & 3) == 1) {
            if ((uint64_t)(fend - fp) >= 8) {
                if (((ft >> 2) & 0x0F) == 3) {
                    scl_memcpy(&parser->num_rows, fp, 8);
                }
                fp += 8;
            } else {
                fp = fend;
            }
        } else if ((ft & 3) == 2) {
            uint64_t len = parquet_read_varint(&fp, fend);
            fp = parquet_clamp(fp, len, fend);
        } else if ((ft & 3) == 3) {
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

scl_error_t scl_parse_parquet_open(scl_allocator_t *alloc, scl_parse_parquet_t *parser, const char *filename) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!filename)) return SCL_ERR_NULL_PTR;

    (void)scl_memset(parser, 0, sizeof(*parser));
    parser->alloc = alloc;

    parser->filename = scl_strdup(alloc, filename);
    if (!parser->filename) return SCL_ERR_OUT_OF_MEMORY;

    FILE *fp = fopen(filename, "rb");
    if (!fp) { scl_free(alloc, parser->filename); return SCL_ERR_NOT_FOUND; }
    parser->fp = fp;

    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    if (sz < 0) { scl_parse_parquet_close(parser); return SCL_ERR_ALLOC; }
    rewind(fp);
    parser->buf_size = (size_t)sz;
    parser->buf = (unsigned char *)scl_alloc(alloc, (size_t)sz, _Alignof(max_align_t));
    if (!parser->buf) { scl_parse_parquet_close(parser); return SCL_ERR_OUT_OF_MEMORY; }
    if (fread(parser->buf, 1, (size_t)sz, fp) != (size_t)sz) {
        scl_parse_parquet_close(parser); return SCL_ERR_ALLOC;
    }

    if (parser->buf_size < 8 || scl_memcmp(parser->buf, PARQUET_MAGIC, 4) != 0) {
        scl_parse_parquet_close(parser); return SCL_ERR_INVALID_ARG;
    }

    parquet_parse_footer(parser);

    return SCL_OK;
}

scl_error_t scl_parse_parquet_get_row_count(scl_parse_parquet_t *parser, int64_t *out) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    *out = parser->num_rows;
    return SCL_OK;
}

scl_error_t scl_parse_parquet_get_column_count(scl_parse_parquet_t *parser, int *out) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    *out = parser->num_columns;
    return SCL_OK;
}

scl_error_t scl_parse_parquet_get_column_name(scl_parse_parquet_t *parser, int index,
                                               const char **out, size_t *out_len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    if (index < 0 || index >= parser->num_columns) return SCL_ERR_INVALID_INDEX;
    if (!parser->column_names[index]) return SCL_ERR_NOT_FOUND;
    *out = parser->column_names[index];
    if (out_len) *out_len = scl_strlen(parser->column_names[index]);
    return SCL_OK;
}

scl_error_t scl_parse_parquet_close(scl_parse_parquet_t *parser) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (parser->fp) fclose(parser->fp);
    parser->fp = NULL;
    scl_free(parser->alloc, parser->filename); parser->filename = NULL;
    scl_free(parser->alloc, parser->buf); parser->buf = NULL;
    if (parser->column_names) {
        for (int i = 0; i < parser->num_columns; i++)
            scl_free(parser->alloc, parser->column_names[i]);
        scl_free(parser->alloc, parser->column_names);
        parser->column_names = NULL;
    }
    scl_free(parser->alloc, parser->column_types);
    parser->column_types = NULL;
    parser->buf_size = 0;
    return SCL_OK;
}
