/* PDF 1.7 (ISO 32000-1) parser tests: full object model, classic xref +
 * incremental updates, cross-reference streams with PNG predictors,
 * compressed object streams, the standard filter set, page-tree traversal,
 * text extraction, text-string encodings, encryption detection, repair
 * mode, and adversarial inputs (cycles, bombs, truncation).
 *
 * Also covers the raw-DEFLATE/zlib entry points added to libs/compress.
 *
 * Run under ASan: documents are built in memory and parsed via
 * scl_parse_pdf_open_mem(), so success paths get leak/overflow checking. */

#include "scl_gzip.h"
#include "scl_parse_pdf.h"
#include "scl_test.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Builders ───────────────────────────────────────────────────── */

typedef struct {
  unsigned char buf[65536];
  size_t len;
} doc_t;

static size_t doc_addf(doc_t *d, const char *fmt, ...) SCL_PRINTF(2, 3);
static size_t doc_addf(doc_t *d, const char *fmt, ...) {
  size_t at = d->len;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf((char *)d->buf + d->len, sizeof(d->buf) - d->len, fmt, ap);
  va_end(ap);
  if (n > 0)
    d->len += (size_t)n;
  return at;
}

static size_t doc_add_raw(doc_t *d, const void *p, size_t n) {
  size_t at = d->len;
  memcpy(d->buf + d->len, p, n);
  d->len += n;
  return at;
}

static uint32_t adler32(const unsigned char *p, size_t len) {
  uint32_t a = 1, b = 0;
  for (size_t i = 0; i < len; i++) {
    a = (a + p[i]) % 65521u;
    b = (b + a) % 65521u;
  }
  return (b << 16) | a;
}

/* Wrap raw bytes in a zlib (RFC 1950) container using a stored DEFLATE
 * block — valid input for /FlateDecode. */
static size_t zlib_stored(unsigned char *dst, const unsigned char *src,
                          size_t len) {
  size_t n = 0;
  dst[n++] = 0x78; /* CMF: deflate, 32K window */
  dst[n++] = 0x01; /* FLG: check bits (0x7801 % 31 == 0) */
  dst[n++] = 0x01; /* BFINAL=1, BTYPE=00 (stored) */
  dst[n++] = (unsigned char)(len & 0xFF);
  dst[n++] = (unsigned char)(len >> 8);
  dst[n++] = (unsigned char)(~len & 0xFF);
  dst[n++] = (unsigned char)((~len >> 8) & 0xFF);
  memcpy(dst + n, src, len);
  n += len;
  uint32_t ad = adler32(src, len);
  dst[n++] = (unsigned char)(ad >> 24);
  dst[n++] = (unsigned char)(ad >> 16);
  dst[n++] = (unsigned char)(ad >> 8);
  dst[n++] = (unsigned char)ad;
  return n;
}

/* ASCII85 encoder (reference implementation for round-trip tests). */
static size_t a85_encode(unsigned char *dst, const unsigned char *src,
                         size_t len) {
  size_t n = 0;
  size_t i = 0;
  while (i + 4 <= len) {
    uint32_t v = ((uint32_t)src[i] << 24) | ((uint32_t)src[i + 1] << 16) |
                 ((uint32_t)src[i + 2] << 8) | src[i + 3];
    if (v == 0) {
      dst[n++] = 'z';
    } else {
      for (int k = 4; k >= 0; k--) {
        uint32_t pow = 1;
        for (int j = 0; j < k; j++)
          pow *= 85;
        dst[n++] = (unsigned char)('!' + (v / pow) % 85);
      }
    }
    i += 4;
  }
  size_t rem = len - i;
  if (rem > 0) {
    unsigned char tail[4] = {0, 0, 0, 0};
    memcpy(tail, src + i, rem);
    uint32_t v = ((uint32_t)tail[0] << 24) | ((uint32_t)tail[1] << 16) |
                 ((uint32_t)tail[2] << 8) | tail[3];
    unsigned char digits[5];
    for (int k = 4; k >= 0; k--) {
      digits[4 - k] = (unsigned char)('!' + (v / (uint32_t[]){1, 85, 7225,
                                                              614125,
                                                              52200625}[k]) %
                                                85);
    }
    for (size_t k = 0; k < rem + 1; k++)
      dst[n++] = digits[k];
  }
  dst[n++] = '~';
  dst[n++] = '>';
  return n;
}

