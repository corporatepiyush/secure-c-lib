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

/* DOCX (OOXML) text extractor. ZIP document.xml parse, shared-strings
 * resolution. Minimal DOM footprint. */

#ifndef SCL_PARSE_DOCX_H
#define SCL_PARSE_DOCX_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include "scl_common.h"

/* Maximum size of ZIP buffer to load (default 128 MB). */
#ifndef SCL_DOCX_MAX_ZIP_SIZE
#define SCL_DOCX_MAX_ZIP_SIZE ((size_t)128 * 1024 * 1024)
#endif
/* Maximum text length extracted (default 16 MB). */
#ifndef SCL_DOCX_MAX_TEXT_LEN
#define SCL_DOCX_MAX_TEXT_LEN ((size_t)16 * 1024 * 1024)
#endif

typedef struct {
   scl_allocator_t *alloc;
   char *filename;
   FILE *fp;
   unsigned char *zip_buf;
   size_t zip_size;
   char *text_buf;
   size_t text_len;
   size_t text_cap;
   size_t max_text_len; /* configurable cap, defaults to SCL_DOCX_MAX_TEXT_LEN */
} scl_parse_docx_t;

scl_error_t scl_parse_docx_open(scl_allocator_t *alloc,
                                 scl_parse_docx_t *parser, const char *filename);
scl_error_t scl_parse_docx_get_text(scl_parse_docx_t *parser, const char **out,
                                     size_t *out_len);
scl_error_t scl_parse_docx_close(scl_parse_docx_t *parser);

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif