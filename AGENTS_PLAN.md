# AGENTS.md Implementation Plan

This plan maps every mechanical-sympathy principle from AGENTS.md to concrete
code changes, prioritized by impact and correctness confidence. Steps are ordered
so earlier changes enable later ones (e.g., power-of-2 deque → branchless wrap).

---

## Step 1: Cache-Line Padding for Concurrent Structures

**AGENTS.md roots:** §"Cache-Line Padding & False Sharing" (line 11),  
§"Application to This Codebase" (line 89): *"Align atomic fields to cache-line
boundaries to prevent false sharing."*

**Rationale:** When two threads write to different atomic fields on the same
cache line, MESI forces ~10k× throughput drop. Every concurrent struct that
has >1 atomic field without cache-line separation is a performance bug.

**Strategy:** In each `.h` struct typedef, group hot atomics onto their own
cache lines using `SCL_CACHE_ALIGNED` (defined in `scl_common.h` as
`__attribute__((aligned(SCL_CACHE_LINE_SIZE)))`). Use explicit `_padN` arrays
to push trailing atomics to the next line.

### Structures to fix (20 files):

#### Lock-free structures (CRITICAL: no spinlock, true concurrent writes)

| Struct | Atomics | Adjacent risk | Fix |
|--------|---------|---------------|-----|
| `scl_concurrent_queue_t` | `head`(uptr), `tail`(uptr), `count`(sz) | head+tail adjacent | Split into: read-only header → pad0 → `head` aligned → pad1 → `tail` aligned → pad2 → `count` aligned |
| `scl_concurrent_stack_t` | `top`(dwcas), `count`(sz) | top+count adjacent | Split: header → pad0 → `top` aligned → pad1 → `count` aligned |
| `scl_concurrent_slist_t` | `head`(dwcas), `count`(sz) | head+count adjacent | Split: header → pad0 → `head` aligned → pad1 → `count` aligned |
| `scl_lfstack_t` | `head`(dwcas) | Single, but only `_Alignas(16)` | Change `_Alignas(16)` to `SCL_CACHE_ALIGNED` |
| `scl_concurrent_skiplist_node_t` | `level`(sz) | Heap-adjacent | Add padding so full node is cache-line-sized |

#### Spinlock-guarded structures (MEDIUM: lock serializes, but still bounce)

