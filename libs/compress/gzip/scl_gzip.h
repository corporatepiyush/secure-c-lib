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

/* Gzip (RFC 1952). LZ77 + Huffman coding (fixed/dynamic trees). CRC-32 checksum. Streaming compress/decompress. */

#ifndef SCL_GZIP_H
#define SCL_GZIP_H

/*
 * scl_gzip — Gzip compression/decompression (RFC 1952).
 *
 * ── What this provides ───────────────────────────────────────────────────────
 *
 * In-memory gzip compress/decompress using raw DEFLATE (RFC 1951) with a
 * gzip wrapper. The implementation:
 *
 *   • Compresses with LZ77 + fixed Huffman codes (good ratio, fast).
 *   • Falls back to stored blocks when data is incompressible.
 *   • Decompresses stored and fixed-Huffman blocks (the most common).
 *   • Computes CRC-32 via a table-driven algorithm.
 *   • Supports streaming via scl_gzip_decompress(), scl_gzip_compress().
 *
 * ── Security ─────────────────────────────────────────────────────────────────
 *
 *   • All internal buffers are bounds-checked before writes.
 *   • The decompressor validates Huffman code lengths against RFC 1951
 *     limits and rejects invalid bitstreams rather than continuing.
 *   • No recursion, no dynamic allocation in the hot path.
 *   • Allocations use the caller-provided scl_allocator_t.
 *
 * ── Limitations ──────────────────────────────────────────────────────────────
 *
 *   • Dynamic Huffman blocks (type 2) are decompressed but NOT emitted by the
 *     compressor — fixed Huffman is simpler and still provides good ratios.
 *   • No streaming API yet: compress/decompress operate on whole buffers.
 *   • No multi-member gzip: each call processes one gzip member.
 *
 * ── API philosophy ───────────────────────────────────────────────────────────
 *
 * The API mirrors scl_http_client — you pass a buffer in, get a buffer out
 * (allocated through your allocator). Call scl_gzip_free_result() to free
 * the output buffer.
 */

#include "scl_common.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* ── Compress ─────────────────────────────────────────────────────
 *
 * Compress `src` (len bytes) into a gzip-compressed buffer.
 * The returned buffer (in *out, length in *out_len) is allocated via
 * `alloc` and must be freed with scl_free() or scl_gzip_free_result().
 *
 * If `level` is 0, no compression is attempted (stored blocks only).
 * If level > 0, LZ77 + fixed Huffman is used.
 *
 * Returns SCL_OK on success, or an error code on allocation/compression
 * failure. */
scl_error_t scl_gzip_compress(scl_allocator_t *alloc,
                              const void *src, size_t src_len,
                              void **out, size_t *out_len,
                              int level);

/* ── Decompress ───────────────────────────────────────────────────
 *
 * Decompress a gzip-compressed buffer `src` (len bytes) back to the
 * original data. The returned buffer (in *out, length in *out_len) is
 * allocated via `alloc` and must be freed with scl_free() or
 * scl_gzip_free_result().
 *
 * Returns SCL_OK on success, or an error code on invalid data,
 * allocation failure, or unsupported features. */
scl_error_t scl_gzip_decompress(scl_allocator_t *alloc,
                                const void *src, size_t src_len,
                                void **out, size_t *out_len);

/* ── Free gzip result ─────────────────────────────────────────────
 *
 * Safely frees the output buffer from compress/decompress.
 * Does nothing if *out is NULL. Sets *out = NULL and *out_len = 0
 * after freeing. */
static inline void scl_gzip_free_result(scl_allocator_t *alloc,
                                        void **out, size_t *out_len) {
    if (out && *out) {
        scl_free(alloc, *out);
        *out = NULL;
    }
    if (out_len) *out_len = 0;
}

#endif /* SCL_GZIP_H */
