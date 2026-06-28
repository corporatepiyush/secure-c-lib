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

/* DDoS protection: token-bucket rate limiter, IP black/whitelist via Patricia trie, LRU grey-list for adaptive scoring. */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize ("O2")
#endif

/*
 * scl_net_ddos.c — DDoS mitigation implementation.
 *
 * ── Architecture ─────────────────────────────────────────────────────────────
 *
 * The module tracks every source IP that connects to the server using a
 * hash table. Each entry stores:
 *
 *   • IP address (128 bits for IPv6, 32 bits for IPv4)
 *   • Token bucket: current tokens and last-refill timestamp (ms)
 *   • Concurrent connection count
 *   • Total drop count for this IP
 *   • Ban timestamp (if currently banned)
 *   • Whitelist flag
 *
 * The hash table uses open addressing with linear probing. It has a fixed
 * number of slots (configurable) — entries are not dynamically allocated
 * per-connection. When all slots are full, a new connection replaces the
 * least-recently-used (LRU) entry. This bounds memory.
 *
 * ── Token bucket algorithm ──────────────────────────────────────────────────
 *
 * Each IP has a bucket that can hold up to `burst_size` tokens. Tokens
 * are added at `rate_per_sec` per second. Each connection consumes one
 * token. If the bucket is empty, the connection is dropped.
 *
 * This limits the sustained rate while allowing short bursts.
 *
 * ── Why this approach? ──────────────────────────────────────────────────────
 *
 * SYN flood mitigation belongs in the kernel (SYN cookies, tcp_syncookies).
 * Application-level DDoS mitigation targets HTTP-level floods — clients
 * that complete the TCP handshake but send excessive requests. The token
 * bucket catches these: a legitimate client opening a few dozen connections
 * per second is fine; a botnet opening thousands per second is blocked.
 */

#include "scl_net_ddos.h"
#include "scl_string.h"
#include "scl_stdlib.h"
#include "scl_time.h"
#include "scl_atomic.h"
#include "scl_concurrent_common.h"   /* scl_spinlock_t */

#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>

/* ── Internal constants ─────────────────────────────────────────── */
#define SCL_DDOS_EMPTY   0xFFFFFFFFu  /* slot marker: empty */
#define SCL_DDOS_BANNED  0xFFFFFFFEu  /* slot marker: banned IP */
#define SCL_DDOS_IPV4_SLOT 0          /* indicates an IPv4 address in slot */

/* ── Per-IP state slot ────────────────────────────────────────────
 * 56 bytes per slot; with 1024 slots = 56 KB. */
typedef struct {
    /* Normalized key: IPv4 = 4 bytes; IPv6 = /64 prefix (first 8 bytes,
     * remainder zeroed). Tracking IPv6 per /64 prevents an attacker who
     * controls a routed prefix from trivially evading per-IP limits by
     * cycling through 2^64 distinct /128 addresses. */
    union {
        struct in_addr  v4;
        struct in6_addr v6;
    } ip;
    int      family;         /* AF_INET or AF_INET6, or -1 for empty */
    uint32_t hash;           /* cached hash */

    /* Token bucket */
    unsigned long tokens;       /* current token count (fixed-point) */
    int64_t       last_refill;  /* last refill timestamp (ms) */
    int64_t       last_seen;    /* last activity (ms) — drives LRU eviction */
    unsigned int  conn_count;   /* current concurrent connections */
    unsigned long total_drops;  /* total drops for this IP */

    /* Ban state */
    int64_t  ban_until;        /* 0 = not banned, INT64_MAX = manual blacklist */
    bool     whitelisted;      /* bypass all checks */
} scl_ddos_slot_t;

/* ── Main state ─────────────────────────────────────────────────── */
struct scl_ddos {
    scl_allocator_t   *alloc;
    scl_ddos_config_t  cfg;

    scl_ddos_slot_t   *slots;
    unsigned int       slot_count;

    /* Serializes all table mutation. The acceptor calls check() while workers
     * call conn_closed(); without this lock those races corrupt conn_count and
     * the token buckets. The fast path is uncontended (single acceptor). */
    scl_spinlock_t     lock;

    /* Stats (relaxed; read without the lock). */
    scl_atomic_size_t  total_allowed;
    scl_atomic_size_t  total_dropped;
};

/* Normalize a raw address into the table key: IPv4 keeps all 4 bytes; IPv6 is
 * collapsed to its /64 prefix (first 8 bytes, rest zeroed). `klen` receives the
 * significant key length. */
static void ip_key(int family, const void *addr, unsigned char out[16], size_t *klen) {
    if (scl_likely(family == AF_INET)) {
        scl_memcpy(out, addr, 4);
        *klen = 4;
    } else {
        scl_memcpy(out, addr, 8);
        scl_memset(out + 8, 0, 8);   /* mask off the host portion of the /64 */
        *klen = 16;
    }
}