| Struct | Atomics | Adjacent risk | Fix |
|--------|---------|---------------|-----|
| `scl_concurrent_deque_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_dlist_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_heap_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_graph_t` | `edge_count`(sz), `lock`(flag) | edge_count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_skiplist_t` | `count`(sz), `level`(sz), `lock`(flag) | count+level+lock clustered | Split each to own line |
| `scl_concurrent_avl_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_bst_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_btree_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_rbtree_t` | `count`(sz), `lock`(flag) | count+lock adjacent | Pad `lock` to its own line |
| `scl_concurrent_lru_t` | `count`(sz), `lock`(flag) | Far apart, low risk | Still pad `lock` for safety |
| `scl_concurrent_hash_t` | `count`(sz) | shares with bucket_count etc | Pad `count` to its own line |

#### Other atomics

| Struct | Atomics | Adjacent risk | Fix |
|--------|---------|---------------|-----|
| `scl_concurrent_array_t` | `count`(sz) | shares with capacity | Pad `count` to its own line |
| `scl_concurrent_unionfind_t` | `sets`(sz) | shares with count | Pad `sets` to its own line |

### Design pattern for padded structs:

```c
typedef struct {
    /* Read-mostly header fields: threads read but rarely write */
    unsigned char *data;
    size_t element_size;
    size_t capacity;
    char _pad0[SCL_CACHE_LINE_SIZE - 3 * sizeof(size_t) - sizeof(void *)];

    /* Consumer-writes (head) */
    atomic_size_t head SCL_CACHE_ALIGNED;
    char _pad1[SCL_CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    /* Producer-writes (tail) */
    atomic_size_t tail SCL_CACHE_ALIGNED;
    char _pad2[SCL_CACHE_LINE_SIZE - sizeof(atomic_size_t)];

    /* Shared (count) */
    atomic_size_t count SCL_CACHE_ALIGNED;
} scl_concurrent_queue_t;
```

**NOTE:** The `_padN` sizing depends on exact field sizes. Each struct will
be computed individually.

---

## Step 2: Deque Power-of-2 + Mask (Remove Modulo)

**AGENTS.md roots:** §"Bespoke Data Structures" line 22: *"Power-of-two sizing
enables bitwise masking (`index & (size-1)`)"*,  
§"Loop Strength Reduction" line 64: *"Use power-of-two sizing so modulo
becomes bitwise AND"*.

**Rationale:** `scl_deque.c` and `scl_concurrent_deque.c` use `% deque->capacity`
in 9 locations. The deque never rounds capacity to a power of two. Every
wrap-around is an expensive `idiv` instruction (~20-40 cycles).

### Changes:

**`libs/structures/deque/scl_deque.h`:**
- Add `size_t mask;` field after `capacity`
- Round `initial_capacity` to power-of-2 in `scl_deque_init`
- Set `deque->mask = deque->capacity - 1`

**`libs/structures/deque/scl_deque.c`:** (6 sites)
- `(deque->head + i) % deque->capacity` → `(deque->head + i) & deque->mask`
- `(deque->head + deque->count) % deque->capacity` → `(...) & deque->mask`
- `(deque->head + 1) % deque->capacity` → `(...) & deque->mask`
- `(deque->head + deque->count - 1) % deque->capacity` → `(...) & deque->mask`
- Keep the branchless `if (head == 0) head = cap-1 else head-1` pattern but it
  becomes `deque->head = (deque->head - 1) & deque->mask` (branchless, see Step 6)

**`libs/structures/deque/scl_concurrent_deque.h`:**
- Add `size_t mask;` field
- Set in init after rounding capacity

**`libs/structures/deque/scl_concurrent_deque.c`:** (3 sites)
- Same modulo→mask conversions

---

## Step 3: Concurrent Hash Power-of-2 + Mask

**Same AGENTS.md roots as Step 2.**

**Rationale:** `scl_concurrent_hash.c` uses `% ht->bucket_count` in 4 locations.
The hash map takes an arbitrary `bucket_count` from the caller and never
rounds it.

### Changes:

**`libs/structures/hash/scl_concurrent_hash.h`:**
- Add `size_t mask;` field

**`libs/structures/hash/scl_concurrent_hash.c`:**
- In `scl_chash_init`: round `bucket_count` up to power-of-2 via
  `scl_bit_ceil_sz`, compute `mask = bucket_count - 1`
- Replace all 4 `% ht->bucket_count` with `& ht->mask`
- Since the bucket array size uses bucket_count, power-of-2 rounding also
  means buckets are power-of-2 sized

---

## Step 4: Hash Search Power-of-2 + Mask

**Same AGENTS.md roots.**

**Rationale:** `scl_hash_search.c` uses `% cap`/`% ht->capacity` in 4 locations.

### Changes:

**`libs/search/hash/scl_hash_search.h`:**
- Add `size_t mask;` field

**`libs/search/hash/scl_hash_search.c`:**
- In init: round capacity to power-of-2, store mask
- Replace 4 `%` with `& mask`

---

## Step 5: CSE / Scalar Replacement

**AGENTS.md roots:** §"Global Value Numbering & CSE" (line 56),  
§"Scalar Replacement of Aggregates" (line 62),  
§"Alias Analysis & Load-Store Disambiguation" (line 66).

**Rationale:** Repeated struct field dereferences force the compiler to
reload from memory (cannot assume values haven't changed through aliasing
pointers). Hoisting to locals guarantees the load happens once and stays
in a register.

### 5a. `scl_concurrent_hash.c` — `scl_chash_insert()`

Current (approximate):
```c
ht->key_size       // line 71 (memcmp)
ht->value_size     // line 72
ht->key_size       // line 80 (alloc)
ht->value_size     // line 81 (alloc)
ht->key_size       // line 87 (memcpy)
ht->value_size     // line 88 (memcpy)
ht->buckets        // line 68, 73, 84, 92
```

Fix: hoist at function top:
```c
size_t ksz = ht->key_size;
size_t vsz = ht->value_size;
scl_concurrent_hash_bucket_t *buckets = ht->buckets;
```

### 5b. `scl_deque.c` — `scl_deque_grow()`

`deque->element_size` accessed 4 times. Hoist to `size_t es = deque->element_size;`.

### 5c. `scl_array.c` — `scl_array_get/set/remove()`

`arr->element_size` accessed 2-3 times per function. Hoist.

---

## Step 6: Cold-Path Outlining

**AGENTS.md roots:** §"Basic Block Linearization & Cold-Path Outlining" (line 60).

**Rationale:** Moving infrequent (error/resize) code paths to separate
`SCL_COLD_PATH` functions keeps I-cache saturated with hot code.

### Changes:

- Mark `scl_hash_grow` — already done (example)
- Mark resize/grow functions in:
  - `scl_array.c`: `scl_array_grow`
  - `scl_deque.c`: `scl_deque_grow`
  - `scl_stack.c`: `scl_stack_grow`
  - `scl_concurrent_array.c`: `scl_carray_grow`

---

## Step 7: Branchless Deque Head Wrap

**AGENTS.md roots:** §"Branchless Execution" (line 32): *"Replace conditional
logic with arithmetic masks, bitwise operations"*.

**Rationale:** `deque->head = (deque->head == 0) ? deque->capacity - 1 : deque->head - 1`
is a branch (or predicated compare) that can be replaced with
`deque->head = (deque->head - 1) & deque->mask` when capacity is power-of-2.
This is branchless and works because `(0 - 1) & mask = mask = capacity - 1`.

**Changes (only after Step 2 is done):**
- Replace the conditional in `scl_deque_push_front` (line 74) and
  `scl_deque_pop_front` if applicable
- Also apply to `scl_concurrent_deque_push_front`

---

## Step 8: CSE in CSV Parser (if worthwhile)

**AGENTS.md roots:** Same as Step 5.

`parser->pos` accessed 15+ times, `parser->buffer` 8+ times in
`scl_parse_csv_next_field`. If this is a hot path, hoisting could help.

Evaluate: is CSV parsing performance-critical? The parser has a complex
state machine and the struct field accesses are in a while loop. This is
worth doing.

---

## Execution Order

Each step MUST:
1. Be applied independently to the relevant files
2. Be followed by `make clean && make` to verify compilation with
   `-Wall -Wextra -Wpedantic -Werror`
3. Be followed by test run to verify all 55 tests pass

Step dependencies:
- Step 7 depends on Step 2 (needs deque mask field)
- All other steps are independent

Final order:
1. Step 1: Cache-line padding (20 header files)
2. Build + test
3. Step 2: Deque power-of-2 + mask (4 files)
4. Build + test
5. Step 3: Concurrent hash power-of-2 (2 files)
6. Build + test
7. Step 4: Hash search power-of-2 (2 files)
8. Build + test
9. Step 5: CSE hoisting (3-4 files)
10. Build + test
11. Step 6: Cold-path outlining (3-4 files)
12. Build + test
13. Step 7: Branchless deque head wrap (2 files)
14. Build + test
15. Step 8: CSV parser CSE (1 file)
16. Build + test
