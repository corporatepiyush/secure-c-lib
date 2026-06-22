#include "scl_string.h"
#include "../stdlib/scl_stdlib.h"

/* ── Length ─────────────────────────────────────────────────── */

size_t scl_strlen(const char *s) {
    return strlen(s);
}

size_t scl_strlen_fast(const char *s) {
#if defined(SCL_ARCH_X86_64) && defined(__GNUC__)
    size_t len;
    /* REPNE SCASB — fast strlen on modern x86_64 */
    __asm__ volatile(
        "mov %1, %0\n\t"
        "1:\n\t"
        "cmpb $0, (%0)\n\t"
        "je 2f\n\t"
        "inc %0\n\t"
        "jmp 1b\n\t"
        "2:\n\t"
        "sub %1, %0"
        : "=&r"(len)
        : "r"(s)
        : "memory"
    );
    return len;
#else
    return strlen(s);
#endif
}

/* ── Copy / concat ──────────────────────────────────────────── */

char *scl_strcpy(char *dest, const char *src) {
    return strcpy(dest, src);
}

char *scl_strncpy(char *dest, const char *src, size_t n) {
    return strncpy(dest, src, n);
}

char *scl_strcat(char *dest, const char *src) {
    return strcat(dest, src);
}

char *scl_strncat(char *dest, const char *src, size_t n) {
    return strncat(dest, src, n);
}

/* ── Compare ────────────────────────────────────────────────── */

int scl_strcmp(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}

int scl_strncmp(const char *s1, const char *s2, size_t n) {
    return strncmp(s1, s2, n);
}

/* ── Search ─────────────────────────────────────────────────── */

char *scl_strchr(const char *s, int c) { return strchr(s, c); }
char *scl_strrchr(const char *s, int c) { return strrchr(s, c); }
char *scl_strstr(const char *haystack, const char *needle) { return strstr(haystack, needle); }
char *scl_strpbrk(const char *s, const char *accept) { return strpbrk(s, accept); }
size_t scl_strspn(const char *s, const char *accept) { return strspn(s, accept); }
size_t scl_strcspn(const char *s, const char *reject) { return strcspn(s, reject); }

/* ── Duplicate (allocates through allocator) ────────────────── */
char *scl_strdup(scl_allocator_t *alloc, const char *src) {
    if (!src) return NULL;
    size_t n = scl_strlen(src);
    char *d = (char *)scl_alloc(alloc, n + 1, _Alignof(max_align_t));
    if (!d) return NULL;
    scl_memcpy(d, src, n + 1);
    return d;
}

/* ── Token ──────────────────────────────────────────────────── */

char *scl_strtok(char *str, const char *delim) {
    return strtok(str, delim);
}