/* ── Hash function (djb2 on the normalized key) ─────────────────── */
static uint32_t ip_hash(const unsigned char *key, size_t klen) {
    uint32_t h = 5381;
    for (size_t i = 0; i < klen; i++)
        h = ((h << 5) + h) + key[i];
    return h;
}

/* A slot is evictable only if it holds no live connections and is not a pinned
 * (whitelisted or manually blacklisted) entry — evicting those would silently
 * drop an operator's policy. */
static bool slot_evictable(const scl_ddos_slot_t *s) {
    return s->conn_count == 0 && !s->whitelisted && s->ban_until != INT64_MAX;
}

static void slot_init(scl_ddos_slot_t *s, const scl_ddos_t *ddos, int family,
                      uint32_t h, const unsigned char *key, size_t klen,
                      int64_t now) {
    s->family      = family;
    s->hash        = h;
    s->tokens      = ddos->cfg.burst_size;
    s->last_refill = now;
    s->last_seen   = now;
    s->conn_count  = 0;
    s->total_drops = 0;
    s->ban_until   = 0;
    s->whitelisted = false;
    scl_memcpy(&s->ip, key, klen);
}

/*
 * Find or allocate a slot for a normalized key. Returns slot index or
 * UINT32_MAX.
 *
 * On a full table we do NOT fail open: we evict the least-recently-seen
 * evictable slot and reuse it. This is the crux of the module's value — an
 * attacker spraying many distinct source addresses must not be able to fill
 * the table and thereby disable rate limiting for everyone. Eviction-by-
 * replacement keeps the slot occupied, so open-addressing probe chains for
 * surviving keys stay intact.
 */
static unsigned int find_slot(scl_ddos_t *ddos, int family,
                              const unsigned char *key, size_t klen,
                              bool alloc_if_missing, int64_t now) {
    uint32_t h = ip_hash(key, klen);
    unsigned int mask = ddos->slot_count - 1;
    unsigned int idx = h & mask;

    unsigned int victim = UINT32_MAX;
    int64_t      victim_seen = INT64_MAX;

    /* Single full sweep: locate an exact match or an empty slot, while also
     * tracking the best eviction candidate in case the table is full. */
    for (unsigned int i = 0; i < ddos->slot_count; i++) {
        unsigned int probe = (idx + i) & mask;
        scl_ddos_slot_t *s = &ddos->slots[probe];

        if (scl_unlikely(s->family < 0)) {
            if (scl_unlikely(!alloc_if_missing)) return UINT32_MAX;
            slot_init(s, ddos, family, h, key, klen, now);
            return probe;
        }

        if (scl_likely(s->hash == h && (size_t)(family == AF_INET ? 4 : 16) == klen &&
                       scl_memcmp(&s->ip, key, klen) == 0)) {
            return probe;
        }

        if (alloc_if_missing && slot_evictable(s) && s->last_seen < victim_seen) {
            victim_seen = s->last_seen;
            victim = probe;
        }
    }

    if (!alloc_if_missing || victim == UINT32_MAX)
        return UINT32_MAX;  /* table full and nothing safe to evict */

    /* Reclaim the LRU victim for this key. */
    scl_secure_zero(&ddos->slots[victim], sizeof(ddos->slots[victim]));
    slot_init(&ddos->slots[victim], ddos, family, h, key, klen, now);
    return victim;
}

/* ── Public API ─────────────────────────────────────────────────── */

scl_error_t scl_ddos_init(scl_allocator_t *alloc, scl_ddos_t **out,
                          const scl_ddos_config_t *cfg) {
    if (!alloc) alloc = scl_allocator_default();
    if (!out) return SCL_ERR_NULL_PTR;
    *out = NULL;

    scl_ddos_t *ddos = (scl_ddos_t *)scl_calloc(alloc, 1,
                           sizeof(scl_ddos_t), _Alignof(scl_ddos_t));
    if (!ddos) return SCL_ERR_OUT_OF_MEMORY;
    ddos->alloc = alloc;

    if (cfg) {
        ddos->cfg = *cfg;
    } else {
        ddos->cfg.rate_per_sec     = 100;
        ddos->cfg.burst_size       = 200;
        ddos->cfg.max_conn_per_ip  = 10;
        ddos->cfg.hash_slots       = 1024;
        ddos->cfg.ban_duration_ms  = 60000;
        ddos->cfg.ban_threshold    = 20;
    }

    /* Round up to power of 2 (for fast modulo). */
    unsigned int slots = ddos->cfg.hash_slots;
    if (slots < 64) slots = 64;
    unsigned int pow2 = 1;
    while (pow2 < slots) pow2 <<= 1;
    ddos->slot_count = pow2;
    ddos->cfg.hash_slots = pow2;

    ddos->slots = (scl_ddos_slot_t *)scl_calloc(alloc, pow2,
                       sizeof(scl_ddos_slot_t), _Alignof(scl_ddos_slot_t));
    if (!ddos->slots) {
        scl_free(alloc, ddos);
        return SCL_ERR_OUT_OF_MEMORY;
    }

    /* Mark all slots as empty. */
    for (unsigned int i = 0; i < pow2; i++)
        ddos->slots[i].family = -1;

    scl_spinlock_init(&ddos->lock);
    atomic_init(&ddos->total_allowed, 0);
    atomic_init(&ddos->total_dropped, 0);

    *out = ddos;
    return SCL_OK;
}