/* ── 1. Full object model ───────────────────────────────────────── */
static void test_object_model(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: all eight object types + refs");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.7\n");
  size_t o1 = doc_addf(
      &d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  size_t o2 = doc_addf(
      &d, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  size_t o3 = doc_addf(&d, "3 0 obj\n<< /Type /Page /Parent 2 0 R "
                           "/MediaBox [0 0 612 792] >>\nendobj\n");
  size_t o4 = doc_addf(
      &d, "4 0 obj\n[ true false null 42 -17 3.5 -0.25 (lit\\)eral) "
          "<48656C6C6F> /Na#6De 5 0 R [1 2] << /K 7 >> ]\nendobj\n");
  size_t o5 = doc_addf(&d, "5 0 obj\n(indirect target)\nendobj\n");
  size_t xr = doc_addf(&d, "xref\n0 6\n");
  doc_addf(&d, "0000000000 65535 f \n");
  doc_addf(&d, "%010zu 00000 n \n", o1);
  doc_addf(&d, "%010zu 00000 n \n", o2);
  doc_addf(&d, "%010zu 00000 n \n", o3);
  doc_addf(&d, "%010zu 00000 n \n", o4);
  doc_addf(&d, "%010zu 00000 n \n", o5);
  doc_addf(&d, "trailer\n<< /Size 6 /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
           xr);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));

  int maj = 0, min = 0;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_version(&p, &maj, &min));
  SCL_EXPECT_EQ_I(tr, maj, 1);
  SCL_EXPECT_EQ_I(tr, min, 7);
  SCL_EXPECT_FALSE(tr, scl_parse_pdf_is_encrypted(&p));

  scl_pdf_obj_t *arr = NULL;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_object(&p, 4, 0, &arr));
  SCL_EXPECT_TRUE(tr, arr && arr->type == SCL_PDF_OBJ_ARRAY);
  if (arr && arr->type == SCL_PDF_OBJ_ARRAY) {
    SCL_EXPECT_EQ_SZ(tr, arr->u.array.count, (size_t)13);
    scl_pdf_obj_t **it = arr->u.array.items;
    SCL_EXPECT_TRUE(tr, it[0]->type == SCL_PDF_OBJ_BOOL && it[0]->u.boolean);
    SCL_EXPECT_TRUE(tr, it[1]->type == SCL_PDF_OBJ_BOOL && !it[1]->u.boolean);
    SCL_EXPECT_TRUE(tr, it[2]->type == SCL_PDF_OBJ_NULL);
    SCL_EXPECT_TRUE(tr, it[3]->type == SCL_PDF_OBJ_INT &&
                            it[3]->u.integer == 42);
    SCL_EXPECT_TRUE(tr, it[4]->type == SCL_PDF_OBJ_INT &&
                            it[4]->u.integer == -17);
    SCL_EXPECT_TRUE(tr, it[5]->type == SCL_PDF_OBJ_REAL &&
                            it[5]->u.real > 3.49 && it[5]->u.real < 3.51);
    SCL_EXPECT_TRUE(tr, it[6]->type == SCL_PDF_OBJ_REAL &&
                            it[6]->u.real < -0.24 && it[6]->u.real > -0.26);
    SCL_EXPECT_TRUE(tr, it[7]->type == SCL_PDF_OBJ_STRING);
    if (it[7]->type == SCL_PDF_OBJ_STRING)
      SCL_EXPECT_EQ_STR(tr, it[7]->u.string.data, "lit)eral");
    SCL_EXPECT_TRUE(tr, it[8]->type == SCL_PDF_OBJ_STRING);
    if (it[8]->type == SCL_PDF_OBJ_STRING)
      SCL_EXPECT_EQ_STR(tr, it[8]->u.string.data, "Hello");
    SCL_EXPECT_TRUE(tr, it[9]->type == SCL_PDF_OBJ_NAME);
    if (it[9]->type == SCL_PDF_OBJ_NAME)
      SCL_EXPECT_EQ_STR(tr, it[9]->u.string.data, "Name"); /* #6D = 'm' */
    SCL_EXPECT_TRUE(tr, it[10]->type == SCL_PDF_OBJ_REF &&
                            it[10]->u.ref.num == 5);
    SCL_EXPECT_TRUE(tr, it[11]->type == SCL_PDF_OBJ_ARRAY &&
                            it[11]->u.array.count == 2);
    SCL_EXPECT_TRUE(tr, it[12]->type == SCL_PDF_OBJ_DICT);

    scl_pdf_obj_t *resolved = NULL;
    SCL_EXPECT_OK(tr, scl_parse_pdf_resolve(&p, it[10], &resolved));
    SCL_EXPECT_TRUE(tr, resolved && resolved->type == SCL_PDF_OBJ_STRING);
    if (resolved && resolved->type == SCL_PDF_OBJ_STRING)
      SCL_EXPECT_EQ_STR(tr, resolved->u.string.data, "indirect target");
  }

  /* Page tree + MediaBox via the object API. */
  int pages = -1;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_page_count(&p, &pages));
  SCL_EXPECT_EQ_I(tr, pages, 1);
  scl_pdf_obj_t *page = NULL;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_page(&p, 0, &page));
  scl_pdf_obj_t *mb = scl_pdf_dict_get(page, "MediaBox");
  SCL_EXPECT_TRUE(tr, mb && mb->type == SCL_PDF_OBJ_ARRAY &&
                          mb->u.array.count == 4);
  SCL_EXPECT_TRUE(tr, scl_parse_pdf_get_page(&p, 1, &page) ==
                          SCL_ERR_INVALID_INDEX);

  scl_parse_pdf_close(&p);
  (void)o3;
  TEST_TRACE_END();
}

