#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_xlsx.h"
#include <stdlib.h>
#include <string.h>

/* ZIP helpers */
#define ZIP_LOCAL_HDR_SZ 30
#define ZIP_LOCAL_SIG 0x04034b50

static uint32_t xlsx_read_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t xlsx_read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int xlsx_zip_find_file(const unsigned char *buf, size_t sz, const char *name,
                               unsigned char **out, size_t *out_len) {
    size_t pos = 0;
    while (pos + ZIP_LOCAL_HDR_SZ <= sz) {
        uint32_t sig = xlsx_read_le32(buf + pos);
        if (sig != ZIP_LOCAL_SIG) break;
        uint32_t comp_sz = xlsx_read_le32(buf + pos + 18);
        uint32_t uncomp_sz = xlsx_read_le32(buf + pos + 22);
        uint16_t name_len = xlsx_read_le16(buf + pos + 26);
        uint16_t extra_len = xlsx_read_le16(buf + pos + 28);
        size_t hdr_end = pos + ZIP_LOCAL_HDR_SZ + name_len + extra_len;
        if (hdr_end > sz) break;
        if ((size_t)name_len == strlen(name) &&
            memcmp(buf + pos + ZIP_LOCAL_HDR_SZ, name, name_len) == 0) {
            *out = (unsigned char *)(buf + hdr_end);
            *out_len = uncomp_sz;
            return 0;
        }
        pos = hdr_end + comp_sz;
    }
    return -1;
}

/* Simple XML string extraction helpers */
static const char *xlsx_xml_strstr(const char *hay, size_t hlen, const char *needle) {
    size_t nlen = strlen(needle);
    for (size_t i = 0; i + nlen <= hlen; i++) {
        if (memcmp(hay + i, needle, nlen) == 0) return hay + i;
    }
    return NULL;
}

static char *xlsx_extract_tag_value(const char *xml, size_t xlen, const char *tag) {
    char open[128], close[128];
    snprintf(open, sizeof(open), "<%s>", tag);
    snprintf(close, sizeof(close), "</%s>", tag);
    const char *start = xlsx_xml_strstr(xml, xlen, open);
    if (!start) {
        snprintf(open, sizeof(open), "<%s ", tag);
        start = xlsx_xml_strstr(xml, xlen, open);
        if (!start) return NULL;
        const char *gt = (const char *)memchr(start, '>', xlen - (size_t)(start - xml));
        if (!gt) return NULL;
        start = gt + 1;
    } else {
        start += strlen(open);
    }
    const char *end = xlsx_xml_strstr(start, xlen - (size_t)(start - xml), close);
    if (!end) return NULL;
    size_t slen = (size_t)(end - start);
    char *val = (char *)malloc(slen + 1);
    if (!val) return NULL;
    memcpy(val, start, slen);
    val[slen] = '\0';
    return val;
}

static int xlsx_parse_shared_strings(scl_parse_xlsx_t *parser, const unsigned char *data, size_t len) {
    const char *xml = (const char *)data;
    size_t xlen = len;
    const char *end = xml + xlen;

    /* Count <si> tags */
    int si_count = 0;
    const char *ptr = xml;
    while ((ptr = xlsx_xml_strstr(ptr, (size_t)(end - ptr), "<si>")) != NULL) {
        si_count++;
        ptr += 4;
    }
    if (si_count == 0) return 0;

    parser->shared_strings = (char **)calloc((size_t)si_count, sizeof(char *));
    if (!parser->shared_strings) return -1;

    ptr = xml;
    int idx = 0;
    while (idx < si_count && ptr < end) {
        const char *si_start = xlsx_xml_strstr(ptr, (size_t)(end - ptr), "<si>");
        if (!si_start) break;
        const char *si_end = xlsx_xml_strstr(si_start, (size_t)(end - si_start), "</si>");
        if (!si_end) break;
        si_end += 5;

        size_t si_len = (size_t)(si_end - si_start);
        char *t_val = xlsx_extract_tag_value(si_start, si_len, "t");
        if (t_val) {
            parser->shared_strings[idx] = t_val;
        } else {
            parser->shared_strings[idx] = strdup("");
        }
        idx++;
        ptr = si_end;
    }
    parser->shared_count = (size_t)idx;
    return 0;
}