void scl_ddos_destroy(scl_ddos_t *ddos) {
    if (!ddos) return;
    if (ddos->slots) {
        scl_secure_zero(ddos->slots,
                        (size_t)ddos->slot_count * sizeof(scl_ddos_slot_t));
        scl_free(ddos->alloc, ddos->slots);
    }
    scl_free(ddos->alloc, ddos);
}

/* Extract the address pointer for a sockaddr; returns -1 for unknown families. */
static int sockaddr_ip(const struct sockaddr *addr, const void **out) {
    int family = addr->sa_family;
    if (scl_likely(family == AF_INET))
        *out = &((const struct sockaddr_in *)addr)->sin_addr;
    else if (family == AF_INET6)
        *out = &((const struct sockaddr_in6 *)addr)->sin6_addr;
    else
        return -1;
    return family;
}

static void bump_dropped(scl_ddos_t *ddos) {
    atomic_fetch_add_explicit(&ddos->total_dropped, 1, memory_order_relaxed);
}
static void bump_allowed(scl_ddos_t *ddos) {
    atomic_fetch_add_explicit(&ddos->total_allowed, 1, memory_order_relaxed);
}

bool scl_ddos_check(scl_ddos_t *ddos, const struct sockaddr *addr) {
    if (!ddos || !addr) return false;

    const void *ip_addr;
    int family = sockaddr_ip(addr, &ip_addr);
    if (scl_unlikely(family < 0)) return true; /* unknown family: allow */

    unsigned char key[16];
    size_t klen;
    ip_key(family, ip_addr, key, &klen);

    int64_t now = scl_now_ms();
    bool allow;

    scl_spinlock_lock(&ddos->lock);

    unsigned int idx = find_slot(ddos, family, key, klen, true, now);
    if (scl_unlikely(idx >= ddos->slot_count)) {
        /* Table is full of currently-active IPs and nothing is evictable.
         * Fail closed: every slot is an in-flight connection, so this is the
         * server already at capacity — dropping is correct back-pressure. */
        scl_spinlock_unlock(&ddos->lock);
        bump_dropped(ddos);
        return false;
    }

    scl_ddos_slot_t *s = &ddos->slots[idx];
    s->last_seen = now;

    /* Whitelist bypasses everything. */
    if (scl_unlikely(s->whitelisted)) {
        s->conn_count++;
        scl_spinlock_unlock(&ddos->lock);
        bump_allowed(ddos);
        return true;
    }

    /* Active ban? */
    if (scl_unlikely(s->ban_until > 0)) {
        if (scl_unlikely(now < s->ban_until)) {
            scl_spinlock_unlock(&ddos->lock);
            bump_dropped(ddos);
            return false;
        }
        s->ban_until = 0;     /* expired */
        s->total_drops = 0;
    }

    /* Token-bucket refill. */
    if (scl_likely(now > s->last_refill)) {
        unsigned long elapsed = (unsigned long)(now - s->last_refill);
        unsigned long add = (elapsed * ddos->cfg.rate_per_sec) / 1000;
        if (scl_likely(add > 0)) {
            unsigned long new_tokens = s->tokens + add;
            if (scl_unlikely(new_tokens > ddos->cfg.burst_size))
                new_tokens = ddos->cfg.burst_size;
            s->tokens = new_tokens;
            s->last_refill = now;
        }
    }

    /* Concurrent-connection cap or empty bucket => drop (and maybe ban). */
    if (scl_unlikely(s->conn_count >= ddos->cfg.max_conn_per_ip || s->tokens == 0)) {
        s->total_drops++;
        if (scl_unlikely(s->total_drops >= ddos->cfg.ban_threshold))
            s->ban_until = now + ddos->cfg.ban_duration_ms;
        scl_spinlock_unlock(&ddos->lock);
        bump_dropped(ddos);
        return false;
    }

    /* Admit: consume a token, count the connection. */
    s->tokens--;
    s->conn_count++;
    allow = true;
    scl_spinlock_unlock(&ddos->lock);
    bump_allowed(ddos);
    return allow;
}

