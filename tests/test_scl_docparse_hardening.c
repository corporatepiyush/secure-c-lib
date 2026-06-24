/* Hardening tests for the document parsers.
 *
 * These feed deliberately malformed / hostile input to each parser and assert
 * that the library returns cleanly (error or best-effort result) without
 * crashing, reading out of bounds, or leaking. Run under AddressSanitizer to
 * exercise the bounds checks added to the parsers:
 *
 *   gcc -fsanitize=address,undefined -I libs/... tests/test_scl_docparse_hardening.c \
 *       libs/docparse/<...>.c libs/common/scl_common.c libs/string/scl_string.c ...
 */
#include "scl_test.h"
#include "scl_parse_json.h"
#include "scl_parse_csv.h"
#include "scl_parse_tsv.h"
#include "scl_parse_pdf.h"
#include "scl_parse_xlsx.h"
#include "scl_parse_docx.h"
#include "scl_parse_parquet.h"

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/* Write `len` bytes to a unique temp file; returns path in static-per-call buf. */
static int write_temp(const char *tag, const unsigned char *data, size_t len, char *path_out, size_t path_cap) {
    snprintf(path_out, path_cap, "/tmp/scl_hard_%s_%d.bin", tag, (int)getpid());
    FILE *f = fopen(path_out, "wb");
    if (!f) return -1;
    if (len) fwrite(data, 1, len, f);
    fclose(f);
    return 0;
}

