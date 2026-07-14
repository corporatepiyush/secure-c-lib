/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* PDF 1.7 parser (ISO 32000-1:2008).
 *
 * ── What this provides ───────────────────────────────────────────────────────
 *
 *   • Full COS object model (§7.3): null, boolean, integer, real, string
 *     (literal + hex), name (with #xx escapes), array, dictionary, stream,
 *     and indirect references.
 *   • Cross-reference tables (§7.5.4) AND cross-reference streams (§7.5.8),
 *     including /Prev chains, hybrid files (/XRefStm), and incremental
 *     updates. Broken xref offsets trigger a repair scan of the whole file.
 *   • Compressed object streams — /Type /ObjStm (§7.5.7).
 *   • Stream filters (§7.4): FlateDecode (zlib via libs/compress),
 *     LZWDecode, ASCIIHexDecode, ASCII85Decode, RunLengthDecode; filter
 *     chains; PNG predictors 10–15 and TIFF predictor 2.
 *   • Page tree traversal (§7.7.3) with cycle detection.
 *   • Content-stream text extraction (Tj, TJ, ', " operators, §9.4.3).
 *   • Document information dictionary (§14.3.3) with UTF-16BE and
 *     PDFDocEncoding text-string decoding (§7.9.2.2).
 *   • Encryption detection (§7.6): /Encrypt is detected and reported;
 *     encrypted string/stream payloads are NOT decrypted.
 *
 * ── Security ─────────────────────────────────────────────────────────────────
 *
 * Every offset/length/count read from the file is validated against the real
 * buffer before use. Recursion is depth-limited, object resolution is
 * cycle-checked, xref chains are bounded, filter cascades are capped, and
 * decompression output is capped (decompression-bomb defence). All limits
 * below are compile-time overridable.
 */

#ifndef SCL_PARSE_PDF_H
#define SCL_PARSE_PDF_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

/* ── Hard limits on untrusted input (overridable) ───────────── */

/* Maximum number of indirect objects (xref capacity). */
#ifndef SCL_PDF_MAX_OBJECTS
#define SCL_PDF_MAX_OBJECTS 65536
#endif
/* Maximum size of PDF file to load (default 256 MB). */
#ifndef SCL_PDF_MAX_FILE_SIZE
#define SCL_PDF_MAX_FILE_SIZE ((size_t)256 * 1024 * 1024)
#endif
/* Maximum decoded size of any single stream (default 64 MB). */
#ifndef SCL_PDF_MAX_STREAM_SIZE
#define SCL_PDF_MAX_STREAM_SIZE ((size_t)64 * 1024 * 1024)
#endif
/* Maximum length of a string object. */
#ifndef SCL_PDF_MAX_STRING_LEN
#define SCL_PDF_MAX_STRING_LEN ((size_t)16 * 1024 * 1024)
#endif
/* Maximum entries in one array / dictionary. */
#ifndef SCL_PDF_MAX_CONTAINER_LEN
#define SCL_PDF_MAX_CONTAINER_LEN 65536
#endif
/* Maximum object/container nesting depth. */
#ifndef SCL_PDF_MAX_DEPTH
#define SCL_PDF_MAX_DEPTH 48
#endif
/* Maximum xref sections in a /Prev chain (incremental updates). */
#ifndef SCL_PDF_MAX_XREF_CHAIN
#define SCL_PDF_MAX_XREF_CHAIN 64
#endif
/* Maximum filters in one stream's filter cascade. */
#ifndef SCL_PDF_MAX_FILTERS
#define SCL_PDF_MAX_FILTERS 8
#endif
/* Maximum pages surfaced from the page tree. */
#ifndef SCL_PDF_MAX_PAGES
#define SCL_PDF_MAX_PAGES 65536
#endif
/* Maximum extracted text per page (bytes of UTF-8). */
#ifndef SCL_PDF_MAX_TEXT_LEN
#define SCL_PDF_MAX_TEXT_LEN ((size_t)4 * 1024 * 1024)
#endif

/* ── COS object model (§7.3) ────────────────────────────────── */

typedef enum {
  SCL_PDF_OBJ_NULL = 0,
  SCL_PDF_OBJ_BOOL,
  SCL_PDF_OBJ_INT,
  SCL_PDF_OBJ_REAL,
  SCL_PDF_OBJ_STRING, /* raw bytes, may be binary */
  SCL_PDF_OBJ_NAME,   /* #xx-decoded, NUL-terminated */
  SCL_PDF_OBJ_ARRAY,
  SCL_PDF_OBJ_DICT,
  SCL_PDF_OBJ_STREAM,
  SCL_PDF_OBJ_REF /* indirect reference "N G R" */
} scl_pdf_obj_type_t;

typedef struct scl_pdf_obj scl_pdf_obj_t;

struct scl_pdf_obj {
  scl_pdf_obj_type_t type;
  union {
    bool boolean;
    int64_t integer;
    double real;
    struct {
      char *data; /* NUL-terminated for convenience; len excludes NUL */
      size_t len;
    } string; /* used by STRING and NAME */
    struct {
      scl_pdf_obj_t **items;
      size_t count;
    } array;
    struct {
      char **keys; /* name strings without leading '/' */
      scl_pdf_obj_t **vals;
      size_t count;
    } dict;
    struct {
      scl_pdf_obj_t *dict;
      size_t data_off; /* offset of raw (encoded) data in file buffer */
      size_t data_len; /* raw length from /Length, clamped to buffer */
    } stream;
    struct {
      int num;
      int gen;
    } ref;
  } u;
};

/* ── Cross-reference entry (§7.5.4, §7.5.8) ─────────────────── */

typedef enum {
  SCL_PDF_XREF_FREE = 0,   /* type 0 / 'f' */
  SCL_PDF_XREF_OFFSET = 1, /* type 1 / 'n': byte offset of "N G obj" */
  SCL_PDF_XREF_IN_OBJSTM = 2 /* type 2: inside a compressed object stream */
} scl_pdf_xref_type_t;

typedef struct {
  uint64_t field2; /* OFFSET: byte offset. IN_OBJSTM: container obj num */
  uint32_t field3; /* OFFSET: generation. IN_OBJSTM: index in container */
  uint8_t type;    /* scl_pdf_xref_type_t */
} scl_parse_pdf_xref_entry_t;

/* ── Parser handle ──────────────────────────────────────────── */

typedef struct {
  scl_allocator_t *alloc;
  char *filename; /* NULL when opened from memory */
  unsigned char *buf;
  size_t buf_size;
  bool owns_buf;

  int version_major;
  int version_minor;

  scl_parse_pdf_xref_entry_t *xref; /* indexed by object number */
  scl_pdf_obj_t **obj_cache;        /* lazily resolved objects */
  size_t xref_size;                 /* entries in xref/obj_cache */

  scl_pdf_obj_t *trailer; /* merged trailer dictionary */
  int root_obj;
  int info_obj;
  int page_count;
  bool encrypted;
  bool repaired; /* xref was rebuilt by scanning */

  /* Flattened page tree: object numbers of leaf /Page objects. */
  int *pages;
  int pages_count;
} scl_parse_pdf_t;

/* ── Lifecycle ──────────────────────────────────────────────── */

/* Open and fully index a PDF file (header, xref chain, trailer, page tree).
 * Individual objects are parsed lazily on first access. */
scl_error_t scl_parse_pdf_open(scl_allocator_t *alloc, scl_parse_pdf_t *parser,
                               const char *filename);

/* Same, from an in-memory buffer. The buffer is copied. */
scl_error_t scl_parse_pdf_open_mem(scl_allocator_t *alloc,
                                   scl_parse_pdf_t *parser, const void *data,
                                   size_t len);

scl_error_t scl_parse_pdf_close(scl_parse_pdf_t *parser);

/* ── Document-level queries ─────────────────────────────────── */

scl_error_t scl_parse_pdf_get_page_count(scl_parse_pdf_t *parser, int *out);

/* Version from the %PDF-x.y header (or /Version in the catalog if newer). */
scl_error_t scl_parse_pdf_get_version(scl_parse_pdf_t *parser, int *major,
                                      int *minor);

/* True when the trailer carries /Encrypt (§7.6). Payloads stay encrypted. */
bool scl_parse_pdf_is_encrypted(const scl_parse_pdf_t *parser);

/* Document info dictionary value (§14.3.3): key is e.g. "Title", "Author".
 * The value is decoded per §7.9.2.2 (UTF-16BE or PDFDocEncoding) into
 * UTF-8. out/out_len follow the usual in/out contract. */
scl_error_t scl_parse_pdf_get_info(scl_parse_pdf_t *parser, const char *key,
                                   char *out, size_t *out_len);

/* ── Object-level access ────────────────────────────────────── */

/* Fetch indirect object `num` (generation checked when `gen` >= 0).
 * The returned object is owned by the parser; do not free. */
scl_error_t scl_parse_pdf_get_object(scl_parse_pdf_t *parser, int num, int gen,
                                     scl_pdf_obj_t **out);

/* Follow indirect references until a direct object (cycle-safe). */
scl_error_t scl_parse_pdf_resolve(scl_parse_pdf_t *parser,
                                  scl_pdf_obj_t *obj, scl_pdf_obj_t **out);

/* Dictionary lookup by key (no leading '/'); NULL when absent or not a
 * dict/stream. Does NOT resolve indirect values. */
scl_pdf_obj_t *scl_pdf_dict_get(const scl_pdf_obj_t *obj, const char *key);

/* Decode a stream object's data through its filter chain (§7.4).
 * Output allocated via the parser's allocator; caller frees with scl_free.
 * DCTDecode/JPXDecode/CCITTFaxDecode/JBIG2Decode return
 * SCL_ERR_UNSUPPORTED (image codecs, not document data). */
scl_error_t scl_parse_pdf_get_stream_data(scl_parse_pdf_t *parser,
                                          scl_pdf_obj_t *stream, void **out,
                                          size_t *out_len);

/* ── Pages and text ─────────────────────────────────────────── */

/* Page dictionary for page `index` (0-based, document order). */
scl_error_t scl_parse_pdf_get_page(scl_parse_pdf_t *parser, int index,
                                   scl_pdf_obj_t **out);

/* Extract text shown by Tj/TJ/'/" operators of page `index` into a
 * UTF-8 buffer allocated via the parser's allocator (caller frees).
 * Encrypted documents return SCL_ERR_UNSUPPORTED. */
scl_error_t scl_parse_pdf_extract_text(scl_parse_pdf_t *parser, int index,
                                       char **out, size_t *out_len);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
