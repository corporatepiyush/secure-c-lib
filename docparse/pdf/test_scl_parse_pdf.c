#include "../../testlib/scl_test.h"
#include "scl_parse_pdf.h"

static void create_test_pdf(const char *path) {
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
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("init and close (file not found)");
    {
        scl_parse_pdf_t pdf;
        scl_error_t e = scl_parse_pdf_open(alloc, &pdf, "/tmp/nonexistent_xxxx.pdf");
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_NOT_FOUND);
    }

    scl_test_group("parse valid PDF");
    {
        const char *path = "/tmp/test_scl_pdf.pdf";
        create_test_pdf(path);
        scl_parse_pdf_t pdf;
        scl_error_t e = scl_parse_pdf_open(alloc, &pdf, path);
        if (e == SCL_OK) {
            int pages = 0;
            scl_parse_pdf_get_page_count(&pdf, &pages);
            SCL_EXPECT_TRUE(&tr, pages >= 0);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_pdf_close(&pdf);
        remove(path);
    }

    scl_test_group("get metadata");
    {
        const char *path = "/tmp/test_scl_pdf2.pdf";
        create_test_pdf(path);
        scl_parse_pdf_t pdf;
        scl_parse_pdf_open(alloc, &pdf, path);
        char buf[256];
        size_t blen = sizeof(buf);
        scl_error_t e = scl_parse_pdf_get_info(&pdf, "Title", buf, &blen);
        SCL_EXPECT_TRUE(&tr, e == SCL_OK);
        scl_parse_pdf_close(&pdf);
        remove(path);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_pdf_open(alloc, NULL, "test.pdf") == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
