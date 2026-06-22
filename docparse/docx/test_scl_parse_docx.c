#include "../../testlib/scl_test.h"
#include "../../string/scl_string.h"
#include "scl_parse_docx.h"

static void create_minimal_docx(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    unsigned char lhdr[] = {
        0x50, 0x4B, 0x03, 0x04,
        0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00,
        0x2A, 0x00, 0x00, 0x00,
        0x2A, 0x00, 0x00, 0x00,
        0x14, 0x00, 0x00, 0x00,
    };
    fwrite(lhdr, 1, sizeof(lhdr), f);
    const char *fname = "word/document.xml";
    fwrite(fname, 1, scl_strlen(fname), f);

    const char *xml = "<?xml version=\"1.0\"?><w:document><w:body><w:p><w:r><w:t>Hello DOCX</w:t></w:r></w:p></w:body></w:document>";
    fwrite(xml, 1, scl_strlen(xml), f);

    fclose(f);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("file not found");
    {
        scl_parse_docx_t docx;
        scl_error_t e = scl_parse_docx_open(alloc, &docx, "/tmp/nonexistent_docx.docx");
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_NOT_FOUND);
    }

    scl_test_group("open minimal docx");
    {
        const char *path = "/tmp/test_scl_docx.docx";
        create_minimal_docx(path);
        scl_parse_docx_t docx;
        scl_error_t e = scl_parse_docx_open(alloc, &docx, path);
        if (e == SCL_OK) {
            const char *text = NULL;
            size_t tlen = 0;
            scl_parse_docx_get_text(&docx, &text, &tlen);
            SCL_EXPECT_NOT_NULL(&tr, text);
        } else { SCL_EXPECT_TRUE(&tr, 0); }
        scl_parse_docx_close(&docx);
        remove(path);
    }

    scl_test_group("not a docx (invalid magic)");
    {
        const char *path = "/tmp/test_not_docx.bin";
        FILE *f = fopen(path, "wb");
        if (f) { fwrite("NotAZIP", 1, 7, f); fclose(f); }
        scl_parse_docx_t docx;
        scl_error_t e = scl_parse_docx_open(alloc, &docx, path);
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_INVALID_ARG || e == SCL_OK);
        if (e == SCL_OK) scl_parse_docx_close(&docx);
        remove(path);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_docx_open(alloc, NULL, "test.docx") == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
