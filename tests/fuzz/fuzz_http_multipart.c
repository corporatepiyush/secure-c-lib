/* Fuzz harness for scl_http_parse_multipart — feeds random byte
 * sequences as multipart/form-data bodies with random content-type
 * boundary strings, asserting no crashes, no out-of-bounds accesses,
 * and no undefined behaviour.
 *
 * A valid multipart body looks like:
 *
 *   --boundary\r\n
 *   Content-Disposition: form-data; name="field"\r\n
 *   \r\n
 *   value\r\n
 *   --boundary--\r\n
 *
 * The fuzzer will explore both valid and invalid variants. */
#include "scl_http_server.h"
#include "scl_string.h"

#include <string.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* Need at least a valid header-like prefix to extract boundary. */
    if (size < 8 || size > 65536) return 0;

    /* We split the input into two halves:
     *   first half  → content-type header value (to extract boundary)
     *   second half → body bytes
     *
     * The boundary in the content-type is taken from the raw bytes as
     * a "boundary=..." parameter. If none is found the parser should
     * safely return 0. */
    size_t half = size / 2;

    char *ct = (char *)malloc(half + 1);
    if (!ct) return 0;
    memcpy(ct, data, half);
    ct[half] = '\0';

    size_t body_len = size - half;
    const void *body = data + half;

    scl_http_upload_t uploads[SCL_HTTP_MAX_UPLOADS];
    size_t count = 0;
    int ret = scl_http_parse_multipart(body, body_len, ct,
                                       uploads, &count,
                                       SCL_HTTP_MAX_UPLOADS);

    /* Validate invariants regardless of return value: */
    if (count > SCL_HTTP_MAX_UPLOADS) __builtin_trap();
    for (size_t i = 0; i < count; i++) {
        /* Pointers are valid or NULL — not trappable, but we can
         * check that data pointers lie within the body range if set. */
        if (uploads[i].data && body_len > 0) {
            const unsigned char *d = (const unsigned char *)uploads[i].data;
            const unsigned char *b = (const unsigned char *)body;
            /* data must be within the body buffer */
            if (d < b || d + uploads[i].data_len > b + body_len)
                __builtin_trap();
        }
        if (ret < 0) break; /* error — count may be partial, ignore */
    }

    free(ct);
    return 0;
}
