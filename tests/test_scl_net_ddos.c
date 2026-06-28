/* Tests for scl_net_ddos: token-bucket rate limiting, per-IP concurrency caps,
 * allow/deny lists, IPv6 /64 aggregation, and — most importantly — that a full
 * table fails CLOSED rather than disabling protection. No sockets involved. */
#include "scl_test.h"
#include "scl_net_ddos.h"

#include <string.h>
#include <arpa/inet.h>

static struct sockaddr_in mk4(const char *ip) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    inet_pton(AF_INET, ip, &sa.sin_addr);
    return sa;
}

static struct sockaddr_in6 mk6(const char *ip) {
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    inet_pton(AF_INET6, ip, &sa.sin6_addr);
    return sa;
}

#define SA(x) ((struct sockaddr *)&(x))

/* ── Token bucket exhausts after burst_size connections ─────────── */
static void test_rate_limit(scl_test_runner_t *tr) {
    scl_test_group("DDoS: token bucket drops past burst");
    scl_ddos_config_t cfg = {
        .rate_per_sec = 0,        /* no refill: deterministic */
        .burst_size = 5,
        .max_conn_per_ip = 1000,  /* concurrency not the limiter here */
        .hash_slots = 256,
        .ban_duration_ms = 60000,
        .ban_threshold = 1000000, /* don't let banning interfere */
    };
    scl_ddos_t *d = NULL;
    SCL_EXPECT_OK(tr, scl_ddos_init(scl_allocator_default(), &d, &cfg));

    struct sockaddr_in c = mk4("203.0.113.7");
    int allowed = 0;
    for (int i = 0; i < 20; i++)
        if (scl_ddos_check(d, SA(c))) allowed++;
    SCL_EXPECT_EQ_I(tr, allowed, 5);                 /* exactly burst_size */
    SCL_EXPECT_EQ_I(tr, (int)scl_ddos_total_allowed(d), 5);
    SCL_EXPECT_EQ_I(tr, (int)scl_ddos_total_dropped(d), 15);

    /* A different IP is unaffected. */
    struct sockaddr_in c2 = mk4("203.0.113.8");
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(c2)));

    scl_ddos_destroy(d);
}

/* ── Per-IP concurrent connection cap ───────────────────────────── */
static void test_conn_cap(scl_test_runner_t *tr) {
    scl_test_group("DDoS: per-IP concurrency cap + release");
    scl_ddos_config_t cfg = {
        .rate_per_sec = 1000000, .burst_size = 1000000,
        .max_conn_per_ip = 3, .hash_slots = 256,
        .ban_duration_ms = 60000, .ban_threshold = 1000000,
    };
    scl_ddos_t *d = NULL;
    SCL_EXPECT_OK(tr, scl_ddos_init(scl_allocator_default(), &d, &cfg));

    struct sockaddr_in c = mk4("198.51.100.5");
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(c)));    /* 1 */
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(c)));    /* 2 */
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(c)));    /* 3 */
    SCL_EXPECT_TRUE(tr, !scl_ddos_check(d, SA(c)));   /* 4 -> over cap */

    scl_ddos_conn_closed(d, SA(c));                   /* free one slot */
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(c)));    /* now admitted */

    scl_ddos_destroy(d);
}

/* ── Whitelist bypass and blacklist drop ────────────────────────── */
static void test_lists(scl_test_runner_t *tr) {
    scl_test_group("DDoS: whitelist bypass / blacklist drop");
    scl_ddos_config_t cfg = {
        .rate_per_sec = 0, .burst_size = 1, .max_conn_per_ip = 1,
        .hash_slots = 256, .ban_duration_ms = 60000, .ban_threshold = 1000000,
    };
    scl_ddos_t *d = NULL;
    SCL_EXPECT_OK(tr, scl_ddos_init(scl_allocator_default(), &d, &cfg));

    /* Whitelisted IP is admitted far beyond burst/concurrency limits. */
    SCL_EXPECT_OK(tr, scl_ddos_whitelist_ip(d, "192.0.2.1"));
    struct sockaddr_in w = mk4("192.0.2.1");
    int ok = 1;
    for (int i = 0; i < 50; i++) ok &= scl_ddos_check(d, SA(w)) ? 1 : 0;
    SCL_EXPECT_TRUE(tr, ok);

    /* Blacklisted IP is always dropped. */
    SCL_EXPECT_OK(tr, scl_ddos_blacklist_ip(d, "192.0.2.99"));
    struct sockaddr_in b = mk4("192.0.2.99");
    SCL_EXPECT_TRUE(tr, !scl_ddos_check(d, SA(b)));

    /* Unlisting the blacklist entry restores normal handling. */
    SCL_EXPECT_OK(tr, scl_ddos_unlist_ip(d, "192.0.2.99"));
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(b)));    /* fresh bucket: admitted */

    scl_ddos_destroy(d);
}

