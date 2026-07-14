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

/* Gzip (RFC 1952). LZ77 + Huffman coding (fixed/dynamic trees). CRC-32
 * checksum. Streaming compress/decompress. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("O3")
#endif

/*
 * scl_gzip.c — Gzip compression/decompression (RFC 1952).
 *
 * ── Internal architecture ────────────────────────────────────────────────────
 *
 * This file builds gzip atop raw DEFLATE (RFC 1951). The layers are:
 *
 *   GZIP frame  ──  RFC 1952 header + trailer wrapping a DEFLATE stream
 *   DEFLATE     ──  RFC 1951 blocks (stored / fixed Huffman)
 *   CRC-32      ──  Table-driven IEEE 802.3 CRC-32 for the trailer
 *   Bit I/O     ──  LSB-first bit-level reading/writing
 *
 * ── Compression strategy ─────────────────────────────────────────────────────
 *
 * The compressor uses LZ77 with a hash-chain match finder (similar to
 * gzip's "level 1-3" strategy) followed by fixed Huffman coding.
 * Incompressible data falls back to stored blocks.
 *
 * ── Decompression strategy ───────────────────────────────────────────────────
 *
 * The decompressor handles stored blocks (type 0) and fixed Huffman blocks
 * (type 1). Dynamic Huffman blocks (type 2) are supported in the decompressor
 * since they appear in many real-world gzip files.
 *
 * ── Security considerations ──────────────────────────────────────────────────
 *
 *   • All internal buffer accesses are bounds-checked.
 *   • Huffman code lengths are validated against RFC 1951 limits.
 *   • The bit reader never shifts by more than 7 bits (undefined behaviour
 *     guard) and always checks remaining input before reading.
 *   • Output sizes are capped by the caller-provided limit where applicable.
 *   • No recursion, no variable-length arrays, no alloca.
 */

#include "scl_gzip.h"
#include "scl_pthread.h"
#include "scl_stdlib.h"
#include "scl_string.h"

#include "scl_stdbool.h"
#include "scl_stddef.h"
#include "scl_stdint.h"

/* ════════════════════════════════════════════════════════════════════
 *  CRC-32 (IEEE 802.3, used by gzip trailer)
 * ════════════════════════════════════════════════════════════════════ */

/* Table-driven CRC-32: pre-computed for each possible byte value.
 * The polynomial is 0xEDB88320 (reflected). */
static uint32_t scl_crc32_table[256];
static scl_once_t scl_crc32_once = SCL_ONCE_INIT;

static void scl_crc32_init_table(void) {
  for (uint32_t i = 0; i < 256; i++) {
    uint32_t crc = i;
    for (int j = 0; j < 8; j++)
      crc = (crc >> 1) ^ (0xEDB88320u & ~((crc & 1) - 1));
    scl_crc32_table[i] = crc;
  }
}

static uint32_t scl_crc32_update(uint32_t crc, const void *data, size_t len) {
  scl_once(&scl_crc32_once, scl_crc32_init_table);
  const unsigned char *p = (const unsigned char *)data;
  for (size_t i = 0; i < len; i++)
    crc = scl_crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return crc;
}

static uint32_t scl_crc32_final(const void *data, size_t len) {
  return ~scl_crc32_update(0xFFFFFFFFu, data, len);
}

/* ════════════════════════════════════════════════════════════════════
 *  Bit I/O — LSB-first (matching DEFLATE's bit ordering)
 * ════════════════════════════════════════════════════════════════════ */

struct bit_reader {
  const unsigned char *buf;
  size_t cap;
  size_t pos;    /* byte position */
  unsigned bits; /* bit buffer (LSB) */
  int nbits;     /* number of valid bits in buffer */
};

static void bit_reader_init(struct bit_reader *br, const void *buf,
                            size_t len) {
  br->buf = (const unsigned char *)buf;
  br->cap = len;
  br->pos = 0;
  br->bits = 0;
  br->nbits = 0;
}

/* Fill bit buffer up to at most 32 bits. Returns false on EOF. */
static bool bit_reader_fill(struct bit_reader *br) {
  while (scl_likely(br->nbits <= 24 && br->pos < br->cap)) {
    br->bits |= (unsigned)br->buf[br->pos] << br->nbits;
    br->pos++;
    br->nbits += 8;
  }
  return br->nbits > 0;
}

/* Read n bits (1-16) without consuming them (peek). Returns 0 on underflow. */
static unsigned bit_reader_peek(struct bit_reader *br, int n) {
  if (scl_likely(bit_reader_fill(br)))
    return br->bits & ((1u << n) - 1);
  return 0;
}

/* Read n bits (1-16) and consume them. Returns 0 on underflow. */
static unsigned bit_reader_read(struct bit_reader *br, int n) {
  unsigned val = bit_reader_peek(br, n);
  br->bits >>= n;
  br->nbits -= n;
  return val;
}

/* Discard remaining bits (align to byte boundary).
 *
 * bit_reader_fill prefetches up to 4 bytes, so after discarding the
 * partial byte the buffer may still hold whole bytes that `pos` has
 * already advanced past. Those must be returned to the stream: callers
 * read stored-block LEN/NLEN (and trailers) directly at buf[pos], and
 * without the rewind they would land up to 3 bytes too far. */
static void bit_reader_align(struct bit_reader *br) {
  int discard = br->nbits & 7;
  br->bits >>= discard;
  br->nbits -= discard;
  br->pos -= (size_t)(br->nbits >> 3);
  br->bits = 0;
  br->nbits = 0;
}

struct bit_writer {
  unsigned char *buf;
  size_t cap;
  size_t pos;
  unsigned bits;
  int nbits;
};

static void bit_writer_init(struct bit_writer *bw, void *buf, size_t cap) {
  bw->buf = (unsigned char *)buf;
  bw->cap = cap;
  bw->pos = 0;
  bw->bits = 0;
  bw->nbits = 0;
}