/* ── 2. Incremental update (/Prev chain): newest section wins ───── */
static void test_incremental_update(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: incremental update overrides object");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.7\n");
  size_t o1 = doc_addf(&d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\n"
                           "endobj\n");
  size_t o2 = doc_addf(
      &d, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  size_t o3 = doc_addf(&d, "3 0 obj\n<< /Type /Page /Parent 2 0 R >>\n"
                           "endobj\n");
  size_t o4 = doc_addf(&d, "4 0 obj\n<< /Title (Original) >>\nendobj\n");
  size_t xr1 = doc_addf(&d, "xref\n0 5\n");
  doc_addf(&d, "0000000000 65535 f \n");
  doc_addf(&d, "%010zu 00000 n \n", o1);
  doc_addf(&d, "%010zu 00000 n \n", o2);
  doc_addf(&d, "%010zu 00000 n \n", o3);
  doc_addf(&d, "%010zu 00000 n \n", o4);
  doc_addf(&d,
           "trailer\n<< /Size 5 /Root 1 0 R /Info 4 0 R >>\nstartxref\n"
           "%zu\n%%%%EOF\n",
           xr1);
  /* Incremental section: replace object 4. */
  size_t o4b = doc_addf(&d, "4 0 obj\n<< /Title (Updated) >>\nendobj\n");
  size_t xr2 = doc_addf(&d, "xref\n4 1\n");
  doc_addf(&d, "%010zu 00000 n \n", o4b);
  doc_addf(&d,
           "trailer\n<< /Size 5 /Root 1 0 R /Info 4 0 R /Prev %zu >>\n"
           "startxref\n%zu\n%%%%EOF\n",
           xr1, xr2);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  char val[64];
  size_t vlen = sizeof(val);
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_info(&p, "Title", val, &vlen));
  SCL_EXPECT_EQ_STR(tr, val, "Updated");
  int pages = -1;
  scl_parse_pdf_get_page_count(&p, &pages);
  SCL_EXPECT_EQ_I(tr, pages, 1);
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

/* ── 3. Xref stream + PNG predictor + object stream ─────────────── */
static void test_xref_stream_objstm(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: xref stream (predictor 12) + ObjStm");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.5\n");
  size_t o1 = doc_addf(&d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\n"
                           "endobj\n");
  size_t o2 = doc_addf(
      &d, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  size_t o3 = doc_addf(&d, "3 0 obj\n<< /Type /Page /Parent 2 0 R "
                           "/Contents 4 0 R >>\nendobj\n");
  const char *content = "BT (Hello) Tj T* (World) Tj ET";
  size_t o4 = doc_addf(&d, "4 0 obj\n<< /Length %zu >>\nstream\n%s\n"
                           "endstream\nendobj\n",
                       strlen(content), content);

  /* Object stream (obj 5) holding object 6: << /Title (FromObjStm) >>. */
  const char *stm_body = "6 0 << /Title (FromObjStm) >>";
  size_t first = 4; /* strlen("6 0 ") */
  unsigned char zbuf[512];
  size_t zlen = zlib_stored(zbuf, (const unsigned char *)stm_body,
                            strlen(stm_body));
  size_t o5 = doc_addf(&d,
                       "5 0 obj\n<< /Type /ObjStm /N 1 /First %zu /Length "
                       "%zu /Filter /FlateDecode >>\nstream\n",
                       first, zlen);
  doc_add_raw(&d, zbuf, zlen);
  doc_addf(&d, "\nendstream\nendobj\n");

  /* Xref stream (obj 7): W [1 2 1], 8 entries, PNG predictor 12 (Up). */
  size_t xr_off = d.len; /* the xref stream object starts here */
  unsigned char rows[8][4];
  uint64_t offs[8] = {0, o1, o2, o3, o4, o5, 0, xr_off};
  for (int i = 0; i < 8; i++) {
    uint8_t type = (i == 0) ? 0 : (i == 6) ? 2 : 1;
    uint64_t f2 = (i == 6) ? 5 : offs[i]; /* obj 6 lives in objstm 5 */
    uint8_t f3 = 0;                       /* gen / index-in-objstm */
    rows[i][0] = type;
    rows[i][1] = (unsigned char)(f2 >> 8);
    rows[i][2] = (unsigned char)(f2 & 0xFF);
    rows[i][3] = f3;
  }
  /* Apply PNG "Up" filter: enc[r] = row[r] - row[r-1] (byte-wise). */
  unsigned char pred[8 * 5];
  for (int r = 0; r < 8; r++) {
    pred[r * 5] = 2; /* filter type: Up */
    for (int i = 0; i < 4; i++) {
      unsigned char up = r > 0 ? rows[r - 1][i] : 0;
      pred[r * 5 + 1 + i] = (unsigned char)(rows[r][i] - up);
    }
  }
  unsigned char zx[512];
  size_t zxlen = zlib_stored(zx, pred, sizeof(pred));
  doc_addf(&d,
           "7 0 obj\n<< /Type /XRef /Size 8 /W [1 2 1] /Root 1 0 R "
           "/Info 6 0 R /Filter /FlateDecode /DecodeParms "
           "<< /Predictor 12 /Columns 4 >> /Length %zu >>\nstream\n",
           zxlen);
  doc_add_raw(&d, zx, zxlen);
  doc_addf(&d, "\nendstream\nendobj\n");
  doc_addf(&d, "startxref\n%zu\n%%%%EOF\n", xr_off);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));

  int pages = -1;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_page_count(&p, &pages));
  SCL_EXPECT_EQ_I(tr, pages, 1);

  /* Info dict lives inside the object stream (type-2 entry). */
  char val[64];
  size_t vlen = sizeof(val);
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_info(&p, "Title", val, &vlen));
  SCL_EXPECT_EQ_STR(tr, val, "FromObjStm");

  /* Text extraction through the page's /Contents. */
  char *text = NULL;
  size_t tlen = 0;
  SCL_EXPECT_OK(tr, scl_parse_pdf_extract_text(&p, 0, &text, &tlen));
  SCL_EXPECT_TRUE(tr, text != NULL);
  if (text) {
    SCL_EXPECT_TRUE(tr, strstr(text, "Hello") != NULL);
    SCL_EXPECT_TRUE(tr, strstr(text, "World") != NULL);
    scl_free(a, text);
  }
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

