#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O3", "unroll-loops", "tree-vectorize", "inline")
#endif

#include "scl_alloc_arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static size_t scl_page_size(void) {
#ifdef _WIN32
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwPageSize;
#else
    return (size_t)sysconf(_SC_PAGESIZE);
#endif
}

static void *scl_mmap_alloc(size_t size) {
#ifdef _WIN32
    return VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (p == MAP_FAILED) return NULL;
    return p;
#endif
}

static void scl_mmap_free(void *ptr, size_t size) {
#ifdef _WIN32
    VirtualFree(ptr, 0, MEM_RELEASE);
#else
    munmap(ptr, size);
#endif
}

static inline size_t scl_align_up_pow2(size_t size, size_t align) {
    return (size + align - 1) & ~(align - 1);
}

scl_error_t scl_alloc_arena_init(scl_alloc_arena_t *arena, size_t capacity) {
    if (__builtin_expect(!arena, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(capacity == 0, 0)) return SCL_ERR_INVALID_ARG;

    size_t page_sz = scl_page_size();
    size_t alloc_size = scl_align_up_pow2(capacity, page_sz);
    if (alloc_size < capacity) return SCL_ERR_SIZE_OVERFLOW;

    void *buf = scl_mmap_alloc(alloc_size);
    if (__builtin_expect(!buf, 0)) return SCL_ERR_OUT_OF_MEMORY;

    arena->buffer = (char *)buf;
    arena->offset = 0;
    arena->capacity = alloc_size;
    arena->next = NULL;
    return SCL_OK;
}

scl_error_t scl_alloc_arena_alloc(scl_alloc_arena_t *arena, size_t size, size_t alignment, void **out_ptr) {
    if (__builtin_expect(!arena, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(!out_ptr, 0)) return SCL_ERR_NULL_PTR;
    if (__builtin_expect(size == 0, 0)) return SCL_ERR_INVALID_ARG;
    if (__builtin_expect(alignment == 0, 0)) alignment = alignof(max_align_t);

    size_t align_mask = alignment - 1;
    size_t aligned_offset = (arena->offset + align_mask) & ~align_mask;

    if (__builtin_expect(aligned_offset > arena->capacity || size > arena->capacity - aligned_offset, 0)) {
        size_t new_cap = arena->capacity * 2;
        while (new_cap < size + alignment) new_cap *= 2;
        scl_alloc_arena_t *new_arena;
        new_arena = (scl_alloc_arena_t *)malloc(sizeof(scl_alloc_arena_t));
        if (__builtin_expect(!new_arena, 0)) return SCL_ERR_OUT_OF_MEMORY;
        scl_error_t err = scl_alloc_arena_init(new_arena, new_cap);
        if (__builtin_expect(err != SCL_OK, 0)) {
            free(new_arena);
            return err;
        }
        new_arena->next = arena->next;
        arena->next = new_arena;
        arena = new_arena;
        aligned_offset = 0;
    }

    *out_ptr = arena->buffer + aligned_offset;
    arena->offset = aligned_offset + size;
    return SCL_OK;
}

scl_error_t scl_alloc_arena_reset(scl_alloc_arena_t *arena) {
    if (__builtin_expect(!arena, 0)) return SCL_ERR_NULL_PTR;
    arena->offset = 0;
    scl_alloc_arena_t *cur = arena->next;
    while (cur) {
        cur->offset = 0;
        cur = cur->next;
    }
    return SCL_OK;
}

scl_error_t scl_alloc_arena_destroy(scl_alloc_arena_t *arena) {
    if (__builtin_expect(!arena, 0)) return SCL_ERR_NULL_PTR;
    scl_alloc_arena_t *cur = arena;
    while (cur) {
        scl_alloc_arena_t *next = cur->next;
        if (cur->buffer) scl_mmap_free(cur->buffer, cur->capacity);
        if (cur != arena) free(cur);
        cur = next;
    }
    arena->buffer = NULL;
    arena->offset = 0;
    arena->capacity = 0;
    arena->next = NULL;
    return SCL_OK;
}