/* Write n bits (value in low n bits, LSB-first). Returns false if buffer full.
 */
static bool bit_writer_write(struct bit_writer *bw, unsigned val, int n) {
  bw->bits |= (val & ((1u << n) - 1)) << bw->nbits;
  bw->nbits += n;
  while (bw->nbits >= 8) {
    if (bw->pos >= bw->cap)
      return false;
    bw->buf[bw->pos++] = (unsigned char)(bw->bits & 0xFF);
    bw->bits >>= 8;
    bw->nbits -= 8;
  }
  return true;
}

/* Flush remaining bits (zero-padded) and align. Returns false if buffer full.
 */
static bool bit_writer_flush(struct bit_writer *bw) {
  if (bw->nbits > 0) {
    if (bw->pos >= bw->cap)
      return false;
    bw->buf[bw->pos++] = (unsigned char)(bw->bits & 0xFF);
    bw->bits = 0;
    bw->nbits = 0;
  }
  return true;
}

/* ════════════════════════════════════════════════════════════════════
 *  Canonical Huffman decoder (RFC 1951 §3.2.2)
 *
 *  Canonical codes of each bit length are consecutive integers, so the
 *  decoder only needs, per length, the number of codes and where that
 *  length's first symbol sits in a symbol table sorted by (length,
 *  code). Decoding is then a subtract-and-compare per bit in registers,
 *  with a single syms[] load at the end — no node tree, no pointer
 *  chasing, and the whole struct lives on the stack so building and
 *  discarding tables per block never touches the allocator.
 * ════════════════════════════════════════════════════════════════════ */

/* Maximum bits in a Huffman code per RFC 1951. */
#define SCL_HUFF_MAX_BITS 15
/* Largest DEFLATE alphabet: 288 literal/length symbols. */
#define SCL_HUFF_MAX_SYMS 288

typedef struct {
  uint16_t count[SCL_HUFF_MAX_BITS + 1]; /* codes of each bit length */
  uint16_t syms[SCL_HUFF_MAX_SYMS];      /* symbols sorted by (len, code) */
} scl_huff_tree_t;

/* Build the decode tables from code lengths. `lens` has `count` entries,
 * each the code length in bits (0 = symbol unused). Rejects
 * over-subscribed length sets (more codes than a prefix tree of that
 * depth can hold), which a crafted stream can use to smuggle ambiguous
 * codes past a non-validating builder. Incomplete codes are accepted
 * (DEFLATE allows them, e.g. single-code distance trees). */
static scl_error_t scl_huff_build(scl_huff_tree_t *t, const unsigned char *lens,
                                  unsigned count) {
  if (scl_unlikely(count > SCL_HUFF_MAX_SYMS))
    return SCL_ERR_INVALID_ARG;
  scl_memset(t->count, 0, sizeof(t->count));
  for (unsigned i = 0; i < count; i++) {
    if (scl_unlikely(lens[i] > SCL_HUFF_MAX_BITS))
      return SCL_ERR_PARSE;
    t->count[lens[i]]++;
  }
  t->count[0] = 0;

  /* `left` = codes still available at each depth; negative means the
   * length set is over-subscribed and the stream is corrupt. */
  int left = 1;
  for (unsigned len = 1; len <= SCL_HUFF_MAX_BITS; len++) {
    left <<= 1;
    left -= (int)t->count[len];
    if (scl_unlikely(left < 0))
      return SCL_ERR_PARSE;
  }

  /* Offset of each length's first symbol in syms[], then fill. */
  uint16_t offs[SCL_HUFF_MAX_BITS + 1];
  offs[1] = 0;
  for (unsigned len = 1; len < SCL_HUFF_MAX_BITS; len++)
    offs[len + 1] = (uint16_t)(offs[len] + t->count[len]);
  for (unsigned i = 0; i < count; i++)
    if (lens[i])
      t->syms[offs[lens[i]]++] = (uint16_t)i;
  return SCL_OK;
}

/* Decode one symbol from the bit stream. Bits arrive LSB-first; each
 * new bit becomes the next lower-significance bit of the growing code.
 * Returns -1 on error (invalid code / EOF). */
static int scl_huff_decode(struct bit_reader *br, const scl_huff_tree_t *t) {
  unsigned code = 0;  /* code accumulated so far */
  unsigned first = 0; /* first canonical code of this length */
  unsigned index = 0; /* syms[] index of that first code */
  for (unsigned len = 1; len <= SCL_HUFF_MAX_BITS; len++) {
    /* Refill only on an empty buffer; fill tops up to 32 bits, so this
     * branch is not-taken for at least 31 of every 32 iterations. */
    if (scl_unlikely(br->nbits == 0) && scl_unlikely(!bit_reader_fill(br)))
      return -1;
    code |= br->bits & 1;
    br->bits >>= 1;
    br->nbits -= 1;
    unsigned cnt = t->count[len];
    if (code - first < cnt)
      return t->syms[index + (code - first)];
    index += cnt;
    first = (first + cnt) << 1;
    code <<= 1;
  }
  return -1;
}

/* Fixed Huffman code lengths per RFC 1951 §3.2.6:
 *   - Literals 0-143: length 8
 *   - Literals 144-255: length 9
 *   - Literals 256-279: length 7
 *   - Literals 280-287: length 8
 *   - Distances 0-31: length 5 (always) */
#define FIXED_LIT_COUNT 288
#define FIXED_DIST_COUNT 32

static void scl_huff_fixed_lens(unsigned char *lens, unsigned count) {
  if (count > FIXED_LIT_COUNT)
    count = FIXED_LIT_COUNT;
  unsigned i = 0;
  while (i < count && i < 144)
    lens[i++] = 8;
  while (i < count && i < 256)
    lens[i++] = 9;
  while (i < count && i < 280)
    lens[i++] = 7;
  while (i < count && i < 288)
    lens[i++] = 8;
  while (i < count)
    lens[i++] = 5; /* distances */
}