/* ── 4. Filters ─────────────────────────────────────────────────── */
static void put_stream_doc(doc_t *d, const char *filter_and_parms,
                           const unsigned char *data, size_t dlen,
                           size_t *startxref_at) {
  d->len = 0;
  doc_addf(d, "%%PDF-1.7\n");
  size_t o1 =
      doc_addf(d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  size_t o2 = doc_addf(
      d, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  size_t o3 =
      doc_addf(d, "3 0 obj\n<< /Type /Page /Parent 2 0 R >>\nendobj\n");
  size_t o4 = doc_addf(d, "4 0 obj\n<< %s /Length %zu >>\nstream\n",
                       filter_and_parms, dlen);
  doc_add_raw(d, data, dlen);
  doc_addf(d, "\nendstream\nendobj\n");
  size_t xr = doc_addf(d, "xref\n0 5\n0000000000 65535 f \n");
  doc_addf(d, "%010zu 00000 n \n", o1);
  doc_addf(d, "%010zu 00000 n \n", o2);
  doc_addf(d, "%010zu 00000 n \n", o3);
  doc_addf(d, "%010zu 00000 n \n", o4);
  doc_addf(d, "trailer\n<< /Size 5 /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
           xr);
  *startxref_at = xr;
}

static void expect_stream_decodes_to(scl_test_runner_t *tr,
                                     const char *filter_and_parms,
                                     const unsigned char *enc, size_t enc_len,
                                     const unsigned char *want,
                                     size_t want_len) {
  scl_allocator_t *a = scl_allocator_default();
  doc_t d;
  size_t xr;
  put_stream_doc(&d, filter_and_parms, enc, enc_len, &xr);
  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  scl_pdf_obj_t *stream = NULL;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_object(&p, 4, 0, &stream));
  SCL_EXPECT_TRUE(tr, stream && stream->type == SCL_PDF_OBJ_STREAM);
  void *out = NULL;
  size_t out_len = 0;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_stream_data(&p, stream, &out, &out_len));
  SCL_EXPECT_EQ_SZ(tr, out_len, want_len);
  if (out && out_len == want_len)
    SCL_EXPECT_TRUE(tr, memcmp(out, want, want_len) == 0);
  scl_free(a, out);
  scl_parse_pdf_close(&p);
}

