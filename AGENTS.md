# Mechanical Sympathy & Performance Principles

**Core Objective:** Treat every abstraction as a measurable, quantifiable cost. Prioritize mechanical sympathy, cache-line granularity, zero-allocation hot paths, kernel-boundary minimization, and compiler-friendly structures. Every byte of indirection, every cycle of branch misprediction, and every nanosecond of cache coherency traffic is a failure to respect the silicon.

## Data Representation & CPU Cache Alignment

**Mechanical Sympathy over Deep Hierarchies:** Data must flow as contiguous byte streams. Prioritize flat arrays and dense vectors over deep object graphs, nested classes, or pointer-chasing models. A single pointer dereference can cost 100 ns from DRAM versus 1 ns from L1 cache. On x86-64, the L1 cache is 32–64 KB per core with 64-byte lines; on ARM64 (Neoverse V2), L1 is 64 KB with 128-byte lines. The hardware prefetcher loads full cache lines; ensure each line contains only active payload. Avoid virtual dispatch in hot structs. Prefer plain data structs with minimal headers: pack fields tightly.

**Structure of Arrays (SoA):** Decompose objects into parallel primitive arrays. When iterating over particle positions, storing `x[], y[], z[]` separately loads only position data into cache lines, whereas Array-of-Structs mixes inactive fields into the same lines. SoA enables auto-vectorization into wide SIMD registers (512-bit AVX-512, ARM SVE). For mixed access patterns, use hybrid AoSoA (Array-of-Structure-of-Arrays): block 8–16 elements into mini-SoA bundles.

**Cache-Line Padding & False Sharing:** Cache coherency operates at 64-byte granularity on most x86/ARM cores. If two parallel threads write to distinct variables on the same cache line, MESI triggers cross-core invalidation. A single false-shared write can drop throughput from millions of ops/sec to thousands. Pad volatile counters and atomic fields to cache-line boundaries.

**Pointer Elimination & Flat Indexing:** Replace references with base + index × stride arithmetic. Intrusive linked lists embed link nodes directly inside data structs. Use relative offsets (32-bit indices into arena buffers) instead of absolute 64-bit pointers where possible.

**NUMA Topology Awareness:** On multi-socket systems, local DRAM access is ~80 ns; remote NUMA node access is ~130–150 ns. Pin thread memory and threads to local sockets. Never let one thread allocate on one socket and hand the pointer to a thread on another socket for mutation.

## Algorithmic Mastery & Lock-Free Concurrency

**Eradicate Blocking Primitives on Hot Paths:** Blocking locks incur kernel-mode context switches (~1–2 μs). Replace with lock-free atomics: compare-and-swap loops, load-acquire/store-release pairs, and atomic sequence counters. Use memory ordering judiciously.

**Bespoke Data Structures:**
- **Ring Buffers:** Fixed-size arrays with atomic sequence counters. Power-of-two sizing enables bitwise masking (`index & (size-1)`).
- **Sparse Sets:** Two dense arrays for O(1) membership, insert, clear with perfect locality.
- **Intrusive Containers:** Embed list nodes directly inside data structs. Eliminates separate wrapper allocations.

**State Sharding:** Partition mutable state by thread ID or CPU core ID modulo shard count. Each worker writes to its local shard; readers merge lazily.

**Read-Copy-Update (RCU):** For read-heavy, write-rare structures, use RCU. Readers proceed with zero atomic operations; writers publish via single atomic pointer swap.

## Control Flow & CPU Instruction Maximization

**Branchless Execution:** Replace conditional logic with arithmetic masks, bitwise operations, or conditional move/select instructions. Use lookup tables when branch is unpredictable but domain is small (≤256).

**Branch Prediction & Layout:** Mark cold paths as unlikely so the compiler places hot code sequentially. Ensure hot paths fall through; cold paths jump away.

**Loop Unrolling & Vectorization:** Unroll by factors matching SIMD width. Ensure inner loops have no loop-carried dependencies. Use unsigned or width-matched induction variables.

**Function Inlining & Monomorphism:** Keep hot functions small and type-stable. Enforce data homogeneity: sort arrays by concrete type before processing.

**Cache-Oblivious Design:** Tile operations into micro-blocks fitting L1 (32–64 KB) or L2 (256 KB–1 MB). Target a compute-to-memory ratio of >2 FLOPs per byte loaded.