static int xlsx_parse_workbook(scl_parse_xlsx_t *parser, const unsigned char *data, size_t len) {
    const char *xml = (const char *)data;
    size_t xlen = len;

    /* Count sheets */
    int count = 0;
    const char *ptr = xml;
    while ((ptr = xlsx_xml_strstr(ptr, (size_t)((xml + xlen) - ptr), "<sheet ")) != NULL) {
        count++;
        ptr += 7;
    }
    parser->sheet_count = (size_t)count;
    if (count == 0) return 0;

    parser->sheet_names = (char **)calloc((size_t)count, sizeof(char *));
    parser->sheet_data = (char **)calloc((size_t)count, sizeof(char *));
    if (!parser->sheet_names || !parser->sheet_data) return -1;

    ptr = xml;
    for (int i = 0; i < count; i++) {
        ptr = xlsx_xml_strstr(ptr, (size_t)((xml + xlen) - ptr), "<sheet ");
        if (!ptr) break;
        /* Extract name attribute */
        const char *name_attr = xlsx_xml_strstr(ptr, (size_t)((xml + xlen) - ptr), "name=\"");
        if (name_attr) {
            name_attr += 6;
            const char *endq = (const char *)memchr(name_attr, '"', (size_t)((xml + xlen) - name_attr));
            if (endq) {
                size_t nlen = (size_t)(endq - name_attr);
                parser->sheet_names[i] = (char *)malloc(nlen + 1);
                if (parser->sheet_names[i]) {
                    memcpy(parser->sheet_names[i], name_attr, nlen);
                    parser->sheet_names[i][nlen] = '\0';
                }
            }
        }
        ptr += 7;
    }
    return 0;
}

scl_error_t scl_parse_xlsx_open(scl_parse_xlsx_t *parser, const char *filename) {
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
    if (sz < 0) { scl_parse_xlsx_close(parser); return SCL_ERR_ALLOC; }
    rewind(fp);
    parser->zip_buf = (unsigned char *)malloc((size_t)sz);
    if (!parser->zip_buf) { scl_parse_xlsx_close(parser); return SCL_ERR_OUT_OF_MEMORY; }
    if (fread(parser->zip_buf, 1, (size_t)sz, fp) != (size_t)sz) {
        scl_parse_xlsx_close(parser); return SCL_ERR_ALLOC;
    }
    parser->zip_size = (size_t)sz;

    /* Verify ZIP */
    if (parser->zip_size < 4 || xlsx_read_le32(parser->zip_buf) != ZIP_LOCAL_SIG) {
        scl_parse_xlsx_close(parser); return SCL_ERR_INVALID_ARG;
    }

    /* Parse shared strings */
    unsigned char *ss_data = NULL; size_t ss_len = 0;
    if (xlsx_zip_find_file(parser->zip_buf, parser->zip_size, "xl/sharedStrings.xml",
                           &ss_data, &ss_len) == 0) {
        xlsx_parse_shared_strings(parser, ss_data, ss_len);
    }

    /* Parse workbook for sheet names */
    unsigned char *wb_data = NULL; size_t wb_len = 0;
    if (xlsx_zip_find_file(parser->zip_buf, parser->zip_size, "xl/workbook.xml",
                           &wb_data, &wb_len) == 0) {
        xlsx_parse_workbook(parser, wb_data, wb_len);
    }

    /* Parse sheet1 data */
    unsigned char *s1_data = NULL; size_t s1_len = 0;
    if (xlsx_zip_find_file(parser->zip_buf, parser->zip_size, "xl/worksheets/sheet1.xml",
                           &s1_data, &s1_len) == 0) {
        parser->sheet_data[0] = (char *)malloc(s1_len + 1);
        if (parser->sheet_data[0]) {
            memcpy(parser->sheet_data[0], s1_data, s1_len);
            parser->sheet_data[0][s1_len] = '\0';
        }
    }

    return SCL_OK;
}

scl_error_t scl_parse_xlsx_get_sheets_count(scl_parse_xlsx_t *parser, int *out) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    *out = (int)parser->sheet_count;
    return SCL_OK;
}

