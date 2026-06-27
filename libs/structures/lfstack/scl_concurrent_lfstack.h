/*
 * Copyright 2026 Piyush Katariya
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Lock-free Treiber stack with DWCAS. ABA-safe via tagged pointer. Multi-producer/multi-consumer. O(1) push/pop. */

#ifndef SCL_CONCURRENT_LFSTACK_H
#define SCL_CONCURRENT_LFSTACK_H

/*
 * scl_lfstack — lock-free intrusive Treiber stack (ABA-safe).
 *
 * Realized over scl_dwcas (a genuine lock-free 16-byte CAS on the {ptr,tag}
 * head; see scl_atomic.h for why the portable __atomic spelling is not).
 *
 * The classic Treiber hazard is that pop() dereferences the head node's
 * `next`, which can race with another thread freeing that node. This API is
 * therefore specified for the *pool* pattern: nodes live in a fixed
 * allocation for the lifetime of the stack and are only recycled between
 * push/pop — never free()d while the stack is in use. Under that invariant
 * the dereference is always valid and the tag defeats ABA, so the structure
 * is fully correct and lock-free.
 *
 * Each node must begin with a `uintptr_t` slot the stack uses as its link.
 */

#include "scl_common.h"
#include "scl_atomic.h"

typedef struct {
    scl_dwcas_t head SCL_CACHE_ALIGNED;  /* .lo = node ptr, .hi = ABA tag */
} scl_lfstack_t;

static inline void scl_lfstack_init(scl_lfstack_t *s) {
    s->head.lo = 0;
    s->head.hi = 0;
}

static inline bool scl_lfstack_empty(scl_lfstack_t *s) {
    return scl_dwcas_load(&s->head).lo == 0;
}

static inline void scl_lfstack_push(scl_lfstack_t *s, void *node) {
    scl_dwcas_t old = scl_dwcas_load(&s->head);
    scl_dwcas_t neu;
    do {
        /* The link is touched atomically: a concurrent pop may read this very
         * node's next field while we write it (it will then fail its CAS and
         * retry), so a plain access would be a C11 data race. Relaxed is
         * enough — publication is ordered by the release in scl_dwcas. */
        __atomic_store_n((uintptr_t *)node, old.lo, __ATOMIC_RELAXED);
        neu.lo = (uintptr_t)node;
        neu.hi = old.hi + 1;                  /* bump tag (ABA guard) */
    } while (!scl_dwcas(&s->head, &old, neu));
}

static inline void *scl_lfstack_pop(scl_lfstack_t *s) {
    scl_dwcas_t old = scl_dwcas_load(&s->head);
    scl_dwcas_t neu;
    do {
        if (old.lo == 0) return NULL;
        /* Read head->next atomically (pool invariant: memory always valid).
         * A stale value is harmless — the tagged CAS below rejects it. */
        neu.lo = __atomic_load_n((uintptr_t *)old.lo, __ATOMIC_RELAXED);
        neu.hi = old.hi + 1;
    } while (!scl_dwcas(&s->head, &old, neu));
    return (void *)old.lo;
}

#endif /* SCL_CONCURRENT_LFSTACK_H */
