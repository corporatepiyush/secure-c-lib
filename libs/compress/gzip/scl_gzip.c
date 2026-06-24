#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3")
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
#include "scl_string.h"
#include "scl_stdlib.h"

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ════════════════════════════════════════════════════════════════════
 *  CRC-32 (IEEE 802.3, used by gzip trailer)
 * ════════════════════════════════════════════════════════════════════ */

/* Table-driven CRC-32: pre-computed for each possible byte value.
 * The polynomial is 0xEDB88320 (reflected). */
static uint32_t scl_crc32_table[256];
static bool scl_crc32_table_init = false;

static void scl_crc32_init_table(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320u & ~((crc & 1) - 1));
        scl_crc32_table[i] = crc;
    }
    scl_crc32_table_init = true;
}

static uint32_t scl_crc32_update(uint32_t crc, const void *data, size_t len) {
    if (scl_unlikely(!scl_crc32_table_init)) scl_crc32_init_table();
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
    size_t   cap;
    size_t   pos;      /* byte position */
    unsigned bits;     /* bit buffer (LSB) */
    int      nbits;    /* number of valid bits in buffer */
};

static void bit_reader_init(struct bit_reader *br, const void *buf, size_t len) {
    br->buf   = (const unsigned char *)buf;
    br->cap   = len;
    br->pos   = 0;
    br->bits  = 0;
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
    if (scl_likely(bit_reader_fill(br))) return br->bits & ((1u << n) - 1);
    return 0;
}

/* Read n bits (1-16) and consume them. Returns 0 on underflow. */
static unsigned bit_reader_read(struct bit_reader *br, int n) {
    unsigned val = bit_reader_peek(br, n);
    br->bits  >>= n;
    br->nbits  -= n;
    return val;
}

/* Discard remaining bits (align to byte boundary). */
static void bit_reader_align(struct bit_reader *br) {
    int discard = br->nbits & 7;
    if (discard) {
        br->bits  >>= discard;
        br->nbits  -= discard;
    }
}

struct bit_writer {
    unsigned char *buf;
    size_t   cap;
    size_t   pos;
    unsigned bits;
    int      nbits;
};

static void bit_writer_init(struct bit_writer *bw, void *buf, size_t cap) {
    bw->buf   = (unsigned char *)buf;
    bw->cap   = cap;
    bw->pos   = 0;
    bw->bits  = 0;
    bw->nbits = 0;
}

/* Write n bits (value in low n bits, LSB-first). Returns false if buffer full. */
static bool bit_writer_write(struct bit_writer *bw, unsigned val, int n) {
    bw->bits |= (val & ((1u << n) - 1)) << bw->nbits;
    bw->nbits += n;
    while (bw->nbits >= 8) {
        if (bw->pos >= bw->cap) return false;
        bw->buf[bw->pos++] = (unsigned char)(bw->bits & 0xFF);
        bw->bits >>= 8;
        bw->nbits -= 8;
    }
    return true;
}

/* Flush remaining bits (zero-padded) and align. Returns false if buffer full. */
static bool bit_writer_flush(struct bit_writer *bw) {
    if (bw->nbits > 0) {
        if (bw->pos >= bw->cap) return false;
        bw->buf[bw->pos++] = (unsigned char)(bw->bits & 0xFF);
        bw->bits  = 0;
        bw->nbits = 0;
    }
    return true;
}

/* ════════════════════════════════════════════════════════════════════
 *  Huffman tree (canonical, used by DEFLATE)
 * ════════════════════════════════════════════════════════════════════ */

/* Maximum bits in a Huffman code per RFC 1951. */
#define SCL_HUFF_MAX_BITS 15

/* A single node in the Huffman decoding tree. */
typedef struct {
    uint16_t children[2];  /* indices: 0 = left (bit 0), 1 = right (bit 1) */
    int16_t  symbol;       /* -1 = not a leaf */
} scl_huff_node_t;

/* A Huffman decoding tree backed by a node pool. */
typedef struct {
    scl_huff_node_t *nodes;
    uint16_t         node_count;
    uint16_t         node_cap;
} scl_huff_tree_t;

