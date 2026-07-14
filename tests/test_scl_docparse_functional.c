/* Functional tests for the binary/container document parsers. Unlike the
 * hardening suite (which only checks that hostile input doesn't crash), these
 * build minimal *valid* documents and assert the extracted values are correct.
 * Run under ASan to also catch leaks/over-reads on the success paths. */
#include "scl_parse_docx.h"
#include "scl_parse_icelake.h"
#include "scl_parse_pdf.h"
#include "scl_parse_xlsx.h"
#include "scl_test.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void put_le32(unsigned char *p, uint32_t v) {
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}
static void put_le16(unsigned char *p, uint16_t v) {
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
}

static int write_file(const char *tag, const unsigned char *data, size_t len,
                      char *path, size_t cap) {
  snprintf(path, cap, "/tmp/scl_func_%s_%d.bin", tag, (int)getpid());
  FILE *f = fopen(path, "wb");
  if (!f)
    return -1;
  if (len)
    fwrite(data, 1, len, f);
  fclose(f);
  return 0;
}

/* Append a STORED (uncompressed) zip local file record. */
static size_t zip_add(unsigned char *buf, size_t pos, const char *name,
                      const char *data, size_t dlen) {
  uint16_t nlen = (uint16_t)strlen(name);
  put_le32(buf + pos + 0, 0x04034b50);
  put_le16(buf + pos + 4, 20);
  put_le16(buf + pos + 6, 0);
  put_le16(buf + pos + 8, 0); /* method 0 = stored */
  put_le16(buf + pos + 10, 0);
  put_le16(buf + pos + 12, 0);
  put_le32(buf + pos + 14, 0); /* crc (unchecked) */
  put_le32(buf + pos + 18, (uint32_t)dlen);
  put_le32(buf + pos + 22, (uint32_t)dlen);
  put_le16(buf + pos + 26, nlen);
  put_le16(buf + pos + 28, 0);
  pos += 30;
  memcpy(buf + pos, name, nlen);
  pos += nlen;
  memcpy(buf + pos, data, dlen);
  pos += dlen;
  return pos;
}

/* ── XLSX ───────────────────────────────────────────────────── */
static void test_xlsx(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("XLSX: sheet name + shared-string cell");
  scl_allocator_t *a = scl_allocator_default();

  static unsigned char buf[2048];
  size_t pos = 0;
  const char *wb = "<workbook><sheets><sheet name=\"Sheet1\" "
                   "sheetId=\"1\"/></sheets></workbook>";
  const char *ss = "<sst><si><t>Hello</t></si><si><t>World</t></si></sst>";
  const char *s1 = "<worksheet><sheetData><row r=\"1\">"
                   "<c r=\"A1\" t=\"s\"><v>0</v></c>"
                   "<c r=\"B1\" t=\"s\"><v>1</v></c>"
                   "<c r=\"C1\"><v>42</v></c></row></sheetData></worksheet>";
  pos = zip_add(buf, pos, "xl/workbook.xml", wb, strlen(wb));
  pos = zip_add(buf, pos, "xl/sharedStrings.xml", ss, strlen(ss));
  pos = zip_add(buf, pos, "xl/worksheets/sheet1.xml", s1, strlen(s1));

  char path[256];
  write_file("xlsx", buf, pos, path, sizeof(path));

  scl_parse_xlsx_t x;
  SCL_EXPECT_OK(tr, scl_parse_xlsx_open(a, &x, path));
  int sheets = -1;
  SCL_EXPECT_OK(tr, scl_parse_xlsx_get_sheets_count(&x, &sheets));
  SCL_EXPECT_EQ_I(tr, sheets, 1);

  const char *nm = NULL;
  size_t nlen = 0;
  SCL_EXPECT_OK(tr, scl_parse_xlsx_get_sheet_name(&x, 0, &nm, &nlen));
  SCL_EXPECT_EQ_STR(tr, nm, "Sheet1");

  const char *cell = NULL;
  size_t clen = 0;
  SCL_EXPECT_OK(tr, scl_parse_xlsx_get_cell(&x, 0, "A1", &cell, &clen));
  SCL_EXPECT_EQ_STR(tr, cell, "Hello"); /* shared string index 0 */
  SCL_EXPECT_OK(tr, scl_parse_xlsx_get_cell(&x, 0, "B1", &cell, &clen));
  SCL_EXPECT_EQ_STR(tr, cell, "World"); /* shared string index 1 */
  SCL_EXPECT_OK(tr, scl_parse_xlsx_get_cell(&x, 0, "C1", &cell, &clen));
  SCL_EXPECT_EQ_STR(tr, cell, "42"); /* inline numeric */

  scl_parse_xlsx_close(&x);
  remove(path);
  TEST_TRACE_END();
}

