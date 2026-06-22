#include "scl_parse_pdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void create_test_pdf(const char *path) {
    /* Minimal PDF with 1 page */
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "%%PDF-1.4\n");
    fprintf(f, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    fprintf(f, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
    fprintf(f, "3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] >>\nendobj\n");
    fprintf(f, "4 0 obj\n<< /Title (Test Document) /Author (Test) >>\nendobj\n");
    fprintf(f, "xref\n0 5\n");
    fprintf(f, "0000000000 65535 f \n");
    fprintf(f, "0000000009 00000 n \n");
    fprintf(f, "0000000058 00000 n \n");
    fprintf(f, "0000000115 00000 n \n");
    fprintf(f, "0000000180 00000 n \n");
    fprintf(f, "trailer\n<< /Size 5 /Root 1 0 R /Info 4 0 R >>\n");
    fprintf(f, "startxref\n%ld\n", ftell(f));
    fprintf(f, "%%%%EOF\n");
    fclose(f);
}

int main(void) {
    printf("=== scl_parse_pdf tests ===\n");

    TEST("init and close (file not found)");
    {
        scl_parse_pdf_t pdf;
        scl_error_t e = scl_parse_pdf_open(&pdf, "/tmp/nonexistent_xxxx.pdf");
        if (e == SCL_ERR_NOT_FOUND) { PASS(); }
        else { FAIL("expected NOT_FOUND"); }
    }

    TEST("parse valid PDF");
    {
        const char *path = "/tmp/test_scl_pdf.pdf";
        create_test_pdf(path);
        scl_parse_pdf_t pdf;
        scl_error_t e = scl_parse_pdf_open(&pdf, path);
        if (e == SCL_OK) {
            int pages = 0;
            scl_parse_pdf_get_page_count(&pdf, &pages);
            if (pages >= 0) { PASS(); } else { FAIL("page count failed"); }
        } else { FAIL("open failed"); }
        scl_parse_pdf_close(&pdf);
        remove(path);
    }

    TEST("get metadata");
    {
        const char *path = "/tmp/test_scl_pdf2.pdf";
        create_test_pdf(path);
        scl_parse_pdf_t pdf;
        scl_parse_pdf_open(&pdf, path);
        char buf[256];
        size_t blen = sizeof(buf);
        scl_error_t e = scl_parse_pdf_get_info(&pdf, "Title", buf, &blen);
        if (e == SCL_OK) { PASS(); } else { FAIL("get info failed"); }
        scl_parse_pdf_close(&pdf);
        remove(path);
    }

    TEST("NULL checks");
    {
        if (scl_parse_pdf_open(NULL, "test.pdf") == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
