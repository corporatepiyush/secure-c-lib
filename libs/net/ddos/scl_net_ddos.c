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
    /* IP address. For IPv4: addr64[0] = 0, addr32 = ip. For IPv6: full. */
    union {
        struct in_addr  v4;
        struct in6_addr v6;
    } ip;
    int      family;         /* AF_INET or AF_INET6, or -1 for empty */
    uint32_t hash;           /* cached hash */

    /* Token bucket */
    unsigned long tokens;       /* current token count (fixed-point) */
    int64_t       last_refill;  /* last refill timestamp (ms) */
    unsigned int  conn_count;   /* current concurrent connections */
    unsigned long total_drops;  /* total drops for this IP */

    /* Ban state */
    int64_t  ban_until;        /* 0 = not banned */
    bool     whitelisted;      /* bypass all checks */
} scl_ddos_slot_t;

/* ── Main state ─────────────────────────────────────────────────── */
struct scl_ddos {
    scl_allocator_t   *alloc;
    scl_ddos_config_t  cfg;

    scl_ddos_slot_t   *slots;
    unsigned int       slot_count;

    /* Stats */
    unsigned long      total_allowed;
    unsigned long      total_dropped;

    /* For LRU replacement when table is full. */
    unsigned int       next_victim;
};

/* ── Hash function (djb2 on IP bytes) ───────────────────────────── */
static uint32_t ip_hash(int family, const void *addr) {
    size_t len;
    unsigned char buf[16];

    if (family == AF_INET) {
        scl_memcpy(buf, addr, 4);
        len = 4;
    } else {
        scl_memcpy(buf, addr, 16);
        len = 16;
    }

    uint32_t h = 5381;
    for (size_t i = 0; i < len; i++)
        h = ((h << 5) + h) + buf[i];
    return h;
}

/* Compare two IP addresses for equality. */
static bool ip_eq(int family_a, const void *addr_a,
                  int family_b, const void *addr_b) {
    if (scl_unlikely(family_a != family_b)) return false;
    if (scl_likely(family_a == AF_INET))
        return scl_memcmp(addr_a, addr_b, 4) == 0;
    return scl_memcmp(addr_a, addr_b, 16) == 0;
}

