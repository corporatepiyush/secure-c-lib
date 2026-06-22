#include "scl_parse_xlsx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %s ... ", name); } while(0)
#define PASS() do { printf("PASSED\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); tests_failed++; } while(0)

static void create_minimal_xlsx(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return;

    /* Minimal ZIP with xl/workbook.xml, etc */
    unsigned char lhdr[] = {
        0x50, 0x4B, 0x03, 0x04, 0x0A, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    fwrite(lhdr, 1, 30, f);
    const char *fname = "xl/workbook.xml";
    uint16_t flen = (uint16_t)strlen(fname);
    fwrite(&flen, 2, 1, f);
    uint16_t xlen = 0;
    fwrite(&xlen, 2, 1, f);
    fwrite(fname, 1, flen, f);

    const char *wb = "<?xml version=\"1.0\"?><workbook><sheets><sheet name=\"Sheet1\"/></sheets></workbook>";
    fwrite(wb, 1, strlen(wb), f);

    /* Also need xl/sharedStrings.xml and xl/worksheets/sheet1.xml for proper tests */
    /* For simplicity, just test file open */
    fclose(f);
}

int main(void) {
    printf("=== scl_parse_xlsx tests ===\n");

    TEST("file not found");
    {
        scl_parse_xlsx_t xlsx;
        scl_error_t e = scl_parse_xlsx_open(&xlsx, "/tmp/nonexistent.xlsx");
        if (e == SCL_ERR_NOT_FOUND) { PASS(); }
        else { FAIL("expected NOT_FOUND"); }
    }

    TEST("open minimal xlsx");
    {
        const char *path = "/tmp/test_scl_xlsx.xlsx";
        create_minimal_xlsx(path);
        scl_parse_xlsx_t xlsx;
        scl_error_t e = scl_parse_xlsx_open(&xlsx, path);
        if (e == SCL_OK) { PASS(); } else { FAIL("open failed"); }
        scl_parse_xlsx_close(&xlsx);
        remove(path);
    }

    TEST("NULL checks");
    {
        if (scl_parse_xlsx_open(NULL, "test.xlsx") == SCL_ERR_NULL_PTR) { PASS(); }
        else { FAIL("expected NULL_PTR"); }
    }

    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
