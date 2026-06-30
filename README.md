# secure-c-libs

**The C library you'd write if memory safety and cache misses both paid your salary.**

A production-grade collection of data structures, allocators, ML primitives, and networking components — every line hardened against UB, designed for mechanical sympathy, and built to integrate as a single `libscl.a`.

---

## Why secure-c-libs?

Most C libraries ask you to choose: safe or fast. We refuse the trade-off.

**Security-first, by default.**  
`-std=c17` with `-fstack-protector-strong`, `_FORTIFY_SOURCE=3`, bounds-checked deserialisation, CRC32C integrity on every serialised model, overflow-safe arithmetic, and zero undefined behaviour paths. Every function validates its inputs. Every buffer has a bound.

**Performance that respects the silicon.**  
Flat arrays over pointer chasing. SoA layouts over AoS. Cache-line–padded atomics. Branchless arithmetic. Arena allocators for O(1) allocation. Lock-free concurrent structures (Treiber stacks, Vyukov MPMC queues). The machine tells us where the cycles go — we listen.

**One allocator interface to rule them all.**  
Five swappable backends (`arena`, `buddy`, `pool`, `slab`, `tlsf`) behind a single `scl_allocator_t` vtable. Inject your own at runtime. Every library component — from JSON parser to ML model — honours it.

**Comprehensive, not minimal.**  
- 20+ lock-free and spinlock-guarded concurrent data structures  
- Full sorting and search library (18 algorithms)  
- Real ML: SVM (SMO), logistic regression (SGD), k-NN, k-means, PCA, Naive Bayes, decision trees  
- RFC-compliant parsers: CSV, JSON, Parquet, Iceberg, DOCX, XLSX, PDF  
- HTTP/1.1 server + client with DDoS mitigation  
- Gzip compression (LZ77 + Huffman)  
- Xoshiro256** PRNG, Perlin noise, UUID v4  
- Thread pool, safe string routines, hardened socket wrappers  

**Battle-tested under sanitizers.**  
50 test suites, runnable under AddressSanitizer, ThreadSanitizer, UndefinedBehaviourSanitizer, and MemorySanitizer. Plus libFuzzer targets for attack-surface components.

---

## Quick start

```sh
make          # build libscl.a + libscl_concurrent.a + run all tests
make test     # 50 test suites, zero failures expected
make asan     # rebuild and test under AddressSanitizer
make ubsan    # rebuild and test under UBSan
make check    # asan + ubsan
make fuzz     # libFuzzer targets for HTTP, MIME, multipart
```

Link with `-lscl` (sequential structures) or `-lscl_concurrent` (lock-free + spinlock-guarded variants).

---

## What's inside

| Category | Components |
|----------|-----------|
| **Allocators** | Arena (bump), Buddy (binary), Pool (fixed-size), Slab (bucketed), TLSF (real-time O(1)) |
| **Data structures** | AVL/RB/BST/B-tree, hash (Robin Hood), heap, deque, ring buffer, sparse set, fenwick tree, segment tree, trie, LRU, bloom filter, graph, skiplist, union-find, dynamic array |
| **Concurrent** | Treiber stack, Vyukov MPMC, spinlock-guarded wrappers for every DS above |
| **ML** | SVM (SMO), logistic regression (SGD + Adam), k-NN, k-means, PCA, Gaussian NB, decision tree — all with CRC32C-verified serialisation |
| **Networking** | HTTP/1.1 server + client, TCP connection pool, token-bucket DDoS protection |
| **Parsers** | CSV, TSV, JSON, Parquet, Iceberg, DOCX, XLSX, PDF |
| **Compression** | Gzip (RFC 1952), LZ77 + Huffman |
| **Random** | Xoshiro256**, splitmix64, UUID v4, Perlin/value noise, uniform/normal/exponential/Bernoulli/Poisson distributions |
| **Search** | 15+ algorithms including KMP, Boyer-Moore, Rabin-Karp, Dijkstra, A*, Floyd-Warshall |
| **Sort** | Introsort dispatch + 10 individual algorithms |
| **Safety** | Bounded string ops, overflow-checked arithmetic, secure memory zeroing, range validators |

---

## Design principles

- **Zero-allocation hot paths** — pre-allocate during init; use object pools and arenas at runtime.
- **Cache-line discipline** — false-sharing prevention via `SCL_CACHE_ALIGNED`, SoA data layout, prefetcher-friendly access patterns.
- **Lock-free where it counts** — Treiber stack, Vyukov MPMC, atomic sequence counters. Spinlocks only guard rare-path wrappers.
- **Deterministic tests** — all random generators seedable; serialisation round-trips verified byte-for-byte.
- **Compiler-agnostic hardening** — works on GCC and Clang; platform-specific flags guarded by `uname` and `__has_feature`.

---

## License

Apache 2.0. See `LICENSE`.
