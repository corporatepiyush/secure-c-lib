#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_docx.h"
#include "scl_stdlib.h"
#include "scl_string.h"

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

        if ((size_t)name_len == scl_strlen(name) &&
            scl_memcmp(buf + pos + ZIP_LOCAL_HDR_SZ, name, name_len) == 0) {
            if (comp_method != 0) {
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
        char *nb = (char *)scl_realloc(parser->alloc, parser->text_buf, parser->text_cap, nc, _Alignof(max_align_t));
        if (!nb) return -1;
        parser->text_buf = nb;
        parser->text_cap = nc;
    }
    scl_memcpy(parser->text_buf + parser->text_len, text, len);
    parser->text_len += len;
    parser->text_buf[parser->text_len] = '\0';
    return 0;
}

static void docx_extract_text_from_xml(scl_parse_docx_t *parser, const unsigned char *xml, size_t xml_len) {
    const unsigned char *end = xml + xml_len;
    const unsigned char *p = xml;
    int in_tag = 0;

    while (p < end) {
        if (*p == '<') { in_tag = 1; p++; continue; }
        if (*p == '>') { in_tag = 0; p++; continue; }
        if (!in_tag) {
            const unsigned char *start = p;
            while (p < end && *p != '<') p++;
            docx_append_text(parser, (const char *)start, (size_t)(p - start));
            if (p < end && *p == '<') {
                if (p + 4 <= end && scl_memcmp(p, "</w:", 4) == 0) {
                    const unsigned char *eot = p;
                    while (eot < end && *eot != '>') eot++;
                    (void)eot;
                }
            }
        } else {
            p++;
        }
    }
}

scl_error_t scl_parse_docx_open(scl_allocator_t *alloc, scl_parse_docx_t *parser, const char *filename) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!filename)) return SCL_ERR_NULL_PTR;

    (void)scl_memset(parser, 0, sizeof(*parser));
    parser->alloc = alloc;

    parser->filename = scl_strdup(alloc, filename);
    if (!parser->filename) return SCL_ERR_OUT_OF_MEMORY;

    parser->fp = fopen(filename, "rb");
    if (scl_unlikely(!parser->fp)) {
        scl_free(alloc, parser->filename);
        parser->filename = NULL;
        return SCL_ERR_NOT_FOUND;
    }

    fseek(parser->fp, 0, SEEK_END);
    long sz = ftell(parser->fp);
    if (sz < 0) { fclose(parser->fp); scl_free(alloc, parser->filename); return SCL_ERR_ALLOC; }
    parser->zip_size = (size_t)sz;
    rewind(parser->fp);

    parser->zip_buf = (unsigned char *)scl_alloc(alloc, parser->zip_size, _Alignof(max_align_t));
    if (!parser->zip_buf) { fclose(parser->fp); scl_free(alloc, parser->filename); return SCL_ERR_OUT_OF_MEMORY; }

    if (fread(parser->zip_buf, 1, parser->zip_size, parser->fp) != parser->zip_size) {
        fclose(parser->fp); scl_free(alloc, parser->filename); scl_free(alloc, parser->zip_buf);
        return SCL_ERR_ALLOC;
    }

    if (parser->zip_size < 4 || read_le32(parser->zip_buf) != ZIP_LOCAL_SIG) {
        fclose(parser->fp); scl_free(alloc, parser->filename); scl_free(alloc, parser->zip_buf);
        return SCL_ERR_INVALID_ARG;
    }

    unsigned char *xml_data = NULL;
    size_t xml_len = 0;
    if (docx_zip_find_file(parser, "word/document.xml", &xml_data, &xml_len) != 0) {
        if (docx_zip_find_file(parser, "/word/document.xml", &xml_data, &xml_len) != 0) {
        }
    }

    if (xml_data) {
        parser->text_buf = (char *)scl_alloc(alloc, 4096, _Alignof(max_align_t));
        parser->text_cap = 4096;
        parser->text_len = 0;
        docx_extract_text_from_xml(parser, xml_data, xml_len);
    }

    return SCL_OK;
}

scl_error_t scl_parse_docx_get_text(scl_parse_docx_t *parser, const char **out, size_t *out_len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;

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
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (parser->fp) fclose(parser->fp);
    parser->fp = NULL;
    scl_free(parser->alloc, parser->filename);
    parser->filename = NULL;
    scl_free(parser->alloc, parser->zip_buf);
    parser->zip_buf = NULL;
    scl_free(parser->alloc, parser->text_buf);
    parser->text_buf = NULL;
    parser->zip_size = parser->text_len = parser->text_cap = 0;
    return SCL_OK;
}