static void test_filters(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: ASCIIHex/ASCII85/RunLength/LZW/chained filters");
  const unsigned char hello[] = "Hello, PDF filters!";
  size_t hlen = sizeof(hello) - 1;

  /* ASCIIHexDecode. */
  unsigned char hex[128];
  size_t hn = 0;
  for (size_t i = 0; i < hlen; i++)
    hn += (size_t)snprintf((char *)hex + hn, sizeof(hex) - hn, "%02X",
                           hello[i]);
  hex[hn++] = '>';
  expect_stream_decodes_to(tr, "/Filter /ASCIIHexDecode", hex, hn, hello,
                           hlen);

  /* ASCII85Decode (encoder-verified round trip). */
  unsigned char a85[128];
  size_t an = a85_encode(a85, hello, hlen);
  expect_stream_decodes_to(tr, "/Filter /ASCII85Decode", a85, an, hello, hlen);

  /* RunLengthDecode: literal run + repeat run. */
  const unsigned char rl_enc[] = {
      4, 'H', 'e', 'l', 'l', 'o',    /* literal x5 */
      (unsigned char)(257 - 6), '!', /* '!' x6 */
      128                            /* EOD */
  };
  const unsigned char rl_dec[] = "Hello!!!!!!";
  expect_stream_decodes_to(tr, "/Filter /RunLengthDecode", rl_enc,
                           sizeof(rl_enc), rl_dec, sizeof(rl_dec) - 1);

  /* LZWDecode: hand-packed 9-bit code sequence
   *   256 (clear) 65 ('A') 66 ('B') 258 ("AB", KwKwK-adjacent) 257 (EOD)
   * which must decode to "ABAB". */
  const unsigned char lzw_enc[] = {0x80, 0x10, 0x48, 0x50, 0x28, 0x08};
  const unsigned char lzw_dec[] = "ABAB";
  expect_stream_decodes_to(tr, "/Filter /LZWDecode", lzw_enc, sizeof(lzw_enc),
                           lzw_dec, sizeof(lzw_dec) - 1);

  /* Filter cascade: RunLength encoded, then hex encoded on top.
   * Decode order is first-to-last: AHx first, then RL (§7.4.1). */
  unsigned char chain[256];
  size_t cn = 0;
  for (size_t i = 0; i < sizeof(rl_enc); i++)
    cn += (size_t)snprintf((char *)chain + cn, sizeof(chain) - cn, "%02x",
                           rl_enc[i]);
  chain[cn++] = '>';
  expect_stream_decodes_to(tr, "/Filter [/ASCIIHexDecode /RunLengthDecode]",
                           chain, cn, rl_dec, sizeof(rl_dec) - 1);

  /* FlateDecode via zlib stored block. */
  unsigned char z[256];
  size_t zn = zlib_stored(z, hello, hlen);
  expect_stream_decodes_to(tr, "/Filter /FlateDecode", z, zn, hello, hlen);
  TEST_TRACE_END();
}

/* ── 5. Text-string encodings in the Info dict ──────────────────── */
static void test_info_encodings(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: UTF-16BE + PDFDocEncoding info strings");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.7\n");
  size_t o1 =
      doc_addf(&d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  size_t o2 =
      doc_addf(&d, "2 0 obj\n<< /Type /Pages /Kids [] /Count 0 >>\nendobj\n");
  /* /Title: UTF-16BE "Hié" (FEFF 0048 0069 00E9); /Author: latin-1 é. */
  size_t o3 = doc_addf(
      &d, "3 0 obj\n<< /Title <FEFF0048006900E9> /Author (Caf\351) >>\n"
          "endobj\n");
  size_t xr = doc_addf(&d, "xref\n0 4\n0000000000 65535 f \n");
  doc_addf(&d, "%010zu 00000 n \n", o1);
  doc_addf(&d, "%010zu 00000 n \n", o2);
  doc_addf(&d, "%010zu 00000 n \n", o3);
  doc_addf(&d,
           "trailer\n<< /Size 4 /Root 1 0 R /Info 3 0 R >>\nstartxref\n"
           "%zu\n%%%%EOF\n",
           xr);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  char val[64];
  size_t vlen = sizeof(val);
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_info(&p, "Title", val, &vlen));
  SCL_EXPECT_EQ_STR(tr, val, "Hi\xC3\xA9"); /* UTF-8 for "Hié" */
  vlen = sizeof(val);
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_info(&p, "Author", val, &vlen));
  SCL_EXPECT_EQ_STR(tr, val, "Caf\xC3\xA9");
  /* out_len == 0 must be rejected (legacy contract). */
  size_t zero = 0;
  SCL_EXPECT_TRUE(tr, scl_parse_pdf_get_info(&p, "Title", val, &zero) ==
                          SCL_ERR_INVALID_ARG);
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

