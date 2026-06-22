#ifndef SCL_COMMON_H
#define SCL_COMMON_H

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <limits.h>
#include <errno.h>

#ifdef __GNUC__
#define SCL_WARN_UNUSED __attribute__((warn_unused_result))
#define SCL_NONNULL(...) __attribute__((nonnull(__VA_ARGS__)))
#define SCL_UNUSED __attribute__((unused))
#else
#define SCL_WARN_UNUSED
#define SCL_NONNULL(...)
#define SCL_UNUSED
#endif

#define SCL_CONTAINER_OF(ptr, type, member) \
    ((type *)((unsigned char *)(ptr) - offsetof(type, member)))

typedef enum {
    SCL_OK = 0,
    SCL_ERR_NULL_PTR,
    SCL_ERR_OUT_OF_MEMORY,
    SCL_ERR_SIZE_OVERFLOW,
    SCL_ERR_EMPTY,
    SCL_ERR_NOT_FOUND,
    SCL_ERR_FULL,
    SCL_ERR_INVALID_INDEX,
    SCL_ERR_INVALID_ARG,
    SCL_ERR_DUPLICATE,
    SCL_ERR_ALLOC,
    SCL_ERR_INVALID_STATE,
    SCL_ERR_NOT_IMPLEMENTED
} scl_error_t;

static inline bool scl_add_overflow(size_t a, size_t b, size_t *out)
{
    if (a > SIZE_MAX - b) return true;
    *out = a + b;
    return false;
}

static inline bool scl_mul_overflow(size_t a, size_t b, size_t *out)
{
    if (a > 0 && b > SIZE_MAX / a) return true;
    *out = a * b;
    return false;
}

#ifndef SCL_CMP_FUNC_T_DEFINED
#define SCL_CMP_FUNC_T_DEFINED
typedef int (*scl_cmp_func_t)(const void *, const void *);
#endif

#ifndef SCL_GRAPH_TYPES_DEFINED
#define SCL_GRAPH_TYPES_DEFINED

typedef struct scl_adj_node {
    size_t to;
    int weight;
    struct scl_adj_node *next;
} scl_adj_node_t;

typedef struct {
    scl_adj_node_t **adj;
    size_t vertex_count;
    size_t edge_count;
} scl_graph_t;

typedef struct {
    size_t from;
    size_t to;
    int weight;
} scl_edge_t;

#endif

#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#endif
