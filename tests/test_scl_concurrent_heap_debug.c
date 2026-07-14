#include "scl_concurrent_heap.h"
#include "scl_test.h"
#include <stdio.h>

static int int_cmp_min(const void *a, const void *b) {
  int va = *(int *)a, vb = *(int *)b;
  return (va > vb) - (va < vb);
}

int main(void) {
  scl_allocator_t *alloc = scl_allocator_default();
  scl_concurrent_heap_t h;
  (void)scl_cheap_init(alloc, &h, sizeof(int), 16, int_cmp_min);

  int vals[] = {5, 3, 7, 1, 9, 2, 8};
  for (int i = 0; i < 7; i++) {
    (void)scl_cheap_push(alloc, &h, &vals[i]);
    printf("After push %d: count=%zu\n", vals[i], scl_cheap_count(&h));
    for (size_t j = 0; j < scl_cheap_count(&h); j++)
      printf("  [%zu] = %d\n", j, ((int *)h.data)[j]);
  }

  printf("\nPops:\n");
  for (int i = 0; i < 7; i++) {
    int out;
    (void)scl_cheap_pop(alloc, &h, &out);
    printf("Pop: %d, count=%zu\n", out, scl_cheap_count(&h));
    for (size_t j = 0; j < scl_cheap_count(&h); j++)
      printf("  [%zu] = %d\n", j, ((int *)h.data)[j]);
  }

  scl_cheap_destroy(alloc, &h);
  return 0;
}
