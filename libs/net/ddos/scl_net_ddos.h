#ifndef SCL_NET_DDOS_H
#define SCL_NET_DDOS_H

/*
 * scl_net_ddos — DDoS mitigation for TCP-based servers.
 *
 * ── What this provides ───────────────────────────────────────────────────────
 *
 * A lightweight, pluggable DDoS prevention module designed to integrate
 * with scl_tcp_pool at the accept() boundary. It implements:
 *
 *   1. Per-IP rate limiting (token bucket algorithm): each source IP gets
 *      a bucket that refills at a configured rate. If the bucket is empty,
 *      the connection is rejected.
 *   2. Max concurrent connections per IP: limits how many slots a single
 *      IP can occupy at once (prevents slot-exhaustion attacks).
 *   3. IP blacklisting: manually or automatically blacklist IPs that exceed
 *      thresholds.
 *   4. White-listing: trusted IPs bypass all checks.
 *
 * ── Integration pattern ──────────────────────────────────────────────────────
 *
 * In the server's acceptor thread (acceptor_main in scl_http_server.c):
 *
 *     scl_tcp_conn_t tmp;
 *     if (scl_tcp_accept(listen_fd, &tmp) == SCL_OK) {
 *         if (!scl_ddos_check(&ddos, (struct sockaddr *)&tmp.peer)) {
 *             close(tmp.fd);
 *             continue;  // rate-limited or blacklisted
 *         }
 *         // proceed with acquire() + post_ready()
 *     }
 *
 * ── Thread safety ────────────────────────────────────────────────────────────
 *
 * The rate limiter uses per-slot atomic operations (no global lock). The
 * IP-to-slot hash is guarded by a spinlock for the rare resize/grow path.
 * In practice, the acceptor is single-threaded, so most accesses are
 * contention-free.
 *
 * ── Security considerations ──────────────────────────────────────────────────
 *
 *   • The token bucket prevents the IP from exhausting server resources,
 *     not from sending malicious payloads (that's the application's job).
 *   • Bucket state uses relaxed memory ordering — approximate counting is
 *     acceptable for rate limiting (no security-critical precision needed).
 *   • The hash table uses scl_secure_zero on entry removal to prevent
 *     information leakage via timing side-channels on the IP data.
 */

#include "scl_common.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* ── Configuration ────────────────────────────────────────────────
 *
 * Reasonable defaults for a small server. Adjust per deployment. */
typedef struct {
    unsigned long  rate_per_sec;       /* token refill rate per IP (default 100) */
    unsigned long  burst_size;         /* max tokens per IP (default 200) */
    unsigned int   max_conn_per_ip;    /* max concurrent connections per IP (default 10) */
    unsigned int   hash_slots;         /* number of hash table slots (default 1024) */
    int64_t        ban_duration_ms;    /* auto-ban duration after threshold (default 60000) */
    unsigned long  ban_threshold;      /* drops before auto-ban (default 20) */
} scl_ddos_config_t;

typedef struct scl_ddos scl_ddos_t;

/* ── Lifecycle ────────────────────────────────────────────────────
 *
 * Initialise a DDoS mitigation state. If cfg is NULL, defaults are used.
 * Memory is allocated from `alloc`. */
scl_error_t scl_ddos_init(scl_allocator_t *alloc, scl_ddos_t **out,
                          const scl_ddos_config_t *cfg);

/* Free all resources. */
void scl_ddos_destroy(scl_ddos_t *ddos);

/* ── Check / allow / deny ─────────────────────────────────────────
 *
 * Check whether a connection from a given IP should be allowed.
 * `addr` is a sockaddr_in or sockaddr_in6 (uses sin_addr / sin6_addr).
 *
 * Returns true if the connection should be accepted, false to drop. */
bool scl_ddos_check(scl_ddos_t *ddos, const struct sockaddr *addr);

/* Notify the module that a connection from this IP has been closed
 * (so it can decrement the concurrent-connection counter). */
void scl_ddos_conn_closed(scl_ddos_t *ddos, const struct sockaddr *addr);

/* Manually blacklist an IP (string format, e.g. "192.168.1.1").
 * Returns SCL_OK on success. */
scl_error_t scl_ddos_blacklist_ip(scl_ddos_t *ddos, const char *ip_str);

/* Manually whitelist an IP (bypasses all checks). */
scl_error_t scl_ddos_whitelist_ip(scl_ddos_t *ddos, const char *ip_str);

/* Remove an IP from a black/whitelist. */
scl_error_t scl_ddos_unlist_ip(scl_ddos_t *ddos, const char *ip_str);

/* ── Stats ────────────────────────────────────────────────────────
 *
 * Return the total number of dropped connections since init. */
unsigned long scl_ddos_total_dropped(const scl_ddos_t *ddos);
unsigned long scl_ddos_total_allowed(const scl_ddos_t *ddos);

#endif /* SCL_NET_DDOS_H */