/* ── 6. Encryption detection ────────────────────────────────────── */
static void test_encryption_detect(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: /Encrypt detected; payload access refused");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.7\n");
  size_t o1 =
      doc_addf(&d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  size_t o2 = doc_addf(
      &d, "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
  size_t o3 = doc_addf(&d, "3 0 obj\n<< /Type /Page /Parent 2 0 R "
                           "/Contents 4 0 R >>\nendobj\n");
  size_t o4 = doc_addf(
      &d, "4 0 obj\n<< /Length 5 >>\nstream\nXXXXX\nendstream\nendobj\n");
  size_t o5 = doc_addf(
      &d, "5 0 obj\n<< /Filter /Standard /V 2 /R 3 >>\nendobj\n");
  size_t xr = doc_addf(&d, "xref\n0 6\n0000000000 65535 f \n");
  doc_addf(&d, "%010zu 00000 n \n", o1);
  doc_addf(&d, "%010zu 00000 n \n", o2);
  doc_addf(&d, "%010zu 00000 n \n", o3);
  doc_addf(&d, "%010zu 00000 n \n", o4);
  doc_addf(&d, "%010zu 00000 n \n", o5);
  doc_addf(&d,
           "trailer\n<< /Size 6 /Root 1 0 R /Encrypt 5 0 R >>\nstartxref\n"
           "%zu\n%%%%EOF\n",
           xr);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  SCL_EXPECT_TRUE(tr, scl_parse_pdf_is_encrypted(&p));
  char *text = NULL;
  size_t tlen = 0;
  SCL_EXPECT_TRUE(tr, scl_parse_pdf_extract_text(&p, 0, &text, &tlen) ==
                          SCL_ERR_UNSUPPORTED);
  scl_pdf_obj_t *stream = NULL;
  scl_parse_pdf_get_object(&p, 4, 0, &stream);
  void *out = NULL;
  size_t olen = 0;
  SCL_EXPECT_TRUE(tr, scl_parse_pdf_get_stream_data(&p, stream, &out, &olen) ==
                          SCL_ERR_UNSUPPORTED);
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

/* ── 7. Repair mode ─────────────────────────────────────────────── */
static void test_repair(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: broken/absent xref rebuilt by scanning");
  scl_allocator_t *a = scl_allocator_default();

  /* No xref at all — just objects and a trailer. */
  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.7\n"
               "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n"
               "2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n"
               "3 0 obj\n<< /Type /Page /Parent 2 0 R >>\nendobj\n"
               "5 0 obj\n<< /Title (Recovered) >>\nendobj\n"
               "trailer\n<< /Size 6 /Root 1 0 R /Info 5 0 R >>\n"
               "startxref\n999999999\n%%%%EOF\n");

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  int pages = -1;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_page_count(&p, &pages));
  SCL_EXPECT_EQ_I(tr, pages, 1);
  char val[64];
  size_t vlen = sizeof(val);
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_info(&p, "Title", val, &vlen));
  SCL_EXPECT_EQ_STR(tr, val, "Recovered");
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

