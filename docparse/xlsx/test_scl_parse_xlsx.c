#include "../../testlib/scl_test.h"
#include "../../string/scl_string.h"
#include "scl_parse_xlsx.h"

static void create_minimal_xlsx(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    unsigned char lhdr[] = {
        0x50, 0x4B, 0x03, 0x04, 0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    fwrite(lhdr, 1, 30, f);
    const char *fname = "xl/workbook.xml";
    uint16_t flen = (uint16_t)scl_strlen(fname);
    fwrite(&flen, 2, 1, f);
    uint16_t xlen = 0;
    fwrite(&xlen, 2, 1, f);
    fwrite(fname, 1, flen, f);

    const char *wb = "<?xml version=\"1.0\"?><workbook><sheets><sheet name=\"Sheet1\"/></sheets></workbook>";
    fwrite(wb, 1, scl_strlen(wb), f);

    fclose(f);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);

    scl_allocator_t *alloc = scl_allocator_default();

    scl_test_group("file not found");
    {
        scl_parse_xlsx_t xlsx;
        scl_error_t e = scl_parse_xlsx_open(alloc, &xlsx, "/tmp/nonexistent.xlsx");
        SCL_EXPECT_TRUE(&tr, e == SCL_ERR_NOT_FOUND);
    }

    scl_test_group("open minimal xlsx");
    {
        const char *path = "/tmp/test_scl_xlsx.xlsx";
        create_minimal_xlsx(path);
        scl_parse_xlsx_t xlsx;
        scl_error_t e = scl_parse_xlsx_open(alloc, &xlsx, path);
        SCL_EXPECT_TRUE(&tr, e == SCL_OK);
        scl_parse_xlsx_close(&xlsx);
        remove(path);
    }

    scl_test_group("NULL checks");
    {
        SCL_EXPECT_TRUE(&tr, scl_parse_xlsx_open(alloc, NULL, "test.xlsx") == SCL_ERR_NULL_PTR);
    }

    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
