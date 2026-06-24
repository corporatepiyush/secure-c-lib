/* Fuzz harness for scl_http_mime_for_ext — feeds random extension
 * strings through the MIME lookup table, asserting no crashes.
 *
 * The table is small (32 entries) so the fuzzer will quickly cover
 * every path: hitting each known extension once, then exploring
 * unknown/empty/long/malformed extensions. */
#include "scl_http_server.h"
#include "scl_string.h"

#include <string.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1 || size > 256) return 0;

    char *ext = (char *)malloc(size + 1);
    if (!ext) return 0;
    memcpy(ext, data, size);
    ext[size] = '\0';

    const char *mime = scl_http_mime_for_ext(ext);
    (void)mime; /* must never return NULL per the API contract */

    free(ext);
    return 0;
}
