# secure-c-libs

Production-grade C libraries with security-first hardening and mechanical-sympathy–driven performance. Built with `-std=c17`, `-O2`, `-fstack-protector-strong`, `_FORTIFY_SOURCE=3`, `-fcf-protection=full`. All allocators accept a runtime-swappable `scl_allocator_t` interface. Data structures favour SoA layouts, cache-line padding, and branchless ops.

## Build

```sh
make          # libscl.a + libscl_concurrent.a + run tests
make asan     # AddressSanitizer
make tsan     # ThreadSanitizer
make ubsan    # UndefinedBehaviourSanitizer
make fuzz     # libFuzzer targets
```

## Modules

### `libs/common/`
**scl_common** — Core type system: `scl_error_t` enum, `scl_allocator_t` interface, overflow-safe arithmetic (`scl_add_overflow`), bounds checkers (`scl_range_in_bounds`), cache-line alignment macros, branch prediction hints, CPU pause/yield, compiler barrier, power-of-2 helpers.

**scl_concurrent_common** — `scl_spinlock_t` (TTAS with pause), `SCL_CACHE_ALIGNED` padding macro, and `SCL_PAD_CACHE` for false-sharing prevention.

### `libs/stdlib/`
**scl_stdlib** — Safe wrappers: `scl_atoi` (range-checked), `scl_strtol` (errno+overflow), `scl_abs` (no INT_MIN UB), `scl_rand` (thread-local splitmix64).

**scl_atomic** — C11 `<stdatomic.h>` with `__sync_*` fallback for older GCC. Provides `scl_atomic_signal_fence` and all standard memory orders.

**scl_pthread** — Zero-overhead wrappers around POSIX threads. Eliminates attribute boilerplate; recursive mutex via `scl_mutexattr_recursive`. Debug owner-tracking behind `NDEBUG`. 505 lines of engineering rationale in `PthreadDesign.md`.

**scl_socket** — Hardened recv/send/accept with EINTR retry, SIGPIPE disarming via `MSG_NOSIGNAL`/`SO_NOSIGPIPE`, forced `TCP_NODELAY`, and non-blocking connect helper.

**scl_time** — Portable `timespec` helpers: monotonic clock, diff/normalize, `timespec_add`/`sub`, conversion to/from ns/μs/ms.

### `libs/allocator/`
Five swappable backends implementing `scl_allocator_t`:

| Backend | Description |
|---------|-------------|
| **arena** | Linear bump allocator; O(1) alloc, linked-list of chunks for bulk reset. |
| **buddy** | Binary buddy (max order 20, min block 16 B); coalesce on free. |
| **pool** | Fixed-size object pool; free-list embedded in slots, O(1) acquire/release. |
| **slab** | Bucket sizes 16–8192; per-size caches, fast path single pointer pop. |
| **tlsf** | TLSF real-time allocator (32 FL × 32 SL); worst-case O(1). |

### `libs/string/`
**scl_string** — Safe replacements: `scl_strcpy` (bounded), `scl_memset`/`scl_memcpy` (no-UB), `scl_strdup` (via allocator), `scl_secure_zero` (volatile/compiler-barrier wipe).

### `libs/math/`
**scl_math** — `exp`/`log`/`pow`/`sqrt`/trig functions returning NaN on domain error (no UB). Wraps `math.h` with hardened arg validation.

### `libs/log/`
**scl_log** — Level-based logger: `SCL_LOG_DEBUG/INFO/WARN/ERROR`. Colourised terminal output, optional timestamps, `stderr` output. Compiled out at `-DSCL_LOG_DISABLE`.

### `libs/threadpool/`
**scl_threadpool** — Fixed-size worker pool (cap 128). Mutex+condvar dispatch, `scl_threadpool_submit` with optional `scl_future_t` for result retrieval.

### `libs/testlib/`
**scl_test** — Assert macros: `scl_expect_true/false/eq_i/eq_u/eq_str/eq_mem/eq_ptr/null/not_null/lt/gt/le/ge`.

**scl_test_runner** — Suite lifecycle (`SCL_TEST_SUITE`/`SCL_TEST`), aggregate pass/fail reporting, exit code propagation.

### `libs/net/http/`
**scl_http_server** — HTTP/1.1 server. Lock-free TCP connection pool, path-traversal defence (`..` + `%00` rejection), request-smuggling guards, MIME type detection.

**scl_http_client** — HTTP/1.1 client. Keep-alive, bounded response buffering, percent-decoding, redirect following.

### `libs/net/pool/`
**scl_tcp_pool** — Lock-free TCP connection pool. Treiber-stack + Vyukov bounded MPMC for concurrent acquire/release. Cache-line padded head/tail.

### `libs/net/ddos/`
**scl_net_ddos** — Token-bucket rate limiter (configurable burst/rate), IP blacklist/whitelist (Patricia trie), LRU eviction for grey-list scoring.

### `libs/random/`
**scl_rand_prng** — Xoshiro256\*\* PRNG. 256-bit state, 64-bit output, passes BigCrush. Splitmix64 seeding from `/dev/urandom` or `time+pid`.

**scl_rand_noise** — Value noise, Perlin noise (1D/2D/3D), white noise, fractal Brownian motion. Uses PRNG for permutation table.