/* ── DOCX ───────────────────────────────────────────────────── */
static void test_docx(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("DOCX: text extraction");
  scl_allocator_t *a = scl_allocator_default();

  static unsigned char buf[1024];
  const char *doc = "<w:document><w:body><w:p><w:r>"
                    "<w:t>Hello World</w:t></w:r></w:p></w:body></w:document>";
  size_t pos = zip_add(buf, 0, "word/document.xml", doc, strlen(doc));

  char path[256];
  write_file("docx", buf, pos, path, sizeof(path));

  scl_parse_docx_t d;
  SCL_EXPECT_OK(tr, scl_parse_docx_open(a, &d, path));
  const char *txt = NULL;
  size_t tlen = 0;
  SCL_EXPECT_OK(tr, scl_parse_docx_get_text(&d, &txt, &tlen));
  SCL_EXPECT_NOT_NULL(tr, txt);
  if (txt)
    SCL_EXPECT_TRUE(tr, strstr(txt, "Hello World") != NULL);
  scl_parse_docx_close(&d);
  remove(path);
  TEST_TRACE_END();
}

/* ── PDF ────────────────────────────────────────────────────── */
static void test_pdf(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF: page count + info + out_len guard");
  scl_allocator_t *a = scl_allocator_default();

  /* Build the body, then a valid xref at a recorded offset, then the trailer
   * with /Root and /Info and a correct startxref. */
  char doc[2048];
  size_t n = 0;
  n += (size_t)snprintf(
      doc + n, sizeof(doc) - n,
      "%%PDF-1.7\n"
      "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
      "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
      "3 0 obj\n<< /Type /Page /Parent 2 0 R >>\nendobj\n"
      "5 0 obj\n<< /Title (Hello) /Author (Me) >>\nendobj\n");
  size_t xref_off = n;
  n += (size_t)snprintf(doc + n, sizeof(doc) - n,
                        "xref\n0 6\n"
                        "0000000000 65535 f \n"
                        "0000000009 00000 n \n"
                        "0000000060 00000 n \n"
                        "0000000120 00000 n \n"
                        "0000000000 65535 f \n"
                        "0000000180 00000 n \n"
                        "trailer\n<< /Root 1 0 R /Info 5 0 R /Size 6 >>\n"
                        "startxref\n%zu\n%%%%EOF\n",
                        xref_off);

  char path[256];
  write_file("pdf", (const unsigned char *)doc, n, path, sizeof(path));

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open(a, &p, path));

  int pages = -1;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_page_count(&p, &pages));
  SCL_EXPECT_EQ_I(tr, pages, 1); /* validates the /Pages object-number fix */

  char val[64];
  size_t vlen = sizeof(val);
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_info(&p, "Title", val, &vlen));
  SCL_EXPECT_EQ_STR(tr, val, "Hello");

  /* out_len == 0 must be rejected, not used in a SIZE_MAX memcpy. */
  size_t zero = 0;
  SCL_EXPECT_TRUE(tr, scl_parse_pdf_get_info(&p, "Title", val, &zero) ==
                          SCL_ERR_INVALID_ARG);

  scl_parse_pdf_close(&p);
  remove(path);
  TEST_TRACE_END();
}

/* ── Icelake ────────────────────────────────────────────────── */
static void test_icelake(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("Icelake: metadata.json snapshot + paths");
  scl_allocator_t *a = scl_allocator_default();

  char dir[] = "/tmp/scl_func_ice_XXXXXX";
  if (!mkdtemp(dir)) {
    SCL_EXPECT_TRUE(tr, 0);
    return;
  }

  char mpath[300];
  snprintf(mpath, sizeof(mpath), "%s/metadata.json", dir);
  FILE *f = fopen(mpath, "wb");
  SCL_EXPECT_NOT_NULL(tr, f);
  if (f) {
    const char *json = "{\"snapshot-id\":12345,"
                       "\"manifests\":[{\"manifest-path\":\"m1.avro\"},{"
                       "\"manifest-path\":\"m2.avro\"}],"
                       "\"entries\":[{\"file-path\":\"d1.parquet\"}]}";
    fwrite(json, 1, strlen(json), f);
    fclose(f);
  }

  scl_parse_icelake_t ic;
  SCL_EXPECT_OK(tr, scl_parse_icelake_open(a, &ic, dir));

  int64_t snap = -1;
  SCL_EXPECT_OK(tr, scl_parse_icelake_get_snapshot_id(&ic, &snap));
  SCL_EXPECT_EQ_I(tr, snap, 12345); /* numeric snapshot-id now read */

  int mc = -1;
  SCL_EXPECT_OK(tr, scl_parse_icelake_get_manifest_count(&ic, &mc));
  SCL_EXPECT_EQ_I(tr, mc, 2);

  const char *mp = NULL;
  size_t ml = 0;
  SCL_EXPECT_OK(tr, scl_parse_icelake_get_manifest_path(&ic, 0, &mp, &ml));
  SCL_EXPECT_EQ_STR(tr, mp, "m1.avro");

  const char *dp = NULL;
  size_t dl = 0;
  SCL_EXPECT_OK(tr, scl_parse_icelake_get_data_file_path(&ic, 0, &dp, &dl));
  SCL_EXPECT_EQ_STR(tr, dp, "d1.parquet");

  scl_parse_icelake_close(&ic);
  remove(mpath);
  rmdir(dir);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_xlsx(&tr);
  test_docx(&tr);
  test_pdf(&tr);
  test_icelake(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
