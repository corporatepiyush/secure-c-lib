#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_parse_pdf.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#define PDF_BUF_SIZE (1024 * 1024)

static char *pdf_strnstr(const char *haystack, const char *needle, size_t len) {
    size_t nlen = scl_strlen(needle);
    if (nlen == 0) return (char *)haystack;
    for (size_t i = 0; i + nlen <= len; i++) {
        if (scl_memcmp(haystack + i, needle, nlen) == 0)
            return (char *)(haystack + i);
    }
    return NULL;
}

static int pdf_read_file(scl_parse_pdf_t *parser) {
    fseek(parser->fp, 0, SEEK_END);
    long sz = ftell(parser->fp);
    if (sz < 0) return -1;
    parser->buf_size = (size_t)sz;
    rewind(parser->fp);
    parser->buf = (unsigned char *)scl_alloc(parser->alloc, parser->buf_size + 1, _Alignof(max_align_t));
    if (!parser->buf) return -1;
    size_t r = fread(parser->buf, 1, parser->buf_size, parser->fp);
    parser->buf[r] = '\0';
    return (r == parser->buf_size) ? 0 : -1;
}

static void pdf_parse_header(scl_parse_pdf_t *parser) {
    if (parser->buf_size < 8) return;
    if (scl_memcmp(parser->buf, "%PDF-", 5) == 0) {
        parser->version_major = parser->buf[5] - '0';
        parser->version_minor = parser->buf[7] - '0';
    }
}

static char *pdf_get_obj_str(scl_parse_pdf_t *parser, int obj_num, size_t *out_len) {
    char obj_marker[64];
    snprintf(obj_marker, sizeof(obj_marker), "\n%d 0 obj", obj_num);
    char *start = pdf_strnstr((char *)parser->buf, obj_marker + 1, parser->buf_size);
    if (!start) {
        snprintf(obj_marker, sizeof(obj_marker), "\r%d 0 obj", obj_num);
        start = pdf_strnstr((char *)parser->buf, obj_marker + 1, parser->buf_size);
    }
    if (!start) return NULL;

    char *end = pdf_strnstr(start, "endobj", parser->buf_size - (start - (char *)parser->buf));
    if (!end) return NULL;
    *out_len = (size_t)(end - start) + 6;
    return start;
}

static int pdf_parse_xref(scl_parse_pdf_t *parser) {
    size_t search = parser->buf_size > 1024 ? parser->buf_size - 1024 : 0;
    char *startxref = pdf_strnstr((char *)parser->buf + search, "startxref", parser->buf_size - search);
    if (!startxref) return -1;

    char *xoff_str = startxref + 10;
    while (*xoff_str && (unsigned char)*xoff_str <= ' ') xoff_str++;
    long long xoff = scl_atoll(xoff_str);
    if (xoff < 0 || (size_t)xoff >= parser->buf_size) return -1;
    parser->xref_offset = (size_t)xoff;

    /* The buffer is NUL-terminated (see pdf_read_file), so the scanning loops
     * below are bounded by the terminator once xref_ptr is in-bounds. */
    char *xref_ptr = (char *)parser->buf + parser->xref_offset;
    if (scl_strncmp(xref_ptr, "xref", 4) != 0) return -1;

    xref_ptr += 4;
    while (*xref_ptr && (unsigned char)*xref_ptr <= ' ') xref_ptr++;

    while (1) {
        if (scl_strncmp(xref_ptr, "trailer", 7) == 0) break;
        int first_obj = 0, count = 0;
        if (sscanf(xref_ptr, "%d %d", &first_obj, &count) != 2) break;
        while (*xref_ptr && !scl_isdigit((unsigned char)*xref_ptr)) xref_ptr++;
        while (*xref_ptr && scl_isdigit((unsigned char)*xref_ptr)) xref_ptr++;
        while (*xref_ptr && (unsigned char)*xref_ptr <= ' ') xref_ptr++;
        while (*xref_ptr && scl_isdigit((unsigned char)*xref_ptr)) xref_ptr++;
        while (*xref_ptr && (unsigned char)*xref_ptr <= ' ') xref_ptr++;

        for (int i = 0; i < count && i < SCL_PDF_MAX_OBJECTS; i++) {
            if (parser->xref_count < SCL_PDF_MAX_OBJECTS) {
                size_t off = 0; int gen = 0; char in_use = 0;
                sscanf(xref_ptr, "%zu %d %c", &off, &gen, &in_use);
                parser->xref_table[parser->xref_count].obj_num = first_obj + i;
                parser->xref_table[parser->xref_count].gen_num = gen;
                parser->xref_table[parser->xref_count].offset = off;
                parser->xref_table[parser->xref_count].in_use = (in_use == 'n' ? 0 : 1);
                parser->xref_count++;
            }
            while (*xref_ptr && *xref_ptr != '\n') xref_ptr++;
            if (*xref_ptr == '\n') xref_ptr++;
        }
    }

    char *trailer = xref_ptr;
    if (scl_strncmp(trailer, "trailer", 7) == 0) {
        char *root_str = pdf_strnstr(trailer, "/Root", parser->buf_size - (trailer - (char *)parser->buf));
        if (root_str) {
            char *r = root_str + 5;
            while (*r && (unsigned char)*r <= ' ') r++;
            parser->root_obj = scl_atoi(r);
            char *g = r;
            while (*g && scl_isdigit((unsigned char)*g)) g++;
            while (*g && (unsigned char)*g <= ' ') g++;
            parser->root_gen = scl_atoi(g);
        }
        char *info_str = pdf_strnstr(trailer, "/Info", parser->buf_size - (trailer - (char *)parser->buf));
        if (info_str) {
            char *r = info_str + 5;
            while (*r && (unsigned char)*r <= ' ') r++;
            parser->info_obj = scl_atoi(r);
            char *g = r;
            while (*g && scl_isdigit((unsigned char)*g)) g++;
            while (*g && (unsigned char)*g <= ' ') g++;
            parser->info_gen = scl_atoi(g);
        }
    }

    return 0;
}

