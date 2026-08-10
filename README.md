# Trading System

A low-latency trading system built from scratch in modern C++, focused on the core
infrastructure pieces a matching engine needs: a lock-free SPSC queue for inter-thread
messaging, a custom memory pool to keep order allocation off the general-purpose
allocator, and a price-time-priority limit order book on top of them.

This is a learning/portfolio project, built one component at a time with an emphasis on
understanding *why* each design choice matters for latency and correctness, not just
getting something that compiles.

---

## What's here

| Component | Status | Docs |
|---|---|---|
| **SPSC Queue** — lock-free single-producer/single-consumer ring buffer | Implemented, unit tested, cross-core benchmark in progress | [docs/SPSC_QUEUE.md](docs/SPSC_QUEUE.md) |
| **Memory Pool** — preallocated fixed-slot allocator | Implemented, unit tested, initial speed comparison added | [docs/MEMORY_POOL.md](docs/MEMORY_POOL.md) |
| **Order Book / Matching Engine** — price-time priority limit order book | Core matching logic implemented and tested | [docs/ORDER_BOOK.md](docs/ORDER_BOOK.md) |
| **TCP socket layer** (`socket_utils.h`, `tcp_socket.h`) | Work in progress, not yet wired into the matching engine | — |

For a tour of the specific engineering techniques used across the codebase (lock-free
programming, thread pinning, move semantics, benchmarking methodology, etc.), see:

**➡️ [docs/CONCEPTS.md](docs/CONCEPTS.md)**

---

## Repository structure

```
.
├── CMakeLists.txt                 # root build config, fetches googletest + benchmark
├── header/
│   ├── libraries/                 # the actual system -- header-only
│   │   ├── common.h               # shared types: Order, ClientRequest, enums, wire structs
│   │   ├── spsc_queue.h           # LockedQueue + lock-free SPSCQueue
│   │   ├── mempool.h              # MemoryPoolHeap
│   │   ├── matching_engine.h      # PriceLevel, HalfBook, Book, MatchingEngine
│   │   ├── thread.h               # launch_thread() + cross-platform CPU pinning
│   │   ├── log.h                  # compile-time-gated logging macro
│   │   ├── socket_utils.h         # raw socket helpers (WIP)
│   │   └── tcp_socket.h           # batching TCP wrapper (WIP)
│   └── tests/                     # GoogleTest unit tests + Google Benchmark microbenchmarks
│       ├── spsc_queue.cpp / spsc_queue_bench.cpp
│       ├── mempool.cpp / mempool_bench.cpp
│       ├── matching_engine.cpp
│       └── thread.cpp
├── docs/                          # component design docs (this is where the detail lives)
└── .github/workflows/             # CI: unit tests + benchmark, run on every push
```


---

## Building & running

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Dependencies (GoogleTest, Google Benchmark) are fetched automatically via CMake's
`FetchContent` — no manual setup needed for those. The benchmark target additionally
links against [gperftools](https://github.com/gperftools/gperftools) (`profiler`) for
CPU profiling support; on Debian/Ubuntu this is `libgoogle-perftools-dev`.

**Run the unit tests:**

```bash
cd build
ctest --output-on-failure
```

**Run the benchmarks:**

```bash
./build/header/tests/benchmarks
```

Filter to a specific benchmark with `--benchmark_filter=<regex>`, e.g.
`--benchmark_filter=Mempool` or `--benchmark_filter=SPSC_CROSS_CORE`.

---

## CI

Every push runs two GitHub Actions workflows:

- **`.github/workflows/tests.yml`** — builds the project and runs the full GoogleTest
  suite via `ctest`.
- **`.github/workflows/spsc_q.yml`** — builds and runs the Google Benchmark suite (for
  visibility into perf trends over time; not currently a pass/fail gate).

---

## A note on where the performance numbers stand

You'll notice the memory pool and SPSC queue docs describe design intent and correctness
testing in detail, but are deliberately light on hard performance numbers. That's
intentional — see the "Testing status" section in each doc for why, and what's still
left to nail down before those numbers are trustworthy enough to publish here.