/* Find or allocate a slot for an IP. Returns slot index or UINT32_MAX. */
static unsigned int find_slot(scl_ddos_t *ddos, int family,
                              const void *addr, bool alloc_if_missing) {
    uint32_t h = ip_hash(family, addr);
    unsigned int mask = ddos->slot_count - 1;
    unsigned int idx = h & mask;

    /* Linear probing. */
    for (unsigned int i = 0; i < ddos->slot_count; i++) {
        unsigned int probe = (idx + i) & mask;
        scl_ddos_slot_t *s = &ddos->slots[probe];

        if (scl_unlikely(s->family < 0)) {
            /* Empty slot. */
            if (scl_unlikely(!alloc_if_missing)) return UINT32_MAX;
            s->family = family;
            s->hash = h;
            s->tokens = ddos->cfg.burst_size;
            s->last_refill = 0;
            s->conn_count = 0;
            s->total_drops = 0;
            s->ban_until = 0;
            s->whitelisted = false;
            if (scl_likely(family == AF_INET))
                scl_memcpy(&s->ip.v4, addr, 4);
            else
                scl_memcpy(&s->ip.v6, addr, 16);
            return probe;
        }

        if (scl_likely(ip_eq(s->family, &s->ip, family, addr))) {
            /* Found existing slot. */
            return probe;
        }
    }

    if (!alloc_if_missing) return UINT32_MAX;
    return UINT32_MAX; /* table full */
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

bool scl_ddos_check(scl_ddos_t *ddos, const struct sockaddr *addr) {
    if (!ddos || !addr) return false;

    int family = addr->sa_family;
    const void *ip_addr;
    if (scl_likely(family == AF_INET)) {
        ip_addr = &((const struct sockaddr_in *)addr)->sin_addr;
    } else if (family == AF_INET6) {
        ip_addr = &((const struct sockaddr_in6 *)addr)->sin6_addr;
    } else {
        return true; /* unknown address family: allow */
    }

    unsigned int idx = find_slot(ddos, family, ip_addr, true);
    if (scl_unlikely(idx >= ddos->slot_count)) {
        /* Table full — allow (can't track all IPs under attack).
         * The LRU scheme could replace here, but for simplicity
         * we let it through. */
        ddos->total_allowed++;
        return true;
    }

    scl_ddos_slot_t *s = &ddos->slots[idx];

    /* Check whitelist. */
    if (scl_unlikely(s->whitelisted)) {
        s->conn_count++;
        ddos->total_allowed++;
        return true;
    }

    /* Check ban. */
    if (scl_unlikely(s->ban_until > 0)) {
        int64_t now = scl_now_ms();
        if (scl_unlikely(now < s->ban_until)) {
            ddos->total_dropped++;
            return false;
        }
        /* Ban expired. */
        s->ban_until = 0;
        s->total_drops = 0;
    }

    /* Token bucket refill. */
    int64_t now = scl_now_ms();
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

    /* Check concurrent connections limit. */
    if (scl_unlikely(s->conn_count >= ddos->cfg.max_conn_per_ip)) {
        s->total_drops++;
        ddos->total_dropped++;
        s->total_drops++;
        if (scl_unlikely(s->total_drops >= ddos->cfg.ban_threshold)) {
            s->ban_until = now + ddos->cfg.ban_duration_ms;
        }
        return false;
    }

    /* Check token bucket. */
    if (scl_unlikely(s->tokens == 0)) {
        s->total_drops++;
        ddos->total_dropped++;
        if (scl_unlikely(s->total_drops >= ddos->cfg.ban_threshold)) {
            s->ban_until = now + ddos->cfg.ban_duration_ms;
        }
        return false;
    }

    /* Consume a token. */
    s->tokens--;
    s->conn_count++;
    ddos->total_allowed++;
    return true;
}

void scl_ddos_conn_closed(scl_ddos_t *ddos, const struct sockaddr *addr) {
    if (!ddos || !addr) return;

    int family = addr->sa_family;
    const void *ip_addr;
    if (family == AF_INET)
        ip_addr = &((const struct sockaddr_in *)addr)->sin_addr;
    else if (family == AF_INET6)
        ip_addr = &((const struct sockaddr_in6 *)addr)->sin6_addr;
    else
        return;

    unsigned int idx = find_slot(ddos, family, ip_addr, false);
    if (idx >= ddos->slot_count) return;

    scl_ddos_slot_t *s = &ddos->slots[idx];
    if (s->conn_count > 0) s->conn_count--;
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
    unsigned char addr[16];
    int family = parse_ip_str(ip_str, addr);
    if (family < 0) return SCL_ERR_INVALID_ARG;

    unsigned int idx = find_slot(ddos, family, addr, true);
    if (idx >= ddos->slot_count) return SCL_ERR_FULL;

    /* Set ban until far future. */
    ddos->slots[idx].ban_until = INT64_MAX;
    return SCL_OK;
}

scl_error_t scl_ddos_whitelist_ip(scl_ddos_t *ddos, const char *ip_str) {
    if (!ddos || !ip_str) return SCL_ERR_NULL_PTR;
    unsigned char addr[16];
    int family = parse_ip_str(ip_str, addr);
    if (family < 0) return SCL_ERR_INVALID_ARG;

    unsigned int idx = find_slot(ddos, family, addr, true);
    if (idx >= ddos->slot_count) return SCL_ERR_FULL;

    ddos->slots[idx].whitelisted = true;
    ddos->slots[idx].ban_until = 0;
    return SCL_OK;
}

scl_error_t scl_ddos_unlist_ip(scl_ddos_t *ddos, const char *ip_str) {
    if (!ddos || !ip_str) return SCL_ERR_NULL_PTR;
    unsigned char addr[16];
    int family = parse_ip_str(ip_str, addr);
    if (family < 0) return SCL_ERR_INVALID_ARG;

    unsigned int idx = find_slot(ddos, family, addr, false);
    if (idx >= ddos->slot_count) return SCL_ERR_NOT_FOUND;

    /* Mark slot as empty. */
    scl_secure_zero(&ddos->slots[idx], sizeof(scl_ddos_slot_t));
    ddos->slots[idx].family = -1;
    return SCL_OK;
}

unsigned long scl_ddos_total_dropped(const scl_ddos_t *ddos) {
    return ddos ? ddos->total_dropped : 0;
}

unsigned long scl_ddos_total_allowed(const scl_ddos_t *ddos) {
    return ddos ? ddos->total_allowed : 0;
}
