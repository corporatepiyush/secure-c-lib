#include "scl_concurrent_fenwick.h"
#include "scl_string.h"

static inline void spin_lock(atomic_flag *lock)
{
    while (atomic_flag_test_and_set_explicit(lock, memory_order_acquire))
        scl_cpu_pause();
}

static inline void spin_unlock(atomic_flag *lock)
{
    atomic_flag_clear_explicit(lock, memory_order_release);
}

scl_error_t scl_cfenwick_init(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw,
                              size_t n, size_t element_size, const void *data,
                              void (*add)(void *out, const void *a, const void *b),
                              void (*sub)(void *out, const void *a, const void *b))
{
    if (!fw || !add || !sub || !data) return SCL_ERR_NULL_PTR;
    if (n == 0 || element_size == 0) return SCL_ERR_INVALID_ARG;

    fw->tree = scl_calloc(alloc, n + 1, element_size, alignof(max_align_t));
    fw->scratch = scl_alloc(alloc, element_size, alignof(max_align_t));
    if (!fw->tree || !fw->scratch) {
        scl_free(alloc, fw->tree);
        scl_free(alloc, fw->scratch);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    fw->n = n;
    fw->element_size = element_size;
    fw->add = add;
    fw->sub = sub;
    atomic_flag_clear(&fw->lock);

    const unsigned char *src = data;
    for (size_t i = 1; i <= n; i++)
        scl_memcpy(fw->tree + i * element_size, src + (i - 1) * element_size, element_size);

    for (size_t i = 1; i <= n; i++) {
        size_t j = i + (i & -i);
        if (j <= n)
            add(fw->tree + j * element_size,
                fw->tree + j * element_size,
                fw->tree + i * element_size);
    }

    return SCL_OK;
}

void scl_cfenwick_destroy(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw)
{
    if (!fw) return;
    scl_free(alloc, fw->tree);
    scl_free(alloc, fw->scratch);
    fw->tree = NULL;
    fw->scratch = NULL;
    fw->n = 0;
}

scl_error_t scl_cfenwick_update(scl_allocator_t *alloc, scl_concurrent_fenwick_t *fw,
                                size_t idx, const void *delta)
{
    (void)alloc;
    if (!fw || !delta) return SCL_ERR_NULL_PTR;
    if (idx >= fw->n) return SCL_ERR_INVALID_INDEX;

    spin_lock(&fw->lock);

    size_t esize = fw->element_size;
    size_t i = idx + 1;

    while (i <= fw->n) {
        fw->add(fw->tree + i * esize, fw->tree + i * esize, delta);
        i += i & -i;
    }

    spin_unlock(&fw->lock);
    return SCL_OK;
}

scl_error_t scl_cfenwick_prefix(const scl_concurrent_fenwick_t *fw, size_t idx, void *out)
{
    if (!fw || !out) return SCL_ERR_NULL_PTR;
    if (idx >= fw->n) return SCL_ERR_INVALID_INDEX;

    spin_lock((atomic_flag *)&fw->lock);

    size_t esize = fw->element_size;
    size_t i = idx + 1;
    bool first = true;
    unsigned char *acc = out;

    while (i > 0) {
        if (first) {
            scl_memcpy(acc, fw->tree + i * esize, esize);
            first = false;
        } else {
            fw->add(acc, acc, fw->tree + i * esize);
        }
        i -= i & -i;
    }

    spin_unlock((atomic_flag *)&fw->lock);
    return first ? SCL_ERR_EMPTY : SCL_OK;
}

scl_error_t scl_cfenwick_range_query(const scl_concurrent_fenwick_t *fw, size_t l, size_t r, void *out)
{
    if (!fw || !out) return SCL_ERR_NULL_PTR;
    if (l > r || r >= fw->n) return SCL_ERR_INVALID_ARG;

    spin_lock((atomic_flag *)&fw->lock);

    scl_error_t err = scl_cfenwick_prefix(fw, r, out);
    if (err != SCL_OK) { spin_unlock((atomic_flag *)&fw->lock); return err; }

    if (l > 0) {
        err = scl_cfenwick_prefix(fw, l - 1, fw->scratch);
        if (err != SCL_OK) { spin_unlock((atomic_flag *)&fw->lock); return err; }
        fw->sub(out, out, fw->scratch);
    }

    spin_unlock((atomic_flag *)&fw->lock);
    return SCL_OK;
}