static void put_le32(unsigned char *p, uint32_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void put_le16(unsigned char *p, uint16_t v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
}

/* ── JSON ───────────────────────────────────────────────────── */
static void test_json_malformed(scl_test_runner_t *tr) {
    scl_test_group("JSON: malformed input does not crash");
    scl_allocator_t *a = scl_allocator_default();

    /* Inputs that previously could read past the NUL terminator. */
    const char *cases[] = {
        "\"\\u",               /* truncated \u escape at EOF */
        "\"\\u12",             /* partial \u escape */
        "\"abc\\",             /* lone trailing backslash */
        "[\"\\uABCD\",\"\\u00",
        "{\"k\":\"\\u",
        "",                    /* empty */
        "[[[[[[[[[[",          /* unterminated nesting */
        "\"unterminated string",
        "123e",                /* malformed number */
    };
    for (size_t i = 0; i < SCL_ARRAY_SIZE(cases); i++) {
        scl_parse_json_value_t *root = NULL;
        scl_error_t err = scl_parse_json_parse(a, cases[i], &root);
        (void)err;
        scl_parse_json_free(a, root);   /* safe on NULL */
        SCL_EXPECT_TRUE(tr, 1);          /* reaching here = no crash */
    }
}

/* ── CSV / TSV ──────────────────────────────────────────────── */
static void test_csv_tsv(scl_test_runner_t *tr) {
    scl_test_group("CSV/TSV: feed and drain without overrun");
    scl_allocator_t *a = scl_allocator_default();

    scl_parse_csv_t csv;
    SCL_EXPECT_OK(tr, scl_parse_csv_init(a, &csv));
    SCL_EXPECT_OK(tr, scl_parse_csv_feed(&csv, "a,\"b\"\"c\",d\r\n1,2,3\n", 18));
    const char *fld; size_t flen;
    int guard = 0;
    while (scl_parse_csv_next_field(&csv, &fld, &flen) == SCL_OK && guard++ < 100) {
        if (scl_parse_csv_next_row(&csv) != SCL_OK) break;
    }
    scl_parse_csv_destroy(&csv);

    scl_parse_tsv_t tsv;
    SCL_EXPECT_OK(tr, scl_parse_tsv_init(a, &tsv));
    SCL_EXPECT_OK(tr, scl_parse_tsv_feed(&tsv, "a\tb\tc\n1\t2\t3\n", 12));
    guard = 0;
    while (scl_parse_tsv_next_field(&tsv, &fld, &flen) == SCL_OK && guard++ < 100) {
        if (scl_parse_tsv_next_row(&tsv) != SCL_OK) break;
    }
    scl_parse_tsv_destroy(&tsv);
    SCL_EXPECT_TRUE(tr, 1);
}

/* ── PDF: startxref pointing out of bounds ──────────────────── */
static void test_pdf_bad_xref(scl_test_runner_t *tr) {
    scl_test_group("PDF: out-of-range startxref offset");
    scl_allocator_t *a = scl_allocator_default();

    /* Valid-looking header + startxref offset far beyond the file size. */
    const char *doc = "%PDF-1.7\nsome body\nstartxref\n9999999\n%%EOF\n";
    char path[256];
    write_temp("pdf", (const unsigned char *)doc, strlen(doc), path, sizeof(path));

    scl_parse_pdf_t p;
    scl_error_t err = scl_parse_pdf_open(a, &p, path);
    if (err == SCL_OK) {
        int pages = -1;
        scl_parse_pdf_get_page_count(&p, &pages);   /* must not deref OOB */
        scl_parse_pdf_close(&p);
    }
    remove(path);
    SCL_EXPECT_TRUE(tr, 1);
}

/* Build a one-entry STORED zip with a forged 4GB size header. */
static size_t build_forged_zip(unsigned char *buf, const char *name, const char *payload) {
    uint16_t nlen = (uint16_t)strlen(name);
    size_t plen = strlen(payload);
    put_le32(buf + 0, 0x04034b50);          /* local file sig */
    put_le16(buf + 4, 20);                   /* version */
    put_le16(buf + 6, 0);                    /* flags */
    put_le16(buf + 8, 0);                    /* method: stored */
    put_le16(buf + 10, 0); put_le16(buf + 12, 0);
    put_le32(buf + 14, 0);                   /* crc */
    put_le32(buf + 18, 0xFFFFFFFFu);         /* comp size: forged huge */
    put_le32(buf + 22, 0xFFFFFFFFu);         /* uncomp size: forged huge */
    put_le16(buf + 26, nlen);
    put_le16(buf + 28, 0);                   /* extra len */
    size_t pos = 30;
    memcpy(buf + pos, name, nlen); pos += nlen;
    memcpy(buf + pos, payload, plen); pos += plen;
    return pos;
}

/* ── XLSX: forged ZIP sizes and missing workbook.xml ────────── */
static void test_xlsx_forged_zip(scl_test_runner_t *tr) {
    scl_test_group("XLSX: forged uncompressed size / no workbook");
    scl_allocator_t *a = scl_allocator_default();
    unsigned char buf[256];
    char path[256];

    /* (1) sharedStrings.xml with a forged 4GB size: the shared-strings scan
     *     reads `out_len` bytes directly, so the clamp-to-available fix is what
     *     keeps this in bounds. */
    size_t pos = build_forged_zip(buf, "xl/sharedStrings.xml", "<si><t>x</t></si>");
    write_temp("xlsx_ss", buf, pos, path, sizeof(path));
    scl_parse_xlsx_t x1;
    if (scl_parse_xlsx_open(a, &x1, path) == SCL_OK) scl_parse_xlsx_close(&x1);
    remove(path);
    SCL_EXPECT_TRUE(tr, 1);

    /* (2) sheet1.xml present but workbook.xml absent: exercises the
     *     sheet_data NULL-deref guard. */
    pos = build_forged_zip(buf, "xl/worksheets/sheet1.xml", "<x/>");
    write_temp("xlsx_s1", buf, pos, path, sizeof(path));
    scl_parse_xlsx_t x2;
    if (scl_parse_xlsx_open(a, &x2, path) == SCL_OK) {
        int sheets = -1;
        scl_parse_xlsx_get_sheets_count(&x2, &sheets);
        scl_parse_xlsx_close(&x2);
    }
    remove(path);
    SCL_EXPECT_TRUE(tr, 1);
}

/* ── DOCX: forged ZIP size for document.xml ─────────────────── */
static void test_docx_forged_zip(scl_test_runner_t *tr) {
    scl_test_group("DOCX: forged size header");
    scl_allocator_t *a = scl_allocator_default();

    const char *name = "word/document.xml";
    uint16_t nlen = (uint16_t)strlen(name);
    const char *payload = "<w:t>hi</w:t>";
    size_t plen = strlen(payload);

    unsigned char buf[256];
    put_le32(buf + 0, 0x04034b50);
    put_le16(buf + 4, 20); put_le16(buf + 6, 0); put_le16(buf + 8, 0);
    put_le16(buf + 10, 0); put_le16(buf + 12, 0);
    put_le32(buf + 14, 0);
    put_le32(buf + 18, 0xFFFFFFFFu);   /* comp size forged */
    put_le32(buf + 22, 0xFFFFFFFFu);   /* uncomp size forged */
    put_le16(buf + 26, nlen);
    put_le16(buf + 28, 0);
    size_t pos = 30;
    memcpy(buf + pos, name, nlen); pos += nlen;
    memcpy(buf + pos, payload, plen); pos += plen;

    char path[256];
    write_temp("docx", buf, pos, path, sizeof(path));

    scl_parse_docx_t d;
    scl_error_t err = scl_parse_docx_open(a, &d, path);
    if (err == SCL_OK) {
        const char *txt = NULL; size_t tlen = 0;
        scl_parse_docx_get_text(&d, &txt, &tlen);   /* reads clamped length */
        scl_parse_docx_close(&d);
    }
    remove(path);
    SCL_EXPECT_TRUE(tr, 1);
}

/* ── Parquet: runaway varints and huge lengths ──────────────── */
static void test_parquet_runaway_varint(scl_test_runner_t *tr) {
    scl_test_group("Parquet: runaway varint footer");
    scl_allocator_t *a = scl_allocator_default();

    /* Build: "PAR1" <footer> <footer_len LE32> "PAR1".
     * Footer is a thrift-ish struct field (type=2 STRING/LIST, id=2 -> column
     * list) followed by a length varint with the continuation bit set on every
     * byte so an unbounded reader would walk off the end. */
    unsigned char footer[32];
    size_t fl = 0;
    footer[fl++] = (unsigned char)((2 << 2) | 2);   /* field: id=2, type=2 */
    for (int i = 0; i < 8; i++) footer[fl++] = 0x80; /* never-terminating varint */

    unsigned char buf[64];
    size_t pos = 0;
    memcpy(buf + pos, "PAR1", 4); pos += 4;
    memcpy(buf + pos, footer, fl); pos += fl;
    put_le32(buf + pos, (uint32_t)fl); pos += 4;
    memcpy(buf + pos, "PAR1", 4); pos += 4;

    char path[256];
    write_temp("parquet", buf, pos, path, sizeof(path));

    scl_parse_parquet_t pq;
    scl_error_t err = scl_parse_parquet_open(a, &pq, path);
    if (err == SCL_OK) {
        int cols = -1; int64_t rows = -1;
        scl_parse_parquet_get_column_count(&pq, &cols);
        scl_parse_parquet_get_row_count(&pq, &rows);
        scl_parse_parquet_close(&pq);
    }
    remove(path);
    SCL_EXPECT_TRUE(tr, 1);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_json_malformed(&tr);
    test_csv_tsv(&tr);
    test_pdf_bad_xref(&tr);
    test_xlsx_forged_zip(&tr);
    test_docx_forged_zip(&tr);
    test_parquet_runaway_varint(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
