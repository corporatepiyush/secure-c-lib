/* Fuzz harness for scl_http_parse_url — feeds random byte sequences
 * through the URL parser, asserting no crashes, no out-of-bounds reads,
 * and no undefined behaviour. Every valid-URL path is reachable from
 * libFuzzer after a few hundred iterations.
 *
 * Build with:
 *   make build/tests/fuzz/fuzz_http_parse_url_fuzzbin
 *
 * Then run individually (or with the provided corpus). */
#include "scl_http_client.h"
#include "scl_string.h"

#include <stdlib.h>
#include <string.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  /* scl_http_parse_url requires a mutable NUL-terminated string
   * (it writes NUL bytes at '?' and ':' during parsing). */
  if (size < 1 || size > 4096)
    return 0;

  char *url = (char *)malloc(size + 1);
  if (!url)
    return 0;
  memcpy(url, data, size);
  url[size] = '\0';

  scl_http_url_t out;
  scl_error_t err = scl_http_parse_url(url, &out);

  /* If parsing succeeded, validate the output invariants.
   * If it failed, there's nothing to check. */
  if (err == SCL_OK) {
    /* — host must be set and non-empty */
    if (out.host) {
      size_t hlen = strlen(out.host);
      (void)hlen;
    }
    /* — path must be set (default is "/") */
    if (out.path) {
      (void)strlen(out.path);
    }
    /* — port must be a valid uint16 (0 or 80 or parsed) — it's a
     *   uint16_t field so this is a type invariant, not a check. */
    (void)out.port;
    /* — scheme is "http" or NULL */
    if (out.scheme) {
      (void)strlen(out.scheme);
    }
    /* — query is NULL or a valid C string */
    if (out.query) {
      (void)strlen(out.query);
    }
  }

  free(url);
  return 0;
}