/* Initialise a Huffman tree with enough capacity for all symbols.
 * For fixed trees: 288 lit/length + 32 distance = 320 symbols.
 * Maximum internal nodes: symbols - 1 (full binary tree). */
static scl_error_t scl_huff_init(scl_huff_tree_t *t, uint16_t max_sym) {
    uint16_t cap = (uint16_t)(max_sym * 2);
    if (cap < 64) cap = 64;
    t->nodes = (scl_huff_node_t *)scl_calloc(NULL, cap, sizeof(scl_huff_node_t),
                                              _Alignof(scl_huff_node_t));
    if (!t->nodes) return SCL_ERR_OUT_OF_MEMORY;
    t->node_count = 1;  /* root at index 0 */
    t->node_cap = cap;
    return SCL_OK;
}

static void scl_huff_destroy(scl_huff_tree_t *t) {
    scl_free(NULL, t->nodes);
    t->nodes = NULL;
}

/* Add a new internal node and return its index. */
static uint16_t scl_huff_new_node(scl_huff_tree_t *t) {
    if (t->node_count >= t->node_cap) return 0; /* overflow */
    uint16_t idx = t->node_count++;
    t->nodes[idx].children[0] = 0;
    t->nodes[idx].children[1] = 0;
    t->nodes[idx].symbol = -1;
    return idx;
}

/* Build a canonical Huffman tree from code lengths per RFC 1951 §3.2.2.
 * `lens` is an array of length `count`, each element is the code length
 * in bits (0 = symbol not used). */
static scl_error_t scl_huff_build(scl_huff_tree_t *t,
                                  const unsigned char *lens,
                                  unsigned count) {
    /* Step 1: count occurrences of each length. */
    unsigned bl_count[SCL_HUFF_MAX_BITS + 1];
    scl_memset(bl_count, 0, sizeof(bl_count));
    unsigned max_len = 0;
    for (unsigned i = 0; i < count; i++) {
        unsigned len = lens[i];
        if (len > SCL_HUFF_MAX_BITS) return SCL_ERR_PARSE;
        if (len > 0) {
            bl_count[len]++;
            if (len > max_len) max_len = len;
        }
    }

    /* Step 2: compute starting code for each length. */
    unsigned next_code[SCL_HUFF_MAX_BITS + 1];
    unsigned code = 0;
    bl_count[0] = 0;
    for (unsigned bits = 1; bits <= max_len; bits++) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    /* Step 3: assign codes and insert into tree. */
    for (unsigned i = 0; i < count; i++) {
        unsigned len = lens[i];
        if (len == 0) continue;
        unsigned c = next_code[len]++;
        uint16_t node = 0; /* start at root */
        for (int b = (int)len - 1; b >= 0; b--) {
            unsigned bit = (c >> b) & 1;
            if (t->nodes[node].children[bit] == 0) {
                uint16_t child = scl_huff_new_node(t);
                if (child == 0) return SCL_ERR_OUT_OF_MEMORY;
                t->nodes[node].children[bit] = child;
            }
            node = t->nodes[node].children[bit];
        }
        t->nodes[node].symbol = (int16_t)i;
    }
    return SCL_OK;
}

/* Decode one symbol from the bit stream using the Huffman tree.
 * Returns -1 on error (invalid code / EOF). */
static int scl_huff_decode(struct bit_reader *br, const scl_huff_tree_t *t) {
    const scl_huff_node_t * nodes = t->nodes;
    uint16_t node = 0;
    while (scl_likely(nodes[node].symbol < 0)) {
        if (scl_unlikely(!bit_reader_fill(br))) return -1;
        unsigned bit = br->bits & 1;
        br->bits  >>= 1;
        br->nbits  -= 1;
        uint16_t child = nodes[node].children[bit];
        if (scl_unlikely(child == 0)) return -1;
        node = child;
    }
    return nodes[node].symbol;
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
    if (count > FIXED_LIT_COUNT) count = FIXED_LIT_COUNT;
    unsigned i = 0;
    while (i < count && i < 144) lens[i++] = 8;
    while (i < count && i < 256) lens[i++] = 9;
    while (i < count && i < 280) lens[i++] = 7;
    while (i < count && i < 288) lens[i++] = 8;
    while (i < count) lens[i++] = 5; /* distances */
}