/* ── IPv6 is aggregated by /64 prefix ───────────────────────────── */
static void test_ipv6_prefix(scl_test_runner_t *tr) {
    scl_test_group("DDoS: IPv6 tracked per /64 prefix");
    scl_ddos_config_t cfg = {
        .rate_per_sec = 0, .burst_size = 1, .max_conn_per_ip = 1000,
        .hash_slots = 256, .ban_duration_ms = 60000, .ban_threshold = 1000000,
    };
    scl_ddos_t *d = NULL;
    SCL_EXPECT_OK(tr, scl_ddos_init(scl_allocator_default(), &d, &cfg));

    /* Same /64, different host bits -> one shared bucket of size 1. */
    struct sockaddr_in6 a = mk6("2001:db8:abcd:1::1");
    struct sockaddr_in6 b = mk6("2001:db8:abcd:1::dead:beef");
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(a)));    /* consumes the token */
    SCL_EXPECT_TRUE(tr, !scl_ddos_check(d, SA(b)));   /* same /64 -> dropped */

    /* A different /64 has its own bucket. */
    struct sockaddr_in6 c = mk6("2001:db8:abcd:2::1");
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(c)));

    scl_ddos_destroy(d);
}

/* ── A full table must NOT fail open ────────────────────────────── */
static void test_full_table_fails_closed(scl_test_runner_t *tr) {
    scl_test_group("DDoS: full table fails closed (no bypass)");
    /* Minimum table size is 64 slots. Fill every slot with a distinct IP that
     * holds a live connection (never closed) so nothing is evictable. */
    scl_ddos_config_t cfg = {
        .rate_per_sec = 1000000, .burst_size = 1000000,
        .max_conn_per_ip = 1000, .hash_slots = 64,
        .ban_duration_ms = 60000, .ban_threshold = 1000000,
    };
    scl_ddos_t *d = NULL;
    SCL_EXPECT_OK(tr, scl_ddos_init(scl_allocator_default(), &d, &cfg));

    int admitted = 0;
    for (int i = 0; i < 64; i++) {
        char ip[32];
        snprintf(ip, sizeof(ip), "10.0.%d.%d", i / 256, i % 256);
        struct sockaddr_in s = mk4(ip);
        if (scl_ddos_check(d, SA(s))) admitted++;   /* leaves conn_count=1 */
    }
    SCL_EXPECT_EQ_I(tr, admitted, 64);

    /* The 65th distinct IP cannot be tracked and nothing is evictable: it must
     * be DROPPED, not waved through. This is the property that makes the limiter
     * useful against a source-address-spraying flood. */
    struct sockaddr_in overflow = mk4("172.16.0.1");
    SCL_EXPECT_TRUE(tr, !scl_ddos_check(d, SA(overflow)));

    /* Once an existing IP releases its connection, its slot becomes evictable
     * and the new IP can be admitted again. */
    char ip0[32]; snprintf(ip0, sizeof(ip0), "10.0.0.0");
    struct sockaddr_in s0 = mk4(ip0);
    scl_ddos_conn_closed(d, SA(s0));
    SCL_EXPECT_TRUE(tr, scl_ddos_check(d, SA(overflow)));

    scl_ddos_destroy(d);
}

int main(void) {
    scl_test_runner_t tr;
    scl_test_init(&tr);
    test_rate_limit(&tr);
    test_conn_cap(&tr);
    test_lists(&tr);
    test_ipv6_prefix(&tr);
    test_full_table_fails_closed(&tr);
    scl_test_summary(&tr);
    return tr.failed > 0 ? 1 : 0;
}