/* ── 8. Adversarial inputs ──────────────────────────────────────── */
static void test_adversarial(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: cycles, deep nesting, loops, huge lengths");
  scl_allocator_t *a = scl_allocator_default();

  /* Reference cycle: 1 -> 2 -> 1. Resolve must terminate. */
  {
    doc_t d = {.len = 0};
    doc_addf(&d, "%%PDF-1.7\n");
    size_t o1 = doc_addf(&d, "1 0 obj\n2 0 R\nendobj\n");
    size_t o2 = doc_addf(&d, "2 0 obj\n1 0 R\nendobj\n");
    size_t xr = doc_addf(&d, "xref\n0 3\n0000000000 65535 f \n");
    doc_addf(&d, "%010zu 00000 n \n", o1);
    doc_addf(&d, "%010zu 00000 n \n", o2);
    doc_addf(&d, "trailer\n<< /Size 3 /Root 1 0 R >>\nstartxref\n%zu\n"
                 "%%%%EOF\n",
             xr);
    scl_parse_pdf_t p;
    if (scl_parse_pdf_open_mem(a, &p, d.buf, d.len) == SCL_OK) {
      scl_pdf_obj_t *o = NULL;
      if (scl_parse_pdf_get_object(&p, 1, 0, &o) == SCL_OK && o) {
        scl_pdf_obj_t *r = NULL;
        /* Must return an error or a non-ref; must not hang. */
        scl_parse_pdf_resolve(&p, o, &r);
        SCL_EXPECT_TRUE(tr, r == NULL || r->type != SCL_PDF_OBJ_REF);
      }
      scl_parse_pdf_close(&p);
    }
    SCL_EXPECT_TRUE(tr, 1); /* reached: no hang/crash */
  }

  /* Nesting deeper than SCL_PDF_MAX_DEPTH: parse must fail cleanly. */
  {
    doc_t d = {.len = 0};
    doc_addf(&d, "%%PDF-1.7\n");
    size_t o1 = doc_addf(&d, "1 0 obj\n");
    for (int i = 0; i < 100; i++)
      doc_addf(&d, "[");
    doc_addf(&d, "42");
    for (int i = 0; i < 100; i++)
      doc_addf(&d, "]");
    doc_addf(&d, "\nendobj\n");
    size_t xr = doc_addf(&d, "xref\n0 2\n0000000000 65535 f \n");
    doc_addf(&d, "%010zu 00000 n \n", o1);
    doc_addf(&d, "trailer\n<< /Size 2 /Root 1 0 R >>\nstartxref\n%zu\n"
                 "%%%%EOF\n",
             xr);
    scl_parse_pdf_t p;
    if (scl_parse_pdf_open_mem(a, &p, d.buf, d.len) == SCL_OK) {
      scl_pdf_obj_t *o = NULL;
      scl_error_t err = scl_parse_pdf_get_object(&p, 1, 0, &o);
      SCL_EXPECT_TRUE(tr, err != SCL_OK || o == NULL ||
                              o->type != SCL_PDF_OBJ_ARRAY);
      scl_parse_pdf_close(&p);
    }
    SCL_EXPECT_TRUE(tr, 1);
  }

  /* /Prev pointing at itself: chain walk must terminate. */
  {
    doc_t d = {.len = 0};
    doc_addf(&d, "%%PDF-1.7\n");
    size_t o1 =
        doc_addf(&d, "1 0 obj\n<< /Type /Catalog >>\nendobj\n");
    size_t xr = doc_addf(&d, "xref\n0 2\n0000000000 65535 f \n");
    doc_addf(&d, "%010zu 00000 n \n", o1);
    doc_addf(&d,
             "trailer\n<< /Size 2 /Root 1 0 R /Prev %zu >>\nstartxref\n"
             "%zu\n%%%%EOF\n",
             xr, xr);
    scl_parse_pdf_t p;
    if (scl_parse_pdf_open_mem(a, &p, d.buf, d.len) == SCL_OK)
      scl_parse_pdf_close(&p);
    SCL_EXPECT_TRUE(tr, 1);
  }

  /* Huge /Length far past EOF: must clamp via endstream scan. */
  {
    doc_t d = {.len = 0};
    doc_addf(&d, "%%PDF-1.7\n");
    size_t o1 = doc_addf(&d, "1 0 obj\n<< /Length 999999999 >>\nstream\n"
                             "tiny\nendstream\nendobj\n");
    size_t xr = doc_addf(&d, "xref\n0 2\n0000000000 65535 f \n");
    doc_addf(&d, "%010zu 00000 n \n", o1);
    doc_addf(&d, "trailer\n<< /Size 2 /Root 1 0 R >>\nstartxref\n%zu\n"
                 "%%%%EOF\n",
             xr);
    scl_parse_pdf_t p;
    if (scl_parse_pdf_open_mem(a, &p, d.buf, d.len) == SCL_OK) {
      scl_pdf_obj_t *o = NULL;
      if (scl_parse_pdf_get_object(&p, 1, 0, &o) == SCL_OK && o &&
          o->type == SCL_PDF_OBJ_STREAM) {
        void *out = NULL;
        size_t olen = 0;
        if (scl_parse_pdf_get_stream_data(&p, o, &out, &olen) == SCL_OK) {
          SCL_EXPECT_EQ_SZ(tr, olen, (size_t)4); /* "tiny" */
          scl_free(a, out);
        }
      }
      scl_parse_pdf_close(&p);
    }
    SCL_EXPECT_TRUE(tr, 1);
  }

  /* Truncated garbage: never crash, error or degrade gracefully. */
  {
    static const char *garbage[] = {
        "%PDF-1.7",
        "%PDF-1.7\n1 0 obj\n<< /K (unterminated",
        "%PDF-1.7\nxref\n0 99999999999\ntrailer",
        "%PDF-1.7\n1 0 obj\n<<>>\nstream\nnoend",
        "%PDF-",
        "",
    };
    for (size_t i = 0; i < sizeof(garbage) / sizeof(garbage[0]); i++) {
      scl_parse_pdf_t p;
      size_t glen = strlen(garbage[i]);
      scl_error_t err = scl_parse_pdf_open_mem(a, &p, garbage[i], glen);
      if (err == SCL_OK) {
        int pages;
        scl_parse_pdf_get_page_count(&p, &pages);
        scl_parse_pdf_close(&p);
      }
    }
    SCL_EXPECT_TRUE(tr, 1);
  }
  TEST_TRACE_END();
}

/* ── 9. zlib / raw-DEFLATE entry points (libs/compress) ─────────── */
static void test_zlib_inflate_api(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("compress: scl_zlib_decompress + scl_inflate_raw");
  scl_allocator_t *a = scl_allocator_default();
  const unsigned char msg[] = "zlib round trip payload 0123456789";
  size_t mlen = sizeof(msg) - 1;

  unsigned char z[256];
  size_t zlen = zlib_stored(z, msg, mlen);

  void *out = NULL;
  size_t out_len = 0;
  SCL_EXPECT_OK(tr, scl_zlib_decompress(a, z, zlen, &out, &out_len, 0));
  SCL_EXPECT_EQ_SZ(tr, out_len, mlen);
  if (out && out_len == mlen)
    SCL_EXPECT_TRUE(tr, memcmp(out, msg, mlen) == 0);
  scl_free(a, out);

  /* Corrupted Adler-32 must be rejected. */
  z[zlen - 1] ^= 0xFF;
  out = NULL;
  SCL_EXPECT_TRUE(tr, scl_zlib_decompress(a, z, zlen, &out, &out_len, 0) ==
                          SCL_ERR_PARSE);
  z[zlen - 1] ^= 0xFF;

  /* Bad header check bits must be rejected. */
  unsigned char bad[8] = {0x78, 0x02, 0, 0, 0, 0, 0, 0};
  SCL_EXPECT_TRUE(tr, scl_zlib_decompress(a, bad, sizeof(bad), &out, &out_len,
                                          0) == SCL_ERR_PARSE);

  /* Output cap (decompression-bomb defence). */
  SCL_EXPECT_TRUE(tr, scl_zlib_decompress(a, z, zlen, &out, &out_len, 8) ==
                          SCL_ERR_SIZE_OVERFLOW);

  /* Raw DEFLATE (no wrapper): skip the 2-byte header, drop the trailer. */
  SCL_EXPECT_OK(tr,
                scl_inflate_raw(a, z + 2, zlen - 6, &out, &out_len, 0));
  SCL_EXPECT_EQ_SZ(tr, out_len, mlen);
  if (out && out_len == mlen)
    SCL_EXPECT_TRUE(tr, memcmp(out, msg, mlen) == 0);
  scl_free(a, out);
  TEST_TRACE_END();
}