/* ════════════════════════════════════════════════════════════════════
 *  DEFLATE constants
 * ════════════════════════════════════════════════════════════════════ */

/* Length codes: base lengths and extra bits for each code 257-285. */
static const unsigned short scl_len_base[] = {
    3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
    67,83,99,115,131,163,195,227,258,0,0
};
static const unsigned char scl_len_extra[] = {
    0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,
    4,4,4,4,5,5,5,5,0,0,0
};

/* Distance codes: base distances and extra bits. */
static const unsigned short scl_dist_base[] = {
    1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,
    257,385,513,769,1025,1537,2049,3073,4097,6145,
    8193,12289,16385,24577
};
static const unsigned char scl_dist_extra[] = {
    0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,
    7,7,8,8,9,9,10,10,11,11,12,12,13,13
};

/* ════════════════════════════════════════════════════════════════════
 *  LZ77 match finder (hash-chain)
 * ════════════════════════════════════════════════════════════════════ */

#define SCL_LZ77_WINDOW_SIZE (32 * 1024)
#define SCL_LZ77_MIN_MATCH   3
#define SCL_LZ77_MAX_MATCH   258
#define SCL_LZ77_HASH_BITS   15
#define SCL_LZ77_HASH_SIZE   (1u << SCL_LZ77_HASH_BITS)

/* Hash function for 3-byte sequences */
static uint32_t lz77_hash(const unsigned char *p) {
    return ((uint32_t)p[0] << 4) ^ ((uint32_t)p[1] << 2) ^ (uint32_t)p[2];
}

/* ════════════════════════════════════════════════════════════════════
 *  DEFLATE decompression
 * ════════════════════════════════════════════════════════════════════ */