## Memory Allocator & Kernel Exploitation

**Zero-Allocation Hot Paths:** Pre-allocate all required containers, pools, and working buffers during initialization. Use object pools with free-lists embedded in unused slots.

**Arena & Region Allocators:** Group objects sharing identical lifecycle into a single pre-allocated buffer. Allocation becomes pointer increment (O(1)). Deallocate entire arena at once.

**Virtual Memory & Huge Pages:** Configure huge pages (2 MB or 1 GB) to reduce TLB misses. Align custom heaps to page boundaries.

**Zero-Copy I/O:** Use memory-mapped files, splice/sendfile, and async ring-buffer interfaces to bypass kernel copying.

**Hardware Affinity & Offloading:** Pin processing threads to specific physical CPU cores. Avoid sharing a physical core via SMT if workload is cache-bound.

## Compiler & Runtime Optimization

**Global Value Numbering & CSE:** Hoist and cache all repeated property lookups, array lengths, and invariant calculations into local variables before entering loops.

**Loop Unswitching & Invariant Code Motion:** If a loop contains a conditional whose predicate does not change, unswitch it: branch on condition first, then write two specialized loops.

**Basic Block Linearization & Cold-Path Outlining:** If an edge case occurs inside a tight loop, branch to a separate, non-inlined function. Keeps I-cache saturated with pure compute.

**Scalar Replacement of Aggregates:** Keep data structures flat and tightly constrained to local function parameters. Destructure objects into primitive locals.

**Loop Strength Reduction:** Maintain linear tracking pointer via addition rather than recalculating `base + index * stride`. Use power-of-two sizing so modulo becomes bitwise AND.

**Alias Analysis & Load-Store Disambiguation:** Localize counters to the stack frame. Never execute nested mutations inside tight loops.

**Superword Level Parallelism & Vectorization:** Ensure operations inside a loop act on completely decoupled parallel array streams. Avoid mixing different primitive data sizes in the same compute block.

**Register Pressure & Loop Fission:** If a processing loop contains more than 4–5 distinct array updates, decompose it into multiple sequential loops.

**Profile-Guided Devirtualization:** Enforce absolute data homogeneity across processing streams. Sort/partition data by exact concrete type before execution loops.

## Measurement & Validation

- Use hardware cycle counters for timing.
- Pin benchmarks to isolated cores; disable frequency scaling.
- Run warm-up iterations; report median and percentiles.
- Profile cycles, cache misses, branch misses, TLB misses.
- Inspect compiler optimization reports and generated assembly.
- Monitor allocation rates and GC pressure in managed runtimes.

## Application to This Codebase (secure-c-libs)

- All hot-path functions should avoid dynamic allocation; use pre-allocated arenas and pools.
- Prefer flat arrays of structs or SoA layouts over pointer-linked structures.
- Mark branch predictions with `scl_unlikely`/`scl_likely`.
- Use lock-free atomics (from `scl_atomic.h`) instead of mutexes on hot paths.
- Align atomic fields to cache-line boundaries to prevent false sharing.
- Keep hot loops small and free of function calls; inline where possible.
- Use power-of-two sizes for ring buffers and cyclic structures.
- Arena-allocate short-lived objects and reset rather than free individually.
- Profile before and after changes; target L1-cache–resident working sets.

## Development Tooling: Nushell for Terminal Tasks

For all terminal operations, scripting, and data manipulation, use **Nushell**:
https://www.nushell.sh/commands/

**Nushell Capabilities:**
- Structured data pipelines (JSON, CSV, tables native)
- File system operations and batch processing
- String manipulation and regular expressions
- Mathematical and logical operations
- Date/time operations and formatting
- Data filtering, transformation, and aggregation
- System information gathering (processes, disk, memory, network)
- Path and environment variable management
- Type-safe command composition

**Development Use Cases:**
- Test case generation and data generation
- Log analysis, filtering, and reporting
- Build system utilities and verification
- Web fetch and API operations
- Batch file processing (small and large files)
- Data validation and error checking
- Configuration file manipulation
- Performance metrics collection and analysis

**Why Nushell:** Structured data handling, type safety, better composability, safer edge case handling, and cross-platform consistency vs. traditional shell scripting.