**scl_rand_distribution** — Uniform (integer/real), Normal (Box-Muller), Exponential, Bernoulli, Poisson, Fisher-Yates shuffle.

**scl_rand_uuid** — UUID v4 generation (122 random bits). Format/parse to/from `xxxxxxxx-xxxx-4xxx-xxxx-xxxxxxxxxxxx`.

### `libs/compress/gzip/`
**scl_gzip** — RFC 1952 gzip: LZ77 + Huffman coding with CRC-32. Streaming compress/decompress. Fixed/dynamic Huffman trees.

### `libs/docparse/`
| Parser | Format | Highlights |
|--------|--------|------------|
| **csv** | RFC 4180 | State-machine parser, quoted fields, CRLF tolerant |
| **tsv** | Tab-sep | Streaming row-by-row, escape handling |
| **json** | ECMA-404 | Recursive-descent → tree, UTF-8 validation |
| **pdf** | PDF 1.7 | Xref-table object parser (max 4096 objects) |
| **docx** | Office Open XML | ZIP-based text extractor, shared-strings resolution |
| **xlsx** | Office Open XML | ZIP sheet/cell reader, cell-type dispatch |
| **parquet** | Apache Parquet | PAR1 footer + column-chunk metadata, schema read |
| **icelake** | Apache Iceberg | Table metadata JSON, manifest list/file, partition scan |

### `libs/sort/`
Unified dispatch in `scl_sort.h` (introspective: quick → heap → insertion). Individual algorithm subdirs: bubble, bucket, counting, heap, insertion, merge (top-down / bottom-up), quick (Lomuto/Hoare), radix (LSD), selection, shell (Knuth gaps).

### `libs/search/`
**Array searches:** linear, binary, jump, interpolation, exponential, fibonacci, ternary, meta-binary, unbounded-binary, sentinel-linear.

**String/pattern:** linear-string, binary-string, exponential-string. KMP (prefix function), Rabin-Karp (rolling hash), Boyer-Moore (bad-char/good-suffix).

**Data-structure-backed:** Trie, hash-table.

**Selection:** Quickselect (Hoare + Lomuto partition, Floyd–Rivest median-of-3 pivot).

**Graph algorithms:** A\*, Bellman-Ford, BFS, DFS, Dijkstra (binary-heap), Floyd-Warshall.

### `libs/ds/`
| Structure | File(s) | Notes |
|-----------|---------|-------|
| **array** | `scl_array` | Dynamic array, geometric growth |
| **avl** | `scl_avl` | Self-balancing BST, recursive |
| **bloom** | `scl_bloom` | Bloom filter, double-hash scheme |
| **bst** | `scl_bst` | Unbalanced BST |
| **btree** | `scl_btree` | B-tree (order t) |
| **deque** | `scl_deque` | Ring-buffer deque, O(1) both ends |
| **dlist** | `scl_dlist` | Doubly-linked list, sentinel |
| **fenwick** | `scl_fenwick` | Fenwick tree (BIT), prefix sum |
| **graph** | `scl_graph` | Adjacency list, directed/undirected |
| **hash** | `scl_hash` | Open-addressing, Robin Hood |
| **heap** | `scl_heap` | Binary heap (min/max) |
| **lru** | `scl_lru` | Hash+list LRU, O(1) get/put |
| **queue** | `scl_queue` | Linked-list queue |
| **rbtree** | `scl_rbtree` | Red-black tree, iterative |
| **ringbuf** | `scl_ringbuf` | Fixed ring buffer, power-of-2 mask |
| **segtree** | `scl_segtree` | Segment tree, range query/update |
| **skiplist** | `scl_skiplist` | Skip list, max 32 levels |
| **slist** | `scl_slist` | Singly-linked list |
| **sparse** | `scl_sparse` | Sparse set, O(1) ops |
| **stack** | `scl_stack` | Dynamic stack |
| **trie** | `scl_trie` | Trie/radix tree |
| **unionfind** | `scl_unionfind` | DSU with path compression |

### `libs/ds/concurrent/`
**Lock-free:** `scl_concurrent_lfstack` (Treiber stack, DWCAS, ABA-safe), `scl_concurrent_mpmc` (Vyukov bounded MPMC queue).

**Spinlock-guarded wrappers** — Every DS above gets a `scl_concurrent_*` variant: `scl_spinlock_t` protects the underlying structure. All in `libs/ds/concurrent/`.

### `tests/`
Test binaries (one per `test_scl_*.c`) using `scl_test`. Each exercises construction, mutation, query, edge cases, and teardown. Run via `make test`.

**Concurrent tests** under `tests/concurrent/` exercise spinlock-guarded variants under multi-threaded contention.

### `tests/fuzz/`
libFuzzer targets for HTTP URL parsing, MIME type detection, and multipart boundary parsing. Run via `make fuzz`.

## Sanitisers

```sh
make asan   # AddressSanitizer
make tsan   # ThreadSanitizer
make ubsan  # UndefinedBehaviourSanitizer
make msan   # MemorySanitizer
make check  # asan + ubsan
```

## Philosophy

See `AGENTS.md` for the full mechanical-sympathy design contract: zero-allocation hot paths, SoA over AoS, branchless arithmetic, arena-based lifecycles, lock-free concurrency, and profile-guided measurement.