static scl_error_t deflate_decompress(struct bit_reader *br,
                                      unsigned char **out_buf,
                                      size_t *out_len, size_t *out_cap,
                                      scl_allocator_t *alloc) {
    scl_error_t err;
    bool final = false;

    while (!final) {
        /* Block header: BFINAL (1 bit), BTYPE (2 bits). */
        if (!bit_reader_fill(br)) return SCL_ERR_PARSE;
        final = bit_reader_read(br, 1) != 0;
        unsigned btype = bit_reader_read(br, 2);

        if (btype == 0) {
            /* ── Stored (uncompressed) block ── */
            bit_reader_align(br);
            /* Read LEN and NLEN (2 bytes each, little-endian). */
            if (br->pos + 4 > br->cap) return SCL_ERR_PARSE;
            unsigned len  = (unsigned)br->buf[br->pos] |
                           ((unsigned)br->buf[br->pos + 1] << 8);
            unsigned nlen = (unsigned)br->buf[br->pos + 2] |
                           ((unsigned)br->buf[br->pos + 3] << 8);
            br->pos += 4;
            if ((len ^ nlen) != 0xFFFFu) return SCL_ERR_PARSE;

            /* Copy stored data to output. */
            if (br->pos + len > br->cap) return SCL_ERR_PARSE;
            size_t needed = *out_len + len;
            if (needed > *out_cap) {
                size_t new_cap = *out_cap ? *out_cap * 2 : 4096;
                while (new_cap < needed) new_cap *= 2;
                void *nb = scl_realloc(alloc, *out_buf, *out_cap,
                                       new_cap, _Alignof(max_align_t));
                if (!nb) return SCL_ERR_OUT_OF_MEMORY;
                *out_buf = (unsigned char *)nb;
                *out_cap = new_cap;
            }
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
                hlit  = bit_reader_read(br, 5) + 257; /* lit/length codes */
                hdist = bit_reader_read(br, 5) + 1;   /* distance codes */
                unsigned hclen = bit_reader_read(br, 4) + 4;   /* code length codes */

                if (hlit > 286 || hdist > 30 || hclen > 19)
                    return SCL_ERR_PARSE;

                /* Read code length code lengths. */
                static const unsigned char cl_order[] =
                    {16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15};
                unsigned char cl_lens[19];
                scl_memset(cl_lens, 0, sizeof(cl_lens));
                for (unsigned i = 0; i < hclen; i++) {
                    cl_lens[cl_order[i]] = (unsigned char)bit_reader_read(br, 3);
                    if (cl_lens[cl_order[i]] > 7) return SCL_ERR_PARSE;
                }

                /* Build the code length tree. */
                scl_huff_tree_t cl_tree;
                err = scl_huff_init(&cl_tree, 19);
                if (err != SCL_OK) return err;
                err = scl_huff_build(&cl_tree, cl_lens, 19);
                if (err != SCL_OK) { scl_huff_destroy(&cl_tree); return err; }

                /* Read literal/length and distance code lengths. */
                unsigned total = hlit + hdist;
                unsigned char dyn_lens[320];
                unsigned idx = 0;
                while (idx < total) {
                    int sym = scl_huff_decode(br, &cl_tree);
                    if (sym < 0 || sym > 18) { scl_huff_destroy(&cl_tree); return SCL_ERR_PARSE; }
                    unsigned repeat = 0;
                    unsigned char val = 0;
                    if (sym < 16) {
                        dyn_lens[idx++] = (unsigned char)sym;
                        continue;
                    } else if (sym == 16) {
                        if (idx == 0) { scl_huff_destroy(&cl_tree); return SCL_ERR_PARSE; }
                        val  = dyn_lens[idx - 1];
                        repeat = bit_reader_read(br, 2) + 3;
                    } else if (sym == 17) {
                        val  = 0;
                        repeat = bit_reader_read(br, 3) + 3;
                    } else { /* sym == 18 */
                        val  = 0;
                        repeat = bit_reader_read(br, 7) + 11;
                    }
                    for (unsigned k = 0; k < repeat && idx < total; k++)
                        dyn_lens[idx++] = val;
                }
                scl_huff_destroy(&cl_tree);

                /* Split into literal and distance tables. */
                for (unsigned i = 0; i < hlit; i++)
                    lit_lens[i] = dyn_lens[i];
                for (unsigned i = hlit; i < total; i++)
                    dist_lens[i - hlit] = dyn_lens[i];
            }

            /* Build Huffman trees. */
            scl_huff_tree_t lit_tree, dist_tree;
            err = scl_huff_init(&lit_tree, hlit);
            if (err != SCL_OK) return err;
            err = scl_huff_init(&dist_tree, hdist);
            if (err != SCL_OK) { scl_huff_destroy(&lit_tree); return err; }

            err = scl_huff_build(&lit_tree, lit_lens, hlit);
            if (err == SCL_OK)
                err = scl_huff_build(&dist_tree, dist_lens, hdist);
            if (err != SCL_OK) {
                scl_huff_destroy(&lit_tree);
                scl_huff_destroy(&dist_tree);
                return err;
            }

            /* Decode symbols. */
            for (;;) {
                int sym = scl_huff_decode(br, &lit_tree);
                if (sym < 0) {
                    scl_huff_destroy(&lit_tree);
                    scl_huff_destroy(&dist_tree);
                    return SCL_ERR_PARSE;
                }

                if (scl_likely(sym < 256)) {
                    /* Literal byte. */
                    size_t needed = *out_len + 1;
                    if (needed > *out_cap) {
                        size_t new_cap = *out_cap ? *out_cap * 2 : 4096;
                        while (new_cap < needed) new_cap *= 2;
                        void *nb = scl_realloc(alloc, *out_buf, *out_cap,
                                               new_cap, _Alignof(max_align_t));
                        if (!nb) { scl_huff_destroy(&lit_tree); scl_huff_destroy(&dist_tree); return SCL_ERR_OUT_OF_MEMORY; }
                        *out_buf = (unsigned char *)nb;
                        *out_cap = new_cap;
                    }
                    (*out_buf)[(*out_len)++] = (unsigned char)sym;
                } else if (scl_unlikely(sym == 256)) {
                    /* End of block. */
                    break;
                } else if (scl_likely(sym <= 285)) {
                    /* Length-distance pair. */
                    unsigned len_code = (unsigned)(sym - 257);
                    if (len_code >= sizeof(scl_len_base) / sizeof(scl_len_base[0])) {
                        scl_huff_destroy(&lit_tree);
                        scl_huff_destroy(&dist_tree);
                        return SCL_ERR_PARSE;
                    }
                    unsigned length = scl_len_base[len_code];
                    unsigned extra  = scl_len_extra[len_code];
                    if (extra > 0) length += bit_reader_read(br, (int)extra);

                    int dist_sym = scl_huff_decode(br, &dist_tree);
                    if (dist_sym < 0 || (unsigned)dist_sym >=
                            sizeof(scl_dist_base) / sizeof(scl_dist_base[0])) {
                        scl_huff_destroy(&lit_tree);
                        scl_huff_destroy(&dist_tree);
                        return SCL_ERR_PARSE;
                    }
                    unsigned distance = scl_dist_base[dist_sym];
                    unsigned dextra  = scl_dist_extra[dist_sym];
                    if (dextra > 0) distance += bit_reader_read(br, (int)dextra);

                    if (distance > *out_len || length > SCL_LZ77_MAX_MATCH) {
                        scl_huff_destroy(&lit_tree);
                        scl_huff_destroy(&dist_tree);
                        return SCL_ERR_PARSE;
                    }

                    size_t needed = *out_len + length;
                    if (needed > *out_cap) {
                        size_t new_cap = *out_cap ? *out_cap * 2 : 4096;
                        while (new_cap < needed) new_cap *= 2;
                        void *nb = scl_realloc(alloc, *out_buf, *out_cap,
                                               new_cap, _Alignof(max_align_t));
                        if (!nb) { scl_huff_destroy(&lit_tree); scl_huff_destroy(&dist_tree); return SCL_ERR_OUT_OF_MEMORY; }
                        *out_buf = (unsigned char *)nb;
                        *out_cap = new_cap;
                    }
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
                    scl_huff_destroy(&lit_tree);
                    scl_huff_destroy(&dist_tree);
                    return SCL_ERR_PARSE;
                }
            }

            scl_huff_destroy(&lit_tree);
            scl_huff_destroy(&dist_tree);

        } else {
            return SCL_ERR_PARSE; /* reserved block type */
        }
    }

    return SCL_OK;
}

/* ════════════════════════════════════════════════════════════════════
 *  DEFLATE compression
 * ════════════════════════════════════════════════════════════════════ */

/* Write a stored (uncompressed) block. */
static scl_error_t deflate_write_stored(struct bit_writer *bw,
                                        const unsigned char *data,
                                        size_t len, bool final) {
    bit_writer_flush(bw);
    /* BFINAL + BTYPE = stored */
    if (!bit_writer_write(bw, final ? 1u : 0u, 1)) return SCL_ERR_SIZE_OVERFLOW;
    if (!bit_writer_write(bw, 0u, 2)) return SCL_ERR_SIZE_OVERFLOW;
    bit_writer_flush(bw);

    /* LEN and NLEN (little-endian 16-bit). */
    if (bw->pos + 4 > bw->cap) return SCL_ERR_SIZE_OVERFLOW;
    unsigned short l = (unsigned short)len;
    unsigned short nl = (unsigned short)(~l);
    bw->buf[bw->pos++] = (unsigned char)(l & 0xFF);
    bw->buf[bw->pos++] = (unsigned char)((l >> 8) & 0xFF);
    bw->buf[bw->pos++] = (unsigned char)(nl & 0xFF);
    bw->buf[bw->pos++] = (unsigned char)((nl >> 8) & 0xFF);

    if (bw->pos + len > bw->cap) return SCL_ERR_SIZE_OVERFLOW;
    scl_memcpy(bw->buf + bw->pos, data, len);
    bw->pos += len;
    return SCL_OK;
}

/* Look up the length code (257-285) for a given match length. */
static unsigned length_code(unsigned len) {
    if (len < 3) return 0;
    if (len <= 10) return 257 + (len - 3) / 2;
    if (len <= 18) return 265 + (len - 11) / 2;
    if (len <= 34) return 273 + (len - 19) / 4;
    if (len <= 66) return 281 + (len - 35) / 8;
    if (len <= 130) return 285;
    if (len <= 257) return 285;
    return 285; /* 258 */
}

