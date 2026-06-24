#include "scl_concurrent_stack.h"
#include "scl_string.h"

scl_error_t scl_cstack_init(scl_allocator_t *alloc, scl_concurrent_stack_t *stack, size_t element_size)
{
    (void)alloc;
    if (scl_unlikely(!stack)) return SCL_ERR_NULL_PTR;
    if (scl_unlikely(element_size == 0)) return SCL_ERR_INVALID_ARG;
    atomic_init(&stack->top, scl_tagged_make(NULL, 0));
    atomic_init(&stack->count, 0);
    stack->element_size = element_size;
    return SCL_OK;
}

void scl_cstack_destroy(scl_allocator_t *alloc, scl_concurrent_stack_t *stack)
{
    if (scl_unlikely(!stack)) return;
    scl_tagged_ptr_t tp = atomic_load_explicit(&stack->top, memory_order_acquire);
    scl_concurrent_stack_node_t *cur = (scl_concurrent_stack_node_t *)tp.ptr;
    while (scl_likely(cur)) {
        scl_concurrent_stack_node_t *next = cur->next;
        scl_free(alloc, cur->data);
        scl_free(alloc, cur);
        cur = next;
    }
    atomic_store_explicit(&stack->top, scl_tagged_make(NULL, 0), memory_order_relaxed);
    atomic_store_explicit(&stack->count, 0, memory_order_relaxed);
}

scl_error_t scl_cstack_push(scl_allocator_t *alloc, scl_concurrent_stack_t *stack, const void  *SCL_RESTRICT element)
{
    if (scl_unlikely(!stack || !element)) return SCL_ERR_NULL_PTR;
    scl_concurrent_stack_node_t *node = scl_alloc(alloc, sizeof(scl_concurrent_stack_node_t), alignof(max_align_t));
    if (scl_unlikely(!node)) return SCL_ERR_OUT_OF_MEMORY;
    node->data = scl_alloc(alloc, stack->element_size, alignof(max_align_t));
    if (scl_unlikely(!node->data)) {
        scl_free(alloc, node);
        return SCL_ERR_OUT_OF_MEMORY;
    }
    scl_memcpy(node->data, element, stack->element_size);

    scl_tagged_ptr_t old = atomic_load_explicit(&stack->top, memory_order_relaxed);
    do {
        node->next = old.ptr;
    } while (!atomic_compare_exchange_weak_explicit(&stack->top, &old,
             scl_tagged_make(node, old.tag + 1),
             memory_order_release, memory_order_relaxed));

    atomic_fetch_add_explicit(&stack->count, 1, memory_order_relaxed);
    return SCL_OK;
}

scl_error_t scl_cstack_pop(scl_allocator_t *alloc, scl_concurrent_stack_t *stack, void  *SCL_RESTRICT out)
{
    if (scl_unlikely(!stack || !out)) return SCL_ERR_NULL_PTR;
    scl_tagged_ptr_t old = atomic_load_explicit(&stack->top, memory_order_relaxed);
    while (1) {
        if (scl_unlikely(!old.ptr)) return SCL_ERR_EMPTY;
        scl_concurrent_stack_node_t *next = ((scl_concurrent_stack_node_t *)old.ptr)->next;
        if (atomic_compare_exchange_weak_explicit(&stack->top, &old,
                scl_tagged_make(next, old.tag + 1),
                memory_order_acquire, memory_order_relaxed))
            break;
    }
    scl_memcpy(out, ((scl_concurrent_stack_node_t *)old.ptr)->data, stack->element_size);
    scl_free(alloc, ((scl_concurrent_stack_node_t *)old.ptr)->data);
    scl_free(alloc, (scl_concurrent_stack_node_t *)old.ptr);
    atomic_fetch_sub_explicit(&stack->count, 1, memory_order_relaxed);
    return SCL_OK;
}

size_t scl_cstack_count(const scl_concurrent_stack_t *stack)
{
    return stack ? atomic_load_explicit(&stack->count, memory_order_relaxed) : 0;
}

bool scl_cstack_empty(const scl_concurrent_stack_t *stack)
{
    scl_tagged_ptr_t tp;
    if (scl_unlikely(!stack)) return true;
    tp = atomic_load_explicit(&stack->top, memory_order_relaxed);
    return tp.ptr == NULL;
}
