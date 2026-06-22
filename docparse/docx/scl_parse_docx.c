#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_docx.h"
#include <stdlib.h>
#include <string.h>

/* Minimal ZIP local file header parsing */
#define ZIP_LOCAL_HDR_SZ 30
#define ZIP_CENTRAL_HDR_SZ 46
#define ZIP_EOCD_SZ 22
#define ZIP_LOCAL_SIG 0x04034b50
#define ZIP_CENTRAL_SIG 0x02014b50
#define ZIP_EOCD_SIG 0x06054b50

static uint32_t read_le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static int docx_zip_find_file(scl_parse_docx_t *parser, const char *name, unsigned char **out, size_t *out_len) {
    unsigned char *buf = parser->zip_buf;
    size_t sz = parser->zip_size;
    size_t pos = 0;

    while (pos + ZIP_LOCAL_HDR_SZ <= sz) {
        uint32_t sig = read_le32(buf + pos);
        if (sig != ZIP_LOCAL_SIG) break;

        uint16_t comp_method = read_le16(buf + pos + 8);
        uint32_t comp_sz = read_le32(buf + pos + 18);
        uint32_t uncomp_sz = read_le32(buf + pos + 22);
        uint16_t name_len = read_le16(buf + pos + 26);
        uint16_t extra_len = read_le16(buf + pos + 28);

        size_t hdr_end = pos + ZIP_LOCAL_HDR_SZ + name_len + extra_len;
        if (hdr_end > sz) break;

        if ((size_t)name_len == strlen(name) &&
            memcmp(buf + pos + ZIP_LOCAL_HDR_SZ, name, name_len) == 0) {
            if (comp_method != 0) {
                // Stored only - handle uncompressed
                *out = buf + hdr_end;
                *out_len = comp_sz;
                return 0;
            }
            *out = buf + hdr_end;
            *out_len = uncomp_sz;
            return 0;
        }

        pos = hdr_end + comp_sz;
    }

    return -1;
}

static int docx_append_text(scl_parse_docx_t *parser, const char *text, size_t len) {
    if (len == 0) return 0;
    while (parser->text_len + len + 1 > parser->text_cap) {
        size_t nc = parser->text_cap ? parser->text_cap * 2 : 4096;
        char *nb = (char *)realloc(parser->text_buf, nc);
        if (!nb) return -1;
        parser->text_buf = nb;
        parser->text_cap = nc;
    }
    memcpy(parser->text_buf + parser->text_len, text, len);
    parser->text_len += len;
    parser->text_buf[parser->text_len] = '\0';
    return 0;
}

static void docx_extract_text_from_xml(scl_parse_docx_t *parser, const unsigned char *xml, size_t xml_len) {
    /* Naive XML text extraction: find text between > and < */
    const unsigned char *end = xml + xml_len;
    const unsigned char *p = xml;
    int in_tag = 0;

    while (p < end) {
        if (*p == '<') { in_tag = 1; p++; continue; }
        if (*p == '>') { in_tag = 0; p++; continue; }
        if (!in_tag) {
            /* Check if this is within a <w:t> tag */
            const unsigned char *before = p;
            while (before > xml && *(before - 1) != '<') before--;
            /* Simple heuristic: extract text between tags */
            const unsigned char *start = p;
            while (p < end && *p != '<') p++;
            docx_append_text(parser, (const char *)start, (size_t)(p - start));
            if (p < end && *p == '<') {
                /* Check if we should add newline */
                if (p + 4 <= end && memcmp(p, "</w:", 4) == 0) {
                    const unsigned char *eot = p;
                    while (eot < end && *eot != '>') eot++;
                    if (eot - p >= 7 && memcmp(eot - 4, "pPr", 3) == 0) {
                        // paragraph end
                    }
                }
            }
        } else {
            p++;
        }
    }
}

scl_error_t scl_parse_docx_open(scl_parse_docx_t *parser, const char *filename) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!filename, 0)) return SCL_ERR_NULL_PTR;

    memset(parser, 0, sizeof(*parser));

    parser->filename = strdup(filename);
    if (!parser->filename) return SCL_ERR_OUT_OF_MEMORY;

    parser->fp = fopen(filename, "rb");
    if (__builtin_expect(!parser->fp, 0)) {
        free(parser->filename);
        parser->filename = NULL;
        return SCL_ERR_NOT_FOUND;
    }

    fseek(parser->fp, 0, SEEK_END);
    long sz = ftell(parser->fp);
    if (sz < 0) { fclose(parser->fp); free(parser->filename); return SCL_ERR_ALLOC; }
    parser->zip_size = (size_t)sz;
    rewind(parser->fp);

    parser->zip_buf = (unsigned char *)malloc(parser->zip_size);
    if (!parser->zip_buf) { fclose(parser->fp); free(parser->filename); return SCL_ERR_OUT_OF_MEMORY; }

    if (fread(parser->zip_buf, 1, parser->zip_size, parser->fp) != parser->zip_size) {
        fclose(parser->fp); free(parser->filename); free(parser->zip_buf);
        return SCL_ERR_ALLOC;
    }

    /* Verify ZIP magic */
    if (parser->zip_size < 4 || read_le32(parser->zip_buf) != ZIP_LOCAL_SIG) {
        fclose(parser->fp); free(parser->filename); free(parser->zip_buf);
        return SCL_ERR_INVALID_ARG;
    }

    /* Find word/document.xml */
    unsigned char *xml_data = NULL;
    size_t xml_len = 0;
    if (docx_zip_find_file(parser, "word/document.xml", &xml_data, &xml_len) != 0) {
        /* Try alternative paths */
        if (docx_zip_find_file(parser, "/word/document.xml", &xml_data, &xml_len) != 0) {
            /* No document found - that's ok, just empty */
        }
    }

    if (xml_data) {
        parser->text_buf = (char *)malloc(4096);
        parser->text_cap = 4096;
        parser->text_len = 0;
        docx_extract_text_from_xml(parser, xml_data, xml_len);
    }

    return SCL_OK;
}

scl_error_t scl_parse_docx_get_text(scl_parse_docx_t *parser, const char **out, size_t *out_len) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out, 0)) return SCL_ERR_NULL_PTR;

    if (!parser->text_buf) {
        *out = "";
        if (out_len) *out_len = 0;
        return SCL_OK;
    }

    *out = parser->text_buf;
    if (out_len) *out_len = parser->text_len;
    return SCL_OK;
}

scl_error_t scl_parse_docx_close(scl_parse_docx_t *parser) {
    if (__builtin_expect(!parser, 0)) return SCL_ERR_NULL_PTR;
    if (parser->fp) fclose(parser->fp);
    parser->fp = NULL;
    free(parser->filename);
    parser->filename = NULL;
    free(parser->zip_buf);
    parser->zip_buf = NULL;
    free(parser->text_buf);
    parser->text_buf = NULL;
    parser->zip_size = parser->text_len = parser->text_cap = 0;
    return SCL_OK;
}
