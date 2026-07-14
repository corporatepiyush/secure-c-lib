# secure-c-libs

A collection of security-hardened, performance-oriented data-structure, algorithm,
and systems libraries written in portable C17. Every module is built with
mechanical-sympathy in mind — flat memory layouts, cache-line awareness, lock-free
hot paths, and zero-allocation inner loops — while being compiled and tested under an
aggressive hardening and sanitizer regime.

The project ships two static archives:

| Archive | Contents |
| --- | --- |
| `libscl.a` | Single-threaded / sequential implementations (`scl_*`) |
| `libscl_concurrent.a` | Thread-safe / lock-free variants (`scl_concurrent_*`) |

## Design principles

- **Mechanical sympathy first.** Data flows as contiguous byte streams. Prefer flat
  arrays and SoA layouts over pointer-chasing object graphs. Hot fields are packed
  together; atomic fields are padded to cache-line boundaries to avoid false sharing.
- **Zero-allocation hot paths.** Arenas, pools, slabs, and free-lists are used so the
  inner loops never touch the general allocator.
- **Lock-free where it counts.** Concurrent structures use CAS loops, acquire/release
  ordering, and sharding instead of coarse mutexes on contended paths.
- **Security by construction.** All untrusted-input parsers are hardened against
  malformed data and validated under AddressSanitizer, not just functional tests.
- **Centralized platform layer.** Every standard-library and platform dependency is
  routed through the `scl_*` proxy headers in `libs/core`, so hardening and portability
  fixes apply project-wide.

## Library modules

All sources live under `libs/<module>/`. Public APIs are exposed through the
corresponding `scl_*.h` headers.

| Module | What it provides |
| --- | --- |
| `core` | Platform proxy headers (`scl_stdlib.h`, `scl_string.h`, `scl_atomic.h`, `scl_pthread.h`, `scl_socket.h`, …), logging, math, and the test framework. |
| `common` | Shared helpers for the sequential and concurrent builds. |
| `allocator` | Arena, pool, slab, buddy, and TLSF allocators (sequential + concurrent). |
| `structures` | Array, deque, singly/doubly linked list, hash map, B-tree, red-black tree, skip list, trie, heap, graph, union-find, bloom filter, LRU cache, sharded array — each with a concurrent variant. |
| `sort` | Bubble, insertion, selection, shell, merge (incl. parallel), quick, heap, counting, radix, bucket sort. |
| `search` | Linear, binary (and variants), interpolation, exponential, jump, Fibonacci, ternary, hash; string search (KMP, Boyer-Moore, Rabin-Karp, trie, suffix trie); graph search (BFS, DFS, Dijkstra, A\*, Bellman-Ford, Floyd-Warshall); quickselect. |
| `ml` | Linear/logistic regression, SVM, decision tree, random forest, GBDT, k-means, DBSCAN, GMM, k-NN, naive Bayes, PCA, scalers, and metrics — with a runtime-dispatched SIMD backend (scalar / SSE4.2 / AVX2 / AVX-512 / NEON / SVE). |
| `random` | PRNGs, distributions, noise generators, and UUID generation. |
| `compress` | Gzip (DEFLATE) encoder/decoder. |
| `docparse` | Hardened parsers for untrusted documents: CSV, TSV, JSON, PDF, DOCX, XLSX, Parquet, and Iceberg. |
| `net` | TCP connection pool, HTTP client/server, and DDoS-mitigation helpers. |
| `threadpool` | Work-stealing thread pool. |

## Building

The project uses a single top-level `Makefile`. It auto-detects the compiler family
(GCC/Clang), the host ISA (x86-64 / ARM64), and the available glibc version to enable
the strongest applicable hardening.

```sh
make lib      # build libscl.a and libscl_concurrent.a
make test     # build all test binaries under tests/
make all      # lib + mandatory sanitizer checks + tests (default)
make clean    # remove build artifacts
```

Builds are parallel by default (`-j$(nproc)`).

### Hardening flags

Every translation unit is compiled with, at minimum:

```
-std=c17 -Wall -Wextra -Wpedantic -Werror -O2
-fstack-protector-strong -fPIE
-Wformat -Wformat-security -Wnull-dereference
-D_FORTIFY_SOURCE=2   # bumped to 3 on glibc >= 2.35
```

On GCC additional protections are enabled where supported:
`-fstack-clash-protection`, `-Walloc-zero`, `-Wstringop-overflow`,
`-Warray-bounds=2`, control-flow integrity (`-fcf-protection=full` on x86-64),
and `-Wl,-z,relro,-z,now` at link time. ML kernels are additionally built with
`-O3 -ffp-contract=fast -funroll-loops -ftree-vectorize`.

## Testing & sanitizers

Tests live under `tests/` (one directory per feature). The default `make all` target
will not ship until the mandatory sanitizer checks pass.

```sh
make asan-check     # build + run the suite under AddressSanitizer
make ubsan-check    # UndefinedBehaviorSanitizer
make tsan-check     # ThreadSanitizer (concurrency)
make sanitize-all   # asan-check + ubsan-check
make check          # sanitize-all + tsan
make fuzz           # build libFuzzer targets under tests/fuzz/
```

`make asan`, `make ubsan`, `make tsan`, and `make msan` build the sanitizer-instrumented
binaries without running them; `make filc` and `make llvm` provide additional
alternative-toolchain builds.

> **Note on SIMD:** the x86 SIMD backends are gated behind the host ISA. When developing
> on an ARM machine, cross-compile the x86 paths explicitly (e.g. `clang -arch x86_64`)
> so they don't silently skip compilation.

## Usage

Link against the archive(s) you need and add the module include directories:

```sh
cc myprog.c libscl.a libscl_concurrent.a -lm -lpthread -o myprog
```

Include the relevant public header, for example:

```c
#include "scl_sort.h"
#include "scl_ml.h"
```

## Repository layout

```
libs/          library sources, grouped by module
  core/        platform proxy headers + test framework
  common/      shared helpers
  <module>/    one directory per library (see table above)
tests/         per-feature test suites + fuzz/ targets
tools/         supporting tooling
Makefile       hardened, parallel build
```

## License

See repository for license details.