static int pdf_count_pages(scl_parse_pdf_t *parser) {
    if (!parser->root_obj) return 0;

    size_t len = 0;
    char *root_str = pdf_get_obj_str(parser, parser->root_obj, &len);
    if (!root_str) return 0;

    char *pages_ref = pdf_strnstr(root_str, "/Pages", len);
    if (!pages_ref) return 0;
    char *p = pages_ref + 6;
    while (*p && (unsigned char)*p <= ' ') p++;
    int pages_obj = scl_atoi(p + 1);

    len = 0;
    char *pages_str = pdf_get_obj_str(parser, pages_obj, &len);
    if (!pages_str) return 0;

    int count = 0;
    char *ptr = pages_str;
    while ((ptr = pdf_strnstr(ptr, "/Type", len - (size_t)(ptr - pages_str))) != NULL) {
        ptr += 5;
        while (*ptr && (unsigned char)*ptr <= ' ') ptr++;
        if (scl_strncmp(ptr, "/Page", 5) == 0)
            count++;
    }
    return count;
}

scl_error_t scl_parse_pdf_open(scl_allocator_t *alloc, scl_parse_pdf_t *parser, const char *filename) {
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

    if (pdf_read_file(parser) != 0) {
        fclose(parser->fp);
        scl_free(alloc, parser->filename);
        parser->filename = NULL;
        return SCL_ERR_ALLOC;
    }

    pdf_parse_header(parser);
    pdf_parse_xref(parser);
    parser->page_count = pdf_count_pages(parser);

    return SCL_OK;
}

scl_error_t scl_parse_pdf_get_page_count(scl_parse_pdf_t *parser, int *out) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;
    *out = parser->page_count;
    return SCL_OK;
}

scl_error_t scl_parse_pdf_get_info(scl_parse_pdf_t *parser, const char *key, char *out, size_t *out_len) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!key)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(!out)) return SCL_ERR_NULL_PTR;

    if (!parser->info_obj) return SCL_ERR_NOT_FOUND;

    size_t len = 0;
    char *info_str = pdf_get_obj_str(parser, parser->info_obj, &len);
    if (!info_str) return SCL_ERR_NOT_FOUND;

    char search[128];
    snprintf(search, sizeof(search), "/%s", key);
    char *found = pdf_strnstr(info_str, search, len);
    if (!found) return SCL_ERR_NOT_FOUND;

    char *val = found + scl_strlen(search);
    while (*val && (unsigned char)*val <= ' ') val++;

    if (*val == '(') {
        val++;
        size_t vlen = 0;
        while (val[vlen] && val[vlen] != ')') vlen++;
        size_t to_copy = vlen < *out_len ? vlen : *out_len - 1;
        scl_memcpy(out, val, to_copy);
        out[to_copy] = '\0';
        *out_len = to_copy + 1;
        return SCL_OK;
    }

    return SCL_ERR_NOT_FOUND;
}

scl_error_t scl_parse_pdf_close(scl_parse_pdf_t *parser) {
    if (scl_unlikely(!parser)) return SCL_ERR_NULL_PTR;
    if (parser->fp) fclose(parser->fp);
    parser->fp = NULL;
    scl_free(parser->alloc, parser->filename);
    parser->filename = NULL;
    scl_free(parser->alloc, parser->buf);
    parser->buf = NULL;
    parser->buf_size = 0;
    return SCL_OK;
}