scl_error_t scl_parse_xlsx_get_sheet_name(scl_parse_xlsx_t *parser, int index, const char **out, size_t *out_len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    if (index < 0 || (size_t)index >= parser->sheet_count) return SCL_ERR_INVALID_INDEX;
    if (!parser->sheet_names[index]) return SCL_ERR_NOT_FOUND;
    *out = parser->sheet_names[index];
    if (out_len) *out_len = strlen(parser->sheet_names[index]);
    return SCL_OK;
}

scl_error_t scl_parse_xlsx_get_cell(scl_parse_xlsx_t *parser, int sheet_idx, const char *cell_ref,
                                     const char **out, size_t *out_len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!cell_ref, 0)) return SCL_ERR_NULL_PTR;
    if (sheet_idx < 0 || (size_t)sheet_idx >= parser->sheet_count) return SCL_ERR_INVALID_INDEX;

    const char *sheet_xml = parser->sheet_data[sheet_idx];
    if (!sheet_xml) return SCL_ERR_NOT_FOUND;

    /* Find cell reference */
    char search_c[128];
    snprintf(search_c, sizeof(search_c), "r=\"%s\"", cell_ref);
    const char *cell = xlsx_xml_strstr(sheet_xml, strlen(sheet_xml), search_c);
    if (!cell) return SCL_ERR_NOT_FOUND;

    /* Find <v> tag after cell */
    const char *v_open = xlsx_xml_strstr(cell, strlen(sheet_xml) - (size_t)(cell - sheet_xml), "<v>");
    if (!v_open) {
        v_open = xlsx_xml_strstr(cell, strlen(sheet_xml) - (size_t)(cell - sheet_xml), "<v ");
        if (!v_open) return SCL_ERR_NOT_FOUND;
        const char *gt = strchr(v_open, '>');
        if (!gt) return SCL_ERR_NOT_FOUND;
        v_open = gt + 1;
    } else {
        v_open += 3;
    }

    const char *v_close = xlsx_xml_strstr(v_open, strlen(sheet_xml) - (size_t)(v_open - sheet_xml), "</v>");
    if (!v_close) return SCL_ERR_NOT_FOUND;

    size_t vlen = (size_t)(v_close - v_open);
    char *val = (char *)malloc(vlen + 1);
    if (!val) return SCL_ERR_OUT_OF_MEMORY;
    memcpy(val, v_open, vlen);
    val[vlen] = '\0';

    /* Check if it's a shared string */
    char *end = NULL;
    long sid = strtol(val, &end, 10);
    if (*end == '\0' && (size_t)sid < parser->shared_count && parser->shared_strings[sid]) {
        free(val);
        *out = parser->shared_strings[sid];
        if (out_len) *out_len = strlen(parser->shared_strings[sid]);
        return SCL_OK;
    }

    /* Return raw value - caller must free */
    /* For simplicity, keep it as a static-like return. The test file can handle allocation. */
    static char *last_val = NULL;
    free(last_val);
    last_val = val;
    *out = last_val;
    if (out_len) *out_len = vlen;
    return SCL_OK;
}

scl_error_t scl_parse_xlsx_close(scl_parse_xlsx_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (parser->fp) fclose(parser->fp);
    parser->fp = NULL;
    free(parser->filename); parser->filename = NULL;
    free(parser->zip_buf); parser->zip_buf = NULL;
    if (parser->shared_strings) {
        for (size_t i = 0; i < parser->shared_count; i++)
            free(parser->shared_strings[i]);
        free(parser->shared_strings);
        parser->shared_strings = NULL;
    }
    if (parser->sheet_names) {
        for (size_t i = 0; i < parser->sheet_count; i++)
            free(parser->sheet_names[i]);
        free(parser->sheet_names);
        parser->sheet_names = NULL;
    }
    if (parser->sheet_data) {
        for (size_t i = 0; i < parser->sheet_count; i++)
            free(parser->sheet_data[i]);
        free(parser->sheet_data);
        parser->sheet_data = NULL;
    }
    parser->shared_count = parser->sheet_count = 0;
    return SCL_OK;
}
