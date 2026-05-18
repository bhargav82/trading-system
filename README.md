# SPSC Queue

A high-performance single-producer, single-consumer (SPSC) bounded queue implemented in modern C++.

Designed for low latency and cache efficiency in single-producer / single-consumer pipelines.

---

## Features

- Lock-free SPSC design using C++ atomics
- Cache-line aligned head/tail indices to prevent false sharing
- Reduced atomic load pressure via local index caching
- Optimized memory ordering:
  - `release/acquire` for synchronization
  - `relaxed` for fast-path buffer access

---

## Performance

Benchmarked on macOS (Apple Silicon), release build (`-O2`), using Google Benchmark.

| Benchmark | Wall Time | CPU Time |
|----------|-----------|----------|
| 500 `emplace_back`, queue size 5000 | 1,013 ns | 1,016 ns |

### ~2 ns per operation (amortized)

This is near L1 cache latency range and reflects a highly optimized fast path. Wall and CPU times being nearly identical suggests minimal OS scheduling overhead.

> ⚠️ Note: Microbenchmark results are hardware- and workload-dependent and should not be treated as universal throughput numbers.

---

## Design & Optimizations

### False sharing avoidance

`head` and `tail` indices are placed on separate cache lines using `alignas(64)`.

This prevents cache line bouncing between producer and consumer cores, reducing coherence traffic and improving scalability.

---

### Memory ordering model

- Producer writes use `std::memory_order_release`
- Consumer reads use `std::memory_order_acquire`
- Buffer accesses use `std::memory_order_relaxed`

Correctness is guaranteed through index synchronization, avoiding expensive `seq_cst` fences and improving throughput.

---

### Local index caching

Each thread caches the last observed opposite index and only refreshes it when the queue appears full or empty.

This reduces atomic loads on the hot path and improves steady-state performance.

---

## Environment

- Platform: macOS (Apple Silicon)
- Compiler: Clang
- Build flags: `-O2`
- Benchmark tool: Google Benchmark
- Queue size: 5000 elements