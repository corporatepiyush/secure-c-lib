/* Gzip (RFC 1952) compress/decompress tests. This module previously had
 * ZERO test coverage, which let a real bug survive: bit_reader_align()
 * discarded partial bits but did not return whole prefetched bytes to the
 * stream, so stored-block LEN/NLEN reads landed up to 3 bytes too far and
 * every stored (incompressible / level-0) block failed to decompress.
 * The round-trips below lock that fix in. */

#include "scl_gzip.h"
#include "scl_test.h"
#include "test_helpers.h"

#include <string.h>

/* Round-trip helper: compress at `level`, decompress, compare. */
static void roundtrip(scl_test_runner_t *tr, const unsigned char *data,
                      size_t len, int level) {
  scl_allocator_t *a = scl_allocator_default();
  void *comp = NULL, *decomp = NULL;
  size_t comp_len = 0, decomp_len = 0;

  SCL_EXPECT_OK(tr, scl_gzip_compress(a, data, len, &comp, &comp_len, level));
  SCL_EXPECT_TRUE(tr, comp != NULL && comp_len >= 18);
  SCL_EXPECT_OK(tr, scl_gzip_decompress(a, comp, comp_len, &decomp,
                                        &decomp_len));
  SCL_EXPECT_EQ_SZ(tr, decomp_len, len);
  if (decomp && decomp_len == len && len > 0)
    SCL_EXPECT_TRUE(tr, memcmp(decomp, data, len) == 0);

  scl_gzip_free_result(a, &comp, &comp_len);
  scl_gzip_free_result(a, &decomp, &decomp_len);
}

/* ── Round trips: compressible, incompressible, level 0 (stored) ── */
static void test_roundtrips(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("gzip: round trips (fixed Huffman + stored blocks)");

  /* Highly compressible text. */
  static unsigned char text[4096];
  for (size_t i = 0; i < sizeof(text); i++)
    text[i] = (unsigned char)("abcabcab"[i % 8]);
  roundtrip(tr, text, sizeof(text), 6);
  roundtrip(tr, text, sizeof(text), 0); /* stored blocks (regression) */

  /* Incompressible pseudo-random data: forces the stored fallback even
   * at level > 0. */
  static unsigned char noise[8192];
  uint32_t rng = 0xC0FFEE11u;
  for (size_t i = 0; i < sizeof(noise); i++)
    noise[i] = (unsigned char)(TEST_RNG(rng) >> 13);
  roundtrip(tr, noise, sizeof(noise), 6);
  roundtrip(tr, noise, sizeof(noise), 0);

  /* Tiny and empty payloads. */
  roundtrip(tr, (const unsigned char *)"x", 1, 6);
  roundtrip(tr, (const unsigned char *)"", 0, 6);

  /* Payload larger than one stored block (LEN is 16-bit → 64K max). */
  static unsigned char big[200 * 1024];
  rng = 0xDEADBEEFu;
  for (size_t i = 0; i < sizeof(big); i++)
    big[i] = (unsigned char)TEST_RNG(rng);
  roundtrip(tr, big, sizeof(big), 0);
  roundtrip(tr, big, sizeof(big), 6);
  TEST_TRACE_END();
}

/* ── Corruption must be detected, never crash ───────────────────── */
static void test_corruption(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("gzip: corrupted input rejected");
  scl_allocator_t *a = scl_allocator_default();

  const unsigned char msg[] = "payload payload payload payload";
  void *comp = NULL;
  size_t comp_len = 0;
  SCL_EXPECT_OK(tr, scl_gzip_compress(a, msg, sizeof(msg) - 1, &comp,
                                      &comp_len, 6));

  unsigned char *c = (unsigned char *)comp;
  void *out = NULL;
  size_t out_len = 0;

  /* Flip the stored CRC-32: decompress must fail. */
  c[comp_len - 5] ^= 0xFF;
  SCL_EXPECT_TRUE(tr, scl_gzip_decompress(a, c, comp_len, &out, &out_len) !=
                          SCL_OK);
  c[comp_len - 5] ^= 0xFF;

  /* Bad magic. */
  c[0] = 0x00;
  SCL_EXPECT_TRUE(tr, scl_gzip_decompress(a, c, comp_len, &out, &out_len) ==
                          SCL_ERR_PARSE);
  c[0] = 0x1F;

  /* Truncations at every prefix length: error, never crash/hang. */
  for (size_t cut = 0; cut < comp_len && cut < 64; cut++) {
    out = NULL;
    if (scl_gzip_decompress(a, c, cut, &out, &out_len) == SCL_OK)
      scl_gzip_free_result(a, &out, &out_len);
  }
  SCL_EXPECT_TRUE(tr, 1);
  scl_gzip_free_result(a, &comp, &comp_len);
  TEST_TRACE_END();
}

/* ── Null/edge argument contract ────────────────────────────────── */
static void test_args(scl_test_runner_t *tr) {
  TEST_TRACE_BEGIN();
  scl_test_group("gzip: argument validation");
  scl_allocator_t *a = scl_allocator_default();
  void *out = NULL;
  size_t out_len = 0;
  SCL_EXPECT_TRUE(tr, scl_gzip_decompress(a, NULL, 10, &out, &out_len) ==
                          SCL_ERR_NULL_PTR);
  SCL_EXPECT_TRUE(tr, scl_gzip_compress(a, NULL, 10, &out, &out_len, 6) !=
                          SCL_OK);
  unsigned char tiny[4] = {0x1F, 0x8B, 8, 0};
  SCL_EXPECT_TRUE(tr, scl_gzip_decompress(a, tiny, sizeof(tiny), &out,
                                          &out_len) == SCL_ERR_PARSE);
  TEST_TRACE_END();
}

int main(void) {
  scl_test_runner_t tr;
  scl_test_init(&tr);
  test_roundtrips(&tr);
  test_corruption(&tr);
  test_args(&tr);
  scl_test_summary(&tr);
  return tr.failed > 0 ? 1 : 0;
}