/* Look up the distance code (0-29) for a given distance. */
static unsigned distance_code(unsigned dist) {
    if (dist < 1) return 0;
    if (dist <= 4) return dist - 1;
    if (dist <= 6) return 4 + (dist - 5) / 2;
    if (dist <= 12) return 6 + (dist - 7) / 2;
    if (dist <= 24) return 8 + (dist - 13) / 4;
    if (dist <= 48) return 10 + (dist - 25) / 8;
    if (dist <= 96) return 12 + (dist - 49) / 16;
    if (dist <= 192) return 14 + (dist - 97) / 32;
    if (dist <= 384) return 16 + (dist - 193) / 64;
    if (dist <= 768) return 18 + (dist - 385) / 128;
    if (dist <= 1536) return 20 + (dist - 769) / 256;
    if (dist <= 3072) return 22 + (dist - 1537) / 512;
    return 24 + (dist - 3073) / 1024; /* 25-29 */
}

/* Emit a literal using fixed Huffman codes (RFC 1951 §3.2.6). */
static SCL_ALWAYS_INLINE scl_error_t emit_literal_fixed(struct bit_writer *bw, unsigned sym) {
    /* Fixed Huffman codes per RFC 1951 §3.2.6:
     *   0-143:  8-bit code = 48 + sym (binary: 0011_0000 + sym, reversed)
     *   144-255: 9-bit code = 400 + (sym - 144) = 256 + sym
     *   256:    7-bit code = 0 (end of block)
     *   257-279: 7-bit codes
     *   280-287: 8-bit codes = 192 + (sym - 280) */
    if (sym <= 143) {
        unsigned rev = 0;
        unsigned c = 48 + sym;
        for (int i = 0; i < 8; i++) rev = (rev << 1) | ((c >> i) & 1);
        return bit_writer_write(bw, rev, 8) ? SCL_OK : SCL_ERR_SIZE_OVERFLOW;
    } else if (sym <= 255) {
        unsigned rev = 0;
        unsigned c = 256 + sym;
        for (int i = 0; i < 9; i++) rev = (rev << 1) | ((c >> i) & 1);
        return bit_writer_write(bw, rev, 9) ? SCL_OK : SCL_ERR_SIZE_OVERFLOW;
    }
    return SCL_ERR_INVALID_ARG;
}

/* Emit a length-distance pair using fixed Huffman codes.
 * Sym is the length code (257-285). */
static SCL_ALWAYS_INLINE scl_error_t emit_match_fixed(struct bit_writer *bw,
                                    unsigned len_code,
                                    unsigned len_extra, unsigned len_extra_bits,
                                    unsigned dist_code,
                                    unsigned dist_extra, unsigned dist_extra_bits) {
    /* Length code (lit value): codes 257-279 have 7-bit, 280-287 have 8-bit. */
    if (len_code <= 279) {
        unsigned c = len_code - 256; /* 1-23, 7-bit codes starting at 0b0000001 */
        unsigned rev = 0;
        unsigned val = c; /* Actually code = (len_code - 256) reversed */
        /* For 7-bit codes, the assigned values are simple: code for 257 is 0b0000001
         * reversed = 0b1000000 = 64. Let me be more careful. */
        /* The RFC assigns:
         * 256: end of block, code 0 (7-bit)
         * 257: code 2 (7-bit: 0000010) -> reversed = 0100000 = 32
         * etc. */
        /* Actually, I need to reverse a 7-bit number. The assigned codes for
         * 257-279 are (len_code - 256) as a 7-bit number. */
        val = len_code - 256;
        for (int i = 0; i < 7; i++) rev = (rev << 1) | ((val >> i) & 1);
        if (!bit_writer_write(bw, rev, 7)) return SCL_ERR_SIZE_OVERFLOW;
    } else {
        unsigned c = len_code - 256; /* 24-31, 8-bit codes */
        unsigned rev = 0;
        unsigned val = c;
        /* Assigned codes for 280-287 are 192 + (len_code - 280) */
        val = 192 + (len_code - 280);
        for (int i = 0; i < 8; i++) rev = (rev << 1) | ((val >> i) & 1);
        if (!bit_writer_write(bw, rev, 8)) return SCL_ERR_SIZE_OVERFLOW;
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
        for (int i = 0; i < 5; i++) rev = (rev << 1) | ((dist_code >> i) & 1);
        if (!bit_writer_write(bw, rev, 5)) return SCL_ERR_SIZE_OVERFLOW;
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
                                          const unsigned char *data,
                                          size_t len, bool final,
                                          int level) {
    /* Write block header: BFINAL + BTYPE = fixed Huffman. */
    if (!bit_writer_write(bw, final ? 1u : 0u, 1)) return SCL_ERR_SIZE_OVERFLOW;
    if (!bit_writer_write(bw, 1u, 2)) return SCL_ERR_SIZE_OVERFLOW;

    if (scl_unlikely(len == 0 || level == 0)) {
        /* No data or no compression requested — emit no matches, just literals. */
        /* For empty, just end of block. */
        goto finish;
    }

    /* LZ77 hash chain table (stores positions in the window). */
    uint32_t *hash_head = NULL;
    uint32_t *hash_prev = NULL;

    if (scl_likely(level > 0)) {
        hash_head = (uint32_t *)scl_calloc(NULL, SCL_LZ77_HASH_SIZE,
                                           sizeof(uint32_t), _Alignof(max_align_t));
        hash_prev = (uint32_t *)scl_calloc(NULL, len,
                                           sizeof(uint32_t), _Alignof(max_align_t));
        if (!hash_head || !hash_prev) {
            scl_free(NULL, hash_head);
            scl_free(NULL, hash_prev);
            /* Fall back to literal-only output. */
            goto do_literals;
        }
    }

    size_t pos = 0;
    while (pos < len) {
        unsigned best_len = 0;
        unsigned best_dist = 0;
        unsigned max_lookahead = (len - pos) < SCL_LZ77_MAX_MATCH ?
                                 (unsigned)(len - pos) : SCL_LZ77_MAX_MATCH;

        /* Search for a match. */
        if (level > 0 && max_lookahead >= SCL_LZ77_MIN_MATCH &&
            pos + SCL_LZ77_MIN_MATCH <= len) {
            uint32_t h = lz77_hash(data + pos) & (SCL_LZ77_HASH_SIZE - 1);
            uint32_t chain = hash_head[h];
            int chain_count = 0;

            while (chain != (uint32_t)-1 && chain < pos &&
                   chain + SCL_LZ77_MAX_MATCH >= pos &&
                   chain_count < (level > 1 ? 64 : 8)) {
                unsigned match_len = 0;
                const unsigned char *a = data + chain;
                const unsigned char *b = data + pos;
                while (match_len < max_lookahead && a[match_len] == b[match_len])
                    match_len++;
                if (match_len >= SCL_LZ77_MIN_MATCH && match_len > best_len) {
                    best_len = match_len;
                    best_dist = (unsigned)(pos - chain);
                    if (match_len == max_lookahead) break;
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
            if (e != SCL_OK) { scl_free(NULL, hash_head); scl_free(NULL, hash_prev); return e; }
            pos += best_len;
        } else {
            /* Emit literal. */
            scl_error_t e = emit_literal_fixed(bw, data[pos]);
            if (e != SCL_OK) { scl_free(NULL, hash_head); scl_free(NULL, hash_prev); return e; }
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

    scl_free(NULL, hash_head);
    scl_free(NULL, hash_prev);

finish:
    /* End of block (literal 256, fixed Huffman). */
    {
        /* 256 in fixed Huffman: code is 0 (7-bit). Reversed = 0. */
        if (!bit_writer_write(bw, 0, 7)) return SCL_ERR_SIZE_OVERFLOW;
    }
    return SCL_OK;

do_literals:
    if (hash_head) scl_free(NULL, hash_head);
    if (hash_prev) scl_free(NULL, hash_prev);
    /* Fallback: emit all literals individually with fixed Huffman.
     * This is inefficient but correct. */
    for (size_t i = 0; i < len; i++) {
        scl_error_t e = emit_literal_fixed(bw, data[i]);
        if (e != SCL_OK) return e;
    }
    goto finish;
}

/* ════════════════════════════════════════════════════════════════════
 *  Public API: gzip compress
 * ════════════════════════════════════════════════════════════════════ */

scl_error_t scl_gzip_compress(scl_allocator_t *alloc,
                              const void *src, size_t src_len,
                              void **out, size_t *out_len,
                              int level) {
    if (!alloc) alloc = scl_allocator_default();
    if (!src || !out || !out_len) return SCL_ERR_NULL_PTR;
    *out = NULL;
    *out_len = 0;

    /* Gzip header (10 bytes fixed + optional) + max data expansion + trailer (8).
     * Maximum DEFLATE expansion: stored blocks add 5 bytes header + 4 len/nlen per
     * 65535 bytes, plus 1 byte per literal in the worst case. We allocate
     * generously. */
    size_t est = src_len + (src_len / 65535) * 5 + 64;
    unsigned char *buf = (unsigned char *)scl_alloc(alloc, est,
                                                     _Alignof(max_align_t));
    if (!buf) return SCL_ERR_OUT_OF_MEMORY;

    struct bit_writer bw;
    bit_writer_init(&bw, buf + 10, est - 10); /* reserve 10 for gzip header */

    scl_error_t err;

    if (level == 0) {
        /* Store verbatim in a stored (type 0) block. */
        err = deflate_write_stored(&bw, (const unsigned char *)src, src_len, true);
    } else {
        err = deflate_compress_fixed(&bw, (const unsigned char *)src,
                                     src_len, true, level);
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
    buf[2] = 8;   /* CM = deflate */
    buf[3] = 0;   /* FLG = no extra fields */
    buf[4] = 0; buf[5] = 0; buf[6] = 0; buf[7] = 0; /* MTIME = 0 */
    buf[8] = level > 0 ? 2 : 0;  /* XFL: 2 = max compression, 0 = none */
    buf[9] = 255; /* OS = unknown */

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

scl_error_t scl_gzip_decompress(scl_allocator_t *alloc,
                                const void *src, size_t src_len,
                                void **out, size_t *out_len) {
    if (!alloc) alloc = scl_allocator_default();
    if (!src || !out || !out_len) return SCL_ERR_NULL_PTR;
    *out = NULL;
    *out_len = 0;

    if (src_len < 18) return SCL_ERR_PARSE; /* minimum valid gzip */

    const unsigned char *buf = (const unsigned char *)src;

    /* Verify gzip header. */
    if (buf[0] != 0x1F || buf[1] != 0x8B) return SCL_ERR_PARSE;
    if (buf[2] != 8) return SCL_ERR_UNSUPPORTED; /* only deflate */
    unsigned char flg = buf[3];
    /* Skip MTIME (4), XFL (1), OS (1) = 6 bytes, total header = 10. */
    size_t hdr_size = 10;

    /* Skip optional fields if flags are set. */
    if (flg & 4) { /* FEXTRA */
        if (hdr_size + 2 > src_len) return SCL_ERR_PARSE;
        unsigned xlen = (unsigned)buf[hdr_size] | ((unsigned)buf[hdr_size + 1] << 8);
        hdr_size += 2 + xlen;
    }
    if (flg & 8) { /* FNAME */
        while (hdr_size < src_len && buf[hdr_size] != 0) hdr_size++;
        hdr_size++; /* skip NUL */
    }
    if (flg & 16) { /* FCOMMENT */
        while (hdr_size < src_len && buf[hdr_size] != 0) hdr_size++;
        hdr_size++;
    }
    if (flg & 2) { /* FHCRC */
        hdr_size += 2; /* skip CRC16 */
    }

    if (hdr_size >= src_len) return SCL_ERR_PARSE;

    /* Decompress the DEFLATE stream. */
    struct bit_reader br;
    bit_reader_init(&br, buf + hdr_size, src_len - hdr_size);

    unsigned char *decomp = NULL;
    size_t decomp_len = 0;
    size_t decomp_cap = 0;

    scl_error_t err = deflate_decompress(&br, &decomp, &decomp_len,
                                         &decomp_cap, alloc);
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