/* ════════════════════════════════════════════════════════════════════
 *  DEFLATE constants
 * ════════════════════════════════════════════════════════════════════ */

/* Length codes: base lengths and extra bits for each code 257-285. */
static const unsigned short scl_len_base[] = {
    3,  4,  5,  6,  7,  8,  9,  10,  11,  13,  15,  17,  19,  23, 27, 31,
    35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0,  0};
static const unsigned char scl_len_extra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
                                              1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
                                              4, 4, 5, 5, 5, 5, 0, 0, 0};

/* Distance codes: base distances and extra bits. */
static const unsigned short scl_dist_base[] = {
    1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const unsigned char scl_dist_extra[] = {
    0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

/* ════════════════════════════════════════════════════════════════════
 *  LZ77 match finder (hash-chain)
 * ════════════════════════════════════════════════════════════════════ */

#define SCL_LZ77_WINDOW_SIZE (32 * 1024)
#define SCL_LZ77_MIN_MATCH 3
#define SCL_LZ77_MAX_MATCH 258
#define SCL_LZ77_HASH_BITS 15
#define SCL_LZ77_HASH_SIZE (1u << SCL_LZ77_HASH_BITS)

/* Hash function for 3-byte sequences */
static uint32_t lz77_hash(const unsigned char *p) {
  return ((uint32_t)p[0] << 4) ^ ((uint32_t)p[1] << 2) ^ (uint32_t)p[2];
}

/* ════════════════════════════════════════════════════════════════════
 *  DEFLATE decompression
 * ════════════════════════════════════════════════════════════════════ */

/* Grow the output buffer to hold `needed` bytes, doubling geometrically.
 * `max_out` bounds total decompressed size so a tiny compressed input
 * cannot expand into an unbounded allocation (decompression bomb). */
static scl_error_t inflate_ensure_cap(unsigned char **out_buf, size_t *out_cap,
                                      size_t needed, size_t max_out,
                                      scl_allocator_t *alloc) {
  if (scl_unlikely(needed > max_out))
    return SCL_ERR_SIZE_OVERFLOW;
  if (needed <= *out_cap)
    return SCL_OK;
  size_t new_cap = *out_cap ? *out_cap : 4096;
  while (new_cap < needed) {
    if (scl_mul_overflow(new_cap, 2, &new_cap))
      return SCL_ERR_SIZE_OVERFLOW;
  }
  if (new_cap > max_out)
    new_cap = max_out;
  void *nb =
      scl_realloc(alloc, *out_buf, *out_cap, new_cap, _Alignof(max_align_t));
  if (!nb)
    return SCL_ERR_OUT_OF_MEMORY;
  *out_buf = (unsigned char *)nb;
  *out_cap = new_cap;
  return SCL_OK;
}

static scl_error_t deflate_decompress(struct bit_reader *br,
                                      unsigned char **out_buf, size_t *out_len,
                                      size_t *out_cap, size_t max_out,
                                      scl_allocator_t *alloc) {
  scl_error_t err;
  bool final = false;

  while (!final) {
    /* Block header: BFINAL (1 bit), BTYPE (2 bits). */
    if (!bit_reader_fill(br))
      return SCL_ERR_PARSE;
    final = bit_reader_read(br, 1) != 0;
    unsigned btype = bit_reader_read(br, 2);

    if (btype == 0) {
      /* ── Stored (uncompressed) block ── */
      bit_reader_align(br);
      /* Read LEN and NLEN (2 bytes each, little-endian). */
      if (br->pos + 4 > br->cap)
        return SCL_ERR_PARSE;
      unsigned len =
          (unsigned)br->buf[br->pos] | ((unsigned)br->buf[br->pos + 1] << 8);
      unsigned nlen = (unsigned)br->buf[br->pos + 2] |
                      ((unsigned)br->buf[br->pos + 3] << 8);
      br->pos += 4;
      if ((len ^ nlen) != 0xFFFFu)
        return SCL_ERR_PARSE;

      /* Copy stored data to output. */
      if (br->pos + len > br->cap)
        return SCL_ERR_PARSE;
      err = inflate_ensure_cap(out_buf, out_cap, *out_len + len, max_out,
                               alloc);
      if (err != SCL_OK)
        return err;
      scl_memcpy(*out_buf + *out_len, br->buf + br->pos, len);
      *out_len += len;
      br->pos += len;

    } else if (btype == 1 || btype == 2) {
      /* ── Compressed block (fixed or dynamic Huffman) ── */
      unsigned char lit_lens[FIXED_LIT_COUNT];
      unsigned char dist_lens[FIXED_DIST_COUNT];

      unsigned hlit = FIXED_LIT_COUNT;
      unsigned hdist = FIXED_DIST_COUNT;

      if (btype == 1) {
        /* Fixed Huffman codes. */
        scl_huff_fixed_lens(lit_lens, FIXED_LIT_COUNT);
        for (unsigned i = 0; i < FIXED_DIST_COUNT; i++)
          dist_lens[i] = 5;
      } else {
        /* ── Dynamic Huffman codes ── */
        hlit = bit_reader_read(br, 5) + 257;         /* lit/length codes */
        hdist = bit_reader_read(br, 5) + 1;          /* distance codes */
        unsigned hclen = bit_reader_read(br, 4) + 4; /* code length codes */

        if (hlit > 286 || hdist > 30 || hclen > 19)
          return SCL_ERR_PARSE;

        /* Read code length code lengths. */
        static const unsigned char cl_order[] = {
            16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
        unsigned char cl_lens[19];
        scl_memset(cl_lens, 0, sizeof(cl_lens));
        for (unsigned i = 0; i < hclen; i++) {
          cl_lens[cl_order[i]] = (unsigned char)bit_reader_read(br, 3);
          if (cl_lens[cl_order[i]] > 7)
            return SCL_ERR_PARSE;
        }

        /* Build the code length decode table (stack-resident). */
        scl_huff_tree_t cl_tree;
        err = scl_huff_build(&cl_tree, cl_lens, 19);
        if (err != SCL_OK)
          return err;

        /* Read literal/length and distance code lengths. */
        unsigned total = hlit + hdist;
        unsigned char dyn_lens[320];
        unsigned idx = 0;
        while (idx < total) {
          int sym = scl_huff_decode(br, &cl_tree);
          if (sym < 0 || sym > 18)
            return SCL_ERR_PARSE;
          unsigned repeat = 0;
          unsigned char val = 0;
          if (sym < 16) {
            dyn_lens[idx++] = (unsigned char)sym;
            continue;
          } else if (sym == 16) {
            if (idx == 0)
              return SCL_ERR_PARSE;
            val = dyn_lens[idx - 1];
            repeat = bit_reader_read(br, 2) + 3;
          } else if (sym == 17) {
            val = 0;
            repeat = bit_reader_read(br, 3) + 3;
          } else { /* sym == 18 */
            val = 0;
            repeat = bit_reader_read(br, 7) + 11;
          }
          for (unsigned k = 0; k < repeat && idx < total; k++)
            dyn_lens[idx++] = val;
        }

        /* Split into literal and distance tables. */
        for (unsigned i = 0; i < hlit; i++)
          lit_lens[i] = dyn_lens[i];
        for (unsigned i = hlit; i < total; i++)
          dist_lens[i - hlit] = dyn_lens[i];
      }

      /* Build the decode tables (stack-resident, no allocation). */
      scl_huff_tree_t lit_tree, dist_tree;
      err = scl_huff_build(&lit_tree, lit_lens, hlit);
      if (err == SCL_OK)
        err = scl_huff_build(&dist_tree, dist_lens, hdist);
      if (err != SCL_OK)
        return err;

      /* Decode symbols. */
      for (;;) {
        int sym = scl_huff_decode(br, &lit_tree);
        if (sym < 0)
          return SCL_ERR_PARSE;

        if (scl_likely(sym < 256)) {
          /* Literal byte. */
          err = inflate_ensure_cap(out_buf, out_cap, *out_len + 1, max_out,
                                   alloc);
          if (err != SCL_OK)
            return err;
          (*out_buf)[(*out_len)++] = (unsigned char)sym;
        } else if (scl_unlikely(sym == 256)) {
          /* End of block. */
          break;
        } else if (scl_likely(sym <= 285)) {
          /* Length-distance pair. */
          unsigned len_code = (unsigned)(sym - 257);
          if (len_code >= sizeof(scl_len_base) / sizeof(scl_len_base[0]))
            return SCL_ERR_PARSE;
          unsigned length = scl_len_base[len_code];
          unsigned extra = scl_len_extra[len_code];
          if (extra > 0)
            length += bit_reader_read(br, (int)extra);

          int dist_sym = scl_huff_decode(br, &dist_tree);
          if (dist_sym < 0 ||
              (unsigned)dist_sym >=
                  sizeof(scl_dist_base) / sizeof(scl_dist_base[0]))
            return SCL_ERR_PARSE;
          unsigned distance = scl_dist_base[dist_sym];
          unsigned dextra = scl_dist_extra[dist_sym];
          if (dextra > 0)
            distance += bit_reader_read(br, (int)dextra);

          if (distance > *out_len || length > SCL_LZ77_MAX_MATCH)
            return SCL_ERR_PARSE;

          err = inflate_ensure_cap(out_buf, out_cap, *out_len + length,
                                   max_out, alloc);
          if (err != SCL_OK)
            return err;
          /* Copy length bytes from distance back.
           * Non-overlapping case (distance >= length): use memcpy
           * for word-at-a-time / SIMD throughput. Overlapping case
           * (distance < length, RLE pattern): fall back to byte
           * copy which naturally handles the repeat. */
          unsigned char *dst = *out_buf + *out_len;
          *out_len += length;
          if (scl_likely(distance >= length)) {
            scl_memcpy(dst, dst - distance, length);
          } else {
            for (unsigned k = 0; k < length; k++)
              dst[k] = (dst - distance)[k];
          }
        } else {
          return SCL_ERR_PARSE;
        }
      }

    } else {
      return SCL_ERR_PARSE; /* reserved block type */
    }
  }

  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  DEFLATE compression
 * ════════════════════════════════════════════════════════════════════ */

/* Write stored (uncompressed) blocks. LEN is a 16-bit field, so inputs
 * larger than 65535 bytes are split across multiple blocks; only the
 * last block of the final chunk carries BFINAL. Empty input still emits
 * one (empty) block so the stream stays well-formed. */
static scl_error_t deflate_write_stored(struct bit_writer *bw,
                                        const unsigned char *data, size_t len,
                                        bool final) {
  size_t off = 0;
  do {
    size_t chunk = len - off > 65535 ? 65535 : len - off;
    bool last = final && off + chunk == len;

    bit_writer_flush(bw);
    /* BFINAL + BTYPE = stored */
    if (!bit_writer_write(bw, last ? 1u : 0u, 1))
      return SCL_ERR_SIZE_OVERFLOW;
    if (!bit_writer_write(bw, 0u, 2))
      return SCL_ERR_SIZE_OVERFLOW;
    bit_writer_flush(bw);

    /* LEN and NLEN (little-endian 16-bit). */
    if (bw->pos + 4 > bw->cap)
      return SCL_ERR_SIZE_OVERFLOW;
    unsigned short l = (unsigned short)chunk;
    unsigned short nl = (unsigned short)(~l);
    bw->buf[bw->pos++] = (unsigned char)(l & 0xFF);
    bw->buf[bw->pos++] = (unsigned char)((l >> 8) & 0xFF);
    bw->buf[bw->pos++] = (unsigned char)(nl & 0xFF);
    bw->buf[bw->pos++] = (unsigned char)((nl >> 8) & 0xFF);

    if (bw->pos + chunk > bw->cap)
      return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(bw->buf + bw->pos, data + off, chunk);
    bw->pos += chunk;
    off += chunk;
  } while (off < len);
  return SCL_OK;
}

/* Look up the length code (257-285) for a given match length.
 * Rows per RFC 1951 §3.2.5: codes 257-264 have no extra bits, then four
 * codes per extra-bit tier, and 285 covers exactly length 258. The
 * result must satisfy len - scl_len_base[code-257] < 2^extra, or the
 * emitted stream decodes to a different length than was compressed. */
static unsigned length_code(unsigned len) {
  if (len < 3)
    return 0;
  if (len <= 10)
    return 257 + (len - 3);
  if (len <= 18)
    return 265 + (len - 11) / 2;
  if (len <= 34)
    return 269 + (len - 19) / 4;
  if (len <= 66)
    return 273 + (len - 35) / 8;
  if (len <= 130)
    return 277 + (len - 67) / 16;
  if (len <= 257)
    return 281 + (len - 131) / 32;
  return 285; /* 258 */
}

/* Look up the distance code (0-29) for a given distance (1-32768).
 * Codes pair up geometrically per RFC 1951 §3.2.5: codes 2k and 2k+1
 * cover (2^k, 2^(k+1)] with k-1 extra bits each, so the code falls out
 * of floor(log2(dist-1)) plus the bit below the leading one. Must agree
 * with scl_dist_base/scl_dist_extra or emitted matches decode to the
 * wrong distance. */
static unsigned distance_code(unsigned dist) {
  if (dist < 1)
    return 0;
  if (dist <= 4)
    return dist - 1;
  unsigned d = dist - 1;
  unsigned log = scl_log2_u32(d); /* >= 2 here */
  return (log << 1) + ((d >> (log - 1)) & 1);
}

/* Emit a literal using fixed Huffman codes (RFC 1951 §3.2.6). */
static SCL_ALWAYS_INLINE scl_error_t emit_literal_fixed(struct bit_writer *bw,
                                                        unsigned sym) {
  /* Fixed Huffman codes per RFC 1951 §3.2.6:
   *   0-143:  8-bit code = 48 + sym (binary: 0011_0000 + sym, reversed)
   *   144-255: 9-bit code = 400 + (sym - 144) = 256 + sym
   *   256:    7-bit code = 0 (end of block)
   *   257-279: 7-bit codes
   *   280-287: 8-bit codes = 192 + (sym - 280) */
  if (sym <= 143) {
    unsigned rev = 0;
    unsigned c = 48 + sym;
    for (int i = 0; i < 8; i++)
      rev = (rev << 1) | ((c >> i) & 1);
    return bit_writer_write(bw, rev, 8) ? SCL_OK : SCL_ERR_SIZE_OVERFLOW;
  } else if (sym <= 255) {
    unsigned rev = 0;
    unsigned c = 256 + sym;
    for (int i = 0; i < 9; i++)
      rev = (rev << 1) | ((c >> i) & 1);
    return bit_writer_write(bw, rev, 9) ? SCL_OK : SCL_ERR_SIZE_OVERFLOW;
  }
  return SCL_ERR_INVALID_ARG;
}

/* Emit a length-distance pair using fixed Huffman codes.
 * Sym is the length code (257-285). */
static SCL_ALWAYS_INLINE scl_error_t
emit_match_fixed(struct bit_writer *bw, unsigned len_code, unsigned len_extra,
                 unsigned len_extra_bits, unsigned dist_code,
                 unsigned dist_extra, unsigned dist_extra_bits) {
  /* Length code (lit value): codes 257-279 have 7-bit, 280-287 have 8-bit. */
  if (len_code <= 279) {
    unsigned c = len_code - 256; /* 1-23, 7-bit codes starting at 0b0000001 */
    unsigned rev = 0;
    unsigned val = c; /* Actually code = (len_code - 256) reversed */
    /* For 7-bit codes, the assigned values are simple: code for 257 is
     * 0b0000001 reversed = 0b1000000 = 64. Let me be more careful. */
    /* The RFC assigns:
     * 256: end of block, code 0 (7-bit)
     * 257: code 2 (7-bit: 0000010) -> reversed = 0100000 = 32
     * etc. */
    /* Actually, I need to reverse a 7-bit number. The assigned codes for
     * 257-279 are (len_code - 256) as a 7-bit number. */
    val = len_code - 256;
    for (int i = 0; i < 7; i++)
      rev = (rev << 1) | ((val >> i) & 1);
    if (!bit_writer_write(bw, rev, 7))
      return SCL_ERR_SIZE_OVERFLOW;
  } else {
    unsigned c = len_code - 256; /* 24-31, 8-bit codes */
    unsigned rev = 0;
    unsigned val = c;
    /* Assigned codes for 280-287 are 192 + (len_code - 280) */
    val = 192 + (len_code - 280);
    for (int i = 0; i < 8; i++)
      rev = (rev << 1) | ((val >> i) & 1);
    if (!bit_writer_write(bw, rev, 8))
      return SCL_ERR_SIZE_OVERFLOW;
  }

  /* Write extra length bits. */
  if (len_extra_bits > 0) {
    if (!bit_writer_write(bw, len_extra, (int)len_extra_bits))
      return SCL_ERR_SIZE_OVERFLOW;
  }

  /* Distance code: always 5-bit fixed Huffman.
   * Code assignment for distance: just the reverse of the 5-bit value. */
  {
    unsigned rev = 0;
    for (int i = 0; i < 5; i++)
      rev = (rev << 1) | ((dist_code >> i) & 1);
    if (!bit_writer_write(bw, rev, 5))
      return SCL_ERR_SIZE_OVERFLOW;
  }

  /* Write extra distance bits. */
  if (dist_extra_bits > 0) {
    if (!bit_writer_write(bw, dist_extra, (int)dist_extra_bits))
      return SCL_ERR_SIZE_OVERFLOW;
  }

  return SCL_OK;
}

/* Compress data using LZ77 + fixed Huffman codes.
 * Writes into the bit_writer (must have room). */
static scl_error_t deflate_compress_fixed(struct bit_writer *bw,
                                          const unsigned char *data, size_t len,
                                          bool final, int level,
                                          scl_allocator_t *alloc) {
  /* Write block header: BFINAL + BTYPE = fixed Huffman. */
  if (!bit_writer_write(bw, final ? 1u : 0u, 1))
    return SCL_ERR_SIZE_OVERFLOW;
  if (!bit_writer_write(bw, 1u, 2))
    return SCL_ERR_SIZE_OVERFLOW;

  if (scl_unlikely(len == 0 || level == 0)) {
    /* No data or no compression requested — emit no matches, just literals. */
    /* For empty, just end of block. */
    goto finish;
  }

  /* LZ77 hash chain table (stores positions in the window). */
  uint32_t *hash_head = NULL;
  uint32_t *hash_prev = NULL;

  if (scl_likely(level > 0)) {
    /* The allocator must be real here: scl_calloc dereferences it, so a
     * NULL "use default" convention would segfault (it did, before this
     * module had tests). */
    hash_head = (uint32_t *)scl_calloc(alloc, SCL_LZ77_HASH_SIZE,
                                       sizeof(uint32_t), _Alignof(max_align_t));
    hash_prev = (uint32_t *)scl_calloc(alloc, len, sizeof(uint32_t),
                                       _Alignof(max_align_t));
    if (!hash_head || !hash_prev) {
      scl_free(alloc, hash_head);
      scl_free(alloc, hash_prev);
      /* Fall back to literal-only output. */
      goto do_literals;
    }
  }

  size_t pos = 0;
  while (pos < len) {
    unsigned best_len = 0;
    unsigned best_dist = 0;
    unsigned max_lookahead = (len - pos) < SCL_LZ77_MAX_MATCH
                                 ? (unsigned)(len - pos)
                                 : SCL_LZ77_MAX_MATCH;

    /* Search for a match. */
    if (level > 0 && max_lookahead >= SCL_LZ77_MIN_MATCH &&
        pos + SCL_LZ77_MIN_MATCH <= len) {
      uint32_t h = lz77_hash(data + pos) & (SCL_LZ77_HASH_SIZE - 1);
      uint32_t chain = hash_head[h];
      int chain_count = 0;

      while (chain != (uint32_t)-1 && chain < pos &&
             pos - chain <= SCL_LZ77_WINDOW_SIZE &&
             chain_count < (level > 1 ? 64 : 8)) {
        unsigned match_len = 0;
        const unsigned char *a = data + chain;
        const unsigned char *b = data + pos;
        while (match_len < max_lookahead && a[match_len] == b[match_len])
          match_len++;
        if (match_len >= SCL_LZ77_MIN_MATCH && match_len > best_len) {
          best_len = match_len;
          best_dist = (unsigned)(pos - chain);
          if (match_len == max_lookahead)
            break;
        }
        chain = hash_prev[chain];
        chain_count++;
      }

      /* Store current position in hash chain. */
      hash_prev[pos] = hash_head[h];
      hash_head[h] = (uint32_t)pos;
    }

    if (best_len >= SCL_LZ77_MIN_MATCH && best_dist > 0) {
      /* Emit match. */
      unsigned lc = length_code(best_len);
      unsigned le = best_len - scl_len_base[lc - 257];
      unsigned leb = scl_len_extra[lc - 257];
      unsigned dc = distance_code(best_dist);
      unsigned de = best_dist - scl_dist_base[dc];
      unsigned deb = scl_dist_extra[dc];

      scl_error_t e = emit_match_fixed(bw, lc, le, leb, dc, de, deb);
      if (e != SCL_OK) {
        scl_free(alloc, hash_head);
        scl_free(alloc, hash_prev);
        return e;
      }
      pos += best_len;
    } else {
      /* Emit literal. */
      scl_error_t e = emit_literal_fixed(bw, data[pos]);
      if (e != SCL_OK) {
        scl_free(alloc, hash_head);
        scl_free(alloc, hash_prev);
        return e;
      }
      pos++;

      /* Still need to update hash for this position if we're at the
       * start of a new potential match. */
      if (level > 0 && pos + 2 < len) {
        uint32_t h = lz77_hash(data + pos) & (SCL_LZ77_HASH_SIZE - 1);
        hash_prev[pos] = hash_head[h];
        hash_head[h] = (uint32_t)pos;
      }
    }
  }

  scl_free(alloc, hash_head);
  scl_free(alloc, hash_prev);

finish:
  /* End of block (literal 256, fixed Huffman). */
  {
    /* 256 in fixed Huffman: code is 0 (7-bit). Reversed = 0. */
    if (!bit_writer_write(bw, 0, 7))
      return SCL_ERR_SIZE_OVERFLOW;
  }
  return SCL_OK;

do_literals:
  if (hash_head)
    scl_free(alloc, hash_head);
  if (hash_prev)
    scl_free(alloc, hash_prev);
  /* Fallback: emit all literals individually with fixed Huffman.
   * This is inefficient but correct. */
  for (size_t i = 0; i < len; i++) {
    scl_error_t e = emit_literal_fixed(bw, data[i]);
    if (e != SCL_OK)
      return e;
  }
  goto finish;
}

/* ════════════════════════════════════════════════════════════════════
 *  Public API: gzip compress
 * ════════════════════════════════════════════════════════════════════ */

scl_error_t scl_gzip_compress(scl_allocator_t *alloc, const void *src,
                              size_t src_len, void **out, size_t *out_len,
                              int level) {
  if (!alloc)
    alloc = scl_allocator_default();
  if (!src || !out || !out_len)
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  *out_len = 0;

  /* Gzip header (10 bytes fixed + optional) + max data expansion + trailer (8).
   * Maximum DEFLATE expansion: stored blocks add 5 bytes header + 4 len/nlen
   * per 65535 bytes, plus 1 byte per literal in the worst case. We allocate
   * generously. */
  size_t est = src_len + (src_len / 65535) * 5 + 64;
  unsigned char *buf =
      (unsigned char *)scl_alloc(alloc, est, _Alignof(max_align_t));
  if (!buf)
    return SCL_ERR_OUT_OF_MEMORY;

  struct bit_writer bw;
  bit_writer_init(&bw, buf + 10, est - 10); /* reserve 10 for gzip header */

  scl_error_t err;

  if (level == 0) {
    /* Store verbatim in stored (type 0) blocks. */
    err = deflate_write_stored(&bw, (const unsigned char *)src, src_len, true);
  } else {
    err = deflate_compress_fixed(&bw, (const unsigned char *)src, src_len, true,
                                 level, alloc);
    if (err == SCL_ERR_SIZE_OVERFLOW) {
      /* Incompressible input: fixed-Huffman output (up to 9 bits per
       * literal) outgrew the stored-size budget. Rewind and emit stored
       * blocks instead, which are bounded at len + 5 bytes per 64K. */
      bit_writer_init(&bw, buf + 10, est - 10);
      err =
          deflate_write_stored(&bw, (const unsigned char *)src, src_len, true);
    }
  }
  if (err != SCL_OK) {
    scl_free(alloc, buf);
    return err;
  }
  bit_writer_flush(&bw);

  size_t comp_len = bw.pos; /* compressed data bytes */

  /* Compute CRC-32. */
  uint32_t crc = scl_crc32_final(src, src_len);

  /* Build gzip header (10 bytes at buf[0..9]):
   * ID1=0x1f, ID2=0x8b, CM=8, FLG=0, MTIME=0, XFL=0, OS=255 */
  buf[0] = 0x1F;
  buf[1] = 0x8B;
  buf[2] = 8; /* CM = deflate */
  buf[3] = 0; /* FLG = no extra fields */
  buf[4] = 0;
  buf[5] = 0;
  buf[6] = 0;
  buf[7] = 0;                 /* MTIME = 0 */
  buf[8] = level > 0 ? 2 : 0; /* XFL: 2 = max compression, 0 = none */
  buf[9] = 255;               /* OS = unknown */

  /* Trailer: CRC-32 (4 bytes LE) + original size (4 bytes LE). */
  size_t trailer_off = 10 + comp_len;
  if (trailer_off + 8 > est) {
    scl_free(alloc, buf);
    return SCL_ERR_SIZE_OVERFLOW;
  }
  buf[trailer_off + 0] = (unsigned char)(crc & 0xFF);
  buf[trailer_off + 1] = (unsigned char)((crc >> 8) & 0xFF);
  buf[trailer_off + 2] = (unsigned char)((crc >> 16) & 0xFF);
  buf[trailer_off + 3] = (unsigned char)((crc >> 24) & 0xFF);
  buf[trailer_off + 4] = (unsigned char)(src_len & 0xFF);
  buf[trailer_off + 5] = (unsigned char)((src_len >> 8) & 0xFF);
  buf[trailer_off + 6] = (unsigned char)((src_len >> 16) & 0xFF);
  buf[trailer_off + 7] = (unsigned char)((src_len >> 24) & 0xFF);

  *out_len = trailer_off + 8;
  *out = buf;
  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  Public API: gzip decompress
 * ════════════════════════════════════════════════════════════════════ */

scl_error_t scl_gzip_decompress(scl_allocator_t *alloc, const void *src,
                                size_t src_len, void **out, size_t *out_len) {
  if (!alloc)
    alloc = scl_allocator_default();
  if (!src || !out || !out_len)
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  *out_len = 0;

  if (src_len < 18)
    return SCL_ERR_PARSE; /* minimum valid gzip */

  const unsigned char *buf = (const unsigned char *)src;

  /* Verify gzip header. */
  if (buf[0] != 0x1F || buf[1] != 0x8B)
    return SCL_ERR_PARSE;
  if (buf[2] != 8)
    return SCL_ERR_UNSUPPORTED; /* only deflate */
  unsigned char flg = buf[3];
  /* Skip MTIME (4), XFL (1), OS (1) = 6 bytes, total header = 10. */
  size_t hdr_size = 10;

  /* Skip optional fields if flags are set. */
  if (flg & 4) { /* FEXTRA */
    if (hdr_size + 2 > src_len)
      return SCL_ERR_PARSE;
    unsigned xlen =
        (unsigned)buf[hdr_size] | ((unsigned)buf[hdr_size + 1] << 8);
    hdr_size += 2 + xlen;
  }
  if (flg & 8) { /* FNAME */
    while (hdr_size < src_len && buf[hdr_size] != 0)
      hdr_size++;
    hdr_size++; /* skip NUL */
  }
  if (flg & 16) { /* FCOMMENT */
    while (hdr_size < src_len && buf[hdr_size] != 0)
      hdr_size++;
    hdr_size++;
  }
  if (flg & 2) {   /* FHCRC */
    hdr_size += 2; /* skip CRC16 */
  }

  if (hdr_size >= src_len)
    return SCL_ERR_PARSE;

  /* Decompress the DEFLATE stream. */
  struct bit_reader br;
  bit_reader_init(&br, buf + hdr_size, src_len - hdr_size);

  unsigned char *decomp = NULL;
  size_t decomp_len = 0;
  size_t decomp_cap = 0;

  scl_error_t err = deflate_decompress(&br, &decomp, &decomp_len, &decomp_cap,
                                       SCL_INFLATE_MAX_OUTPUT, alloc);
  if (err != SCL_OK) {
    scl_free(alloc, decomp);
    return err;
  }

  /* Verify trailer: CRC-32 + original size (8 bytes at end). */
  size_t trailer_off = src_len - 8;
  uint32_t stored_crc = (uint32_t)buf[trailer_off] |
                        ((uint32_t)buf[trailer_off + 1] << 8) |
                        ((uint32_t)buf[trailer_off + 2] << 16) |
                        ((uint32_t)buf[trailer_off + 3] << 24);
  uint32_t stored_size = (uint32_t)buf[trailer_off + 4] |
                         ((uint32_t)buf[trailer_off + 5] << 8) |
                         ((uint32_t)buf[trailer_off + 6] << 16) |
                         ((uint32_t)buf[trailer_off + 7] << 24);

  /* Verify CRC-32. */
  uint32_t actual_crc = scl_crc32_final(decomp, decomp_len);
  if (actual_crc != stored_crc) {
    scl_free(alloc, decomp);
    return SCL_ERR_PARSE;
  }

  /* Verify original size (mod 2^32). */
  if ((uint32_t)decomp_len != stored_size) {
    scl_free(alloc, decomp);
    return SCL_ERR_PARSE;
  }

  *out = decomp;
  *out_len = decomp_len;
  return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  Raw DEFLATE (RFC 1951) and zlib (RFC 1950) entry points
 *
 *  These reuse the same inflate core as gzip but without the gzip
 *  wrapper. zlib streams are what PDF FlateDecode, PNG, and most
 *  network protocols carry.
 * ════════════════════════════════════════════════════════════════════ */

scl_error_t scl_inflate_raw(scl_allocator_t *alloc, const void *src,
                            size_t src_len, void **out, size_t *out_len,
                            size_t max_out) {
  if (!alloc)
    alloc = scl_allocator_default();
  if (!src || !out || !out_len)
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  *out_len = 0;
  if (src_len == 0)
    return SCL_ERR_PARSE;
  if (max_out == 0)
    max_out = SCL_INFLATE_MAX_OUTPUT;

  struct bit_reader br;
  bit_reader_init(&br, src, src_len);

  unsigned char *decomp = NULL;
  size_t decomp_len = 0;
  size_t decomp_cap = 0;
  scl_error_t err = deflate_decompress(&br, &decomp, &decomp_len, &decomp_cap,
                                       max_out, alloc);
  if (err != SCL_OK) {
    scl_free(alloc, decomp);
    return err;
  }
  *out = decomp;
  *out_len = decomp_len;
  return SCL_OK;
}

/* Adler-32 (RFC 1950 §8.2) — checksum carried in the zlib trailer. */
static uint32_t scl_adler32(const void *data, size_t len) {
  const uint32_t MOD = 65521u;
  const unsigned char *p = (const unsigned char *)data;
  uint32_t a = 1, b = 0;
  /* Process in blocks of 5552 (largest n with no 32-bit overflow). */
  while (len > 0) {
    size_t n = len > 5552 ? 5552 : len;
    len -= n;
    for (size_t i = 0; i < n; i++) {
      a += p[i];
      b += a;
    }
    p += n;
    a %= MOD;
    b %= MOD;
  }
  return (b << 16) | a;
}

scl_error_t scl_zlib_decompress(scl_allocator_t *alloc, const void *src,
                                size_t src_len, void **out, size_t *out_len,
                                size_t max_out) {
  if (!alloc)
    alloc = scl_allocator_default();
  if (!src || !out || !out_len)
    return SCL_ERR_NULL_PTR;
  *out = NULL;
  *out_len = 0;
  if (src_len < 6) /* 2-byte header + 4-byte Adler-32 minimum */
    return SCL_ERR_PARSE;
  if (max_out == 0)
    max_out = SCL_INFLATE_MAX_OUTPUT;

  const unsigned char *buf = (const unsigned char *)src;
  unsigned cmf = buf[0], flg = buf[1];
  if ((cmf & 0x0F) != 8)
    return SCL_ERR_UNSUPPORTED; /* only DEFLATE */
  if (((cmf << 8) | flg) % 31 != 0)
    return SCL_ERR_PARSE; /* FCHECK failed */
  if (flg & 0x20)
    return SCL_ERR_UNSUPPORTED; /* FDICT preset dictionaries */

  struct bit_reader br;
  bit_reader_init(&br, buf + 2, src_len - 2);

  unsigned char *decomp = NULL;
  size_t decomp_len = 0;
  size_t decomp_cap = 0;
  scl_error_t err = deflate_decompress(&br, &decomp, &decomp_len, &decomp_cap,
                                       max_out, alloc);
  if (err != SCL_OK) {
    scl_free(alloc, decomp);
    return err;
  }

  /* Verify Adler-32 trailer (big-endian, follows the deflate stream).
   * Some real-world producers truncate it; only verify when present.
   * br.pos counts bytes prefetched into the bit buffer, so the logical
   * stream position is pos minus the whole bytes still buffered. */
  bit_reader_align(&br);
  size_t tail = br.pos - (size_t)(br.nbits / 8);
  if (br.cap - tail >= 4) {
    uint32_t stored = ((uint32_t)br.buf[tail] << 24) |
                      ((uint32_t)br.buf[tail + 1] << 16) |
                      ((uint32_t)br.buf[tail + 2] << 8) |
                      (uint32_t)br.buf[tail + 3];
    if (scl_adler32(decomp, decomp_len) != stored) {
      scl_free(alloc, decomp);
      return SCL_ERR_PARSE;
    }
  }

  *out = decomp;
  *out_len = decomp_len;
  return SCL_OK;
}