/* ── 10. Catalog /Version override ──────────────────────────────── */
static void test_version_override(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: catalog /Version overrides older header");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.4\n");
  size_t o1 = doc_addf(
      &d, "1 0 obj\n<< /Type /Catalog /Version /1.7 /Pages 2 0 R >>\n"
          "endobj\n");
  size_t o2 =
      doc_addf(&d, "2 0 obj\n<< /Type /Pages /Kids [] /Count 0 >>\nendobj\n");
  size_t xr = doc_addf(&d, "xref\n0 3\n0000000000 65535 f \n");
  doc_addf(&d, "%010zu 00000 n \n", o1);
  doc_addf(&d, "%010zu 00000 n \n", o2);
  doc_addf(&d, "trailer\n<< /Size 3 /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
           xr);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  int maj = 0, min = 0;
  scl_parse_pdf_get_version(&p, &maj, &min);
  SCL_EXPECT_EQ_I(tr, maj, 1);
  SCL_EXPECT_EQ_I(tr, min, 7);
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

/* ── 11. Multi-page tree with nested Pages nodes ────────────────── */
static void test_page_tree(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("PDF17: nested page tree, document order");
  scl_allocator_t *a = scl_allocator_default();

  doc_t d = {.len = 0};
  doc_addf(&d, "%%PDF-1.7\n");
  size_t o1 =
      doc_addf(&d, "1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
  size_t o2 = doc_addf(&d, "2 0 obj\n<< /Type /Pages /Kids [3 0 R 6 0 R] "
                           "/Count 3 >>\nendobj\n");
  size_t o3 = doc_addf(&d, "3 0 obj\n<< /Type /Pages /Parent 2 0 R "
                           "/Kids [4 0 R 5 0 R] /Count 2 >>\nendobj\n");
  size_t o4 = doc_addf(&d, "4 0 obj\n<< /Type /Page /Parent 3 0 R "
                           "/PageLabel (A) >>\nendobj\n");
  size_t o5 = doc_addf(&d, "5 0 obj\n<< /Type /Page /Parent 3 0 R "
                           "/PageLabel (B) >>\nendobj\n");
  size_t o6 = doc_addf(&d, "6 0 obj\n<< /Type /Page /Parent 2 0 R "
                           "/PageLabel (C) >>\nendobj\n");
  size_t xr = doc_addf(&d, "xref\n0 7\n0000000000 65535 f \n");
  doc_addf(&d, "%010zu 00000 n \n", o1);
  doc_addf(&d, "%010zu 00000 n \n", o2);
  doc_addf(&d, "%010zu 00000 n \n", o3);
  doc_addf(&d, "%010zu 00000 n \n", o4);
  doc_addf(&d, "%010zu 00000 n \n", o5);
  doc_addf(&d, "%010zu 00000 n \n", o6);
  doc_addf(&d, "trailer\n<< /Size 7 /Root 1 0 R >>\nstartxref\n%zu\n%%%%EOF\n",
           xr);

  scl_parse_pdf_t p;
  SCL_EXPECT_OK(tr, scl_parse_pdf_open_mem(a, &p, d.buf, d.len));
  int pages = -1;
  SCL_EXPECT_OK(tr, scl_parse_pdf_get_page_count(&p, &pages));
  SCL_EXPECT_EQ_I(tr, pages, 3);
  const char *want[3] = {"A", "B", "C"};
  for (int i = 0; i < 3; i++) {
    scl_pdf_obj_t *page = NULL;
    SCL_EXPECT_OK(tr, scl_parse_pdf_get_page(&p, i, &page));
    scl_pdf_obj_t *label = scl_pdf_dict_get(page, "PageLabel");
    SCL_EXPECT_TRUE(tr, label && label->type == SCL_PDF_OBJ_STRING);
    if (label && label->type == SCL_PDF_OBJ_STRING)
      SCL_EXPECT_EQ_STR(tr, label->u.string.data, want[i]);
  }
  scl_parse_pdf_close(&p);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_object_model(&tr);
  test_incremental_update(&tr);
  test_xref_stream_objstm(&tr);
  test_filters(&tr);
  test_info_encodings(&tr);
  test_encryption_detect(&tr);
  test_repair(&tr);
  test_adversarial(&tr);
  test_zlib_inflate_api(&tr);
  test_version_override(&tr);
  test_page_tree(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