void scl_ddos_conn_closed(scl_ddos_t *ddos, const struct sockaddr *addr) {
    if (!ddos || !addr) return;

    const void *ip_addr;
    int family = sockaddr_ip(addr, &ip_addr);
    if (family < 0) return;

    unsigned char key[16];
    size_t klen;
    ip_key(family, ip_addr, key, &klen);

    scl_spinlock_lock(&ddos->lock);
    unsigned int idx = find_slot(ddos, family, key, klen, false, scl_now_ms());
    if (idx < ddos->slot_count) {
        scl_ddos_slot_t *s = &ddos->slots[idx];
        if (s->conn_count > 0) s->conn_count--;
    }
    scl_spinlock_unlock(&ddos->lock);
}

/* ── IP string helpers ──────────────────────────────────────────── */

/* Parse "a.b.c.d" or "::1" into family + addr.
 * Returns AF_INET, AF_INET6, or -1 on error. */
static int parse_ip_str(const char *ip_str, void *addr_out) {
    if (!ip_str) return -1;
    /* Try IPv4. */
    struct in_addr v4;
    if (inet_pton(AF_INET, ip_str, &v4) == 1) {
        scl_memcpy(addr_out, &v4, sizeof(v4));
        return AF_INET;
    }
    /* Try IPv6. */
    struct in6_addr v6;
    if (inet_pton(AF_INET6, ip_str, &v6) == 1) {
        scl_memcpy(addr_out, &v6, sizeof(v6));
        return AF_INET6;
    }
    return -1;
}

scl_error_t scl_ddos_blacklist_ip(scl_ddos_t *ddos, const char *ip_str) {
    if (!ddos || !ip_str) return SCL_ERR_NULL_PTR;
    unsigned char raw[16];
    int family = parse_ip_str(ip_str, raw);
    if (family < 0) return SCL_ERR_INVALID_ARG;

    unsigned char key[16];
    size_t klen;
    ip_key(family, raw, key, &klen);

    scl_spinlock_lock(&ddos->lock);
    unsigned int idx = find_slot(ddos, family, key, klen, true, scl_now_ms());
    if (idx < ddos->slot_count) {
        ddos->slots[idx].ban_until   = INT64_MAX;  /* pinned: never evicted */
        ddos->slots[idx].whitelisted = false;
    }
    scl_spinlock_unlock(&ddos->lock);
    return idx < ddos->slot_count ? SCL_OK : SCL_ERR_FULL;
}

scl_error_t scl_ddos_whitelist_ip(scl_ddos_t *ddos, const char *ip_str) {
    if (!ddos || !ip_str) return SCL_ERR_NULL_PTR;
    unsigned char raw[16];
    int family = parse_ip_str(ip_str, raw);
    if (family < 0) return SCL_ERR_INVALID_ARG;

    unsigned char key[16];
    size_t klen;
    ip_key(family, raw, key, &klen);

    scl_spinlock_lock(&ddos->lock);
    unsigned int idx = find_slot(ddos, family, key, klen, true, scl_now_ms());
    if (idx < ddos->slot_count) {
        ddos->slots[idx].whitelisted = true;   /* pinned: never evicted */
        ddos->slots[idx].ban_until   = 0;
    }
    scl_spinlock_unlock(&ddos->lock);
    return idx < ddos->slot_count ? SCL_OK : SCL_ERR_FULL;
}

scl_error_t scl_ddos_unlist_ip(scl_ddos_t *ddos, const char *ip_str) {
    if (!ddos || !ip_str) return SCL_ERR_NULL_PTR;
    unsigned char raw[16];
    int family = parse_ip_str(ip_str, raw);
    if (family < 0) return SCL_ERR_INVALID_ARG;

    unsigned char key[16];
    size_t klen;
    ip_key(family, raw, key, &klen);

    scl_spinlock_lock(&ddos->lock);
    unsigned int idx = find_slot(ddos, family, key, klen, false, scl_now_ms());
    if (idx >= ddos->slot_count) {
        scl_spinlock_unlock(&ddos->lock);
        return SCL_ERR_NOT_FOUND;
    }

    /* Mark slot as empty. */
    scl_secure_zero(&ddos->slots[idx], sizeof(scl_ddos_slot_t));
    ddos->slots[idx].family = -1;
    scl_spinlock_unlock(&ddos->lock);
    return SCL_OK;
}

unsigned long scl_ddos_total_dropped(const scl_ddos_t *ddos) {
    return ddos ? (unsigned long)atomic_load_explicit(&ddos->total_dropped,
                                                      memory_order_relaxed) : 0;
}

unsigned long scl_ddos_total_allowed(const scl_ddos_t *ddos) {
    return ddos ? (unsigned long)atomic_load_explicit(&ddos->total_allowed,
                                                      memory_order_relaxed) : 0;
}
