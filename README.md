# Order Matching Engine

A low-latency order matching engine built from scratch in modern C++, focused on the core
infrastructure pieces a matching engine needs: a lock-free SPSC queue for cross-thread
messaging, a custom memory pool to keep order allocation off the general-purpose
allocator, and a limit order book with price-time priority.

---

## Components

| Component | Status | Docs |
|---|---|---|
| **SPSC Queue** — lock-free single-producer/single-consumer ring buffer | Implemented, unit tested, benchmarked cross-core on real hardware with `perf` counters — **~60x** faster than a mutex-guarded queue in both latency and throughput | [docs/SPSC_QUEUE.md](docs/SPSC_QUEUE.md) |
| **Memory Pool** — preallocated fixed-slot allocator | Implemented, unit tested, initial speed comparison shows **~5–11x** over `new`/`delete`| [docs/MEMORY_POOL.md](docs/MEMORY_POOL.md) |
| **Order Book / Matching Engine** — price-time priority limit order book | Core matching logic implemented and tested | [docs/ORDER_BOOK.md](docs/ORDER_BOOK.md) |
| **TCP socket layer** (`socket_utils.h`, `tcp_socket.h`) | Work in progress | — |

To see the specific engineering techniques used across the codebase (lock-free
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
├── docs/                          # component design docs
│   └── images/                    # perf stat screenshots referenced by the docs
└── .github/workflows/             # CI: unit tests + benchmark, run on every push
```


---

## Building & running

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Dependencies (GoogleTest, Google Benchmark) are fetched automatically via CMake's
`FetchContent`. 

**Run the unit tests:**

```bash
cd build
ctest --output-on-failure
```

**Run the benchmarks:**

```bash
./build/header/tests/benchmarks
```

Filter to a specific benchmark with `--benchmark_filter=<benchmark_name>`, 
e.g. `--benchmark_filter=Mempool` or `--benchmark_filter=SPSC_CROSS_CORE`.

---

## CI

Every push runs two GitHub Actions workflows:

- **`.github/workflows/tests.yml`** — builds the project and runs the full GoogleTest
  suite via `ctest`.
- **`.github/workflows/spsc_q.yml`** — builds and runs the Google Benchmark suite.

---

## Performance highlights
**Single-Producer Single-Consumer Lock Free Queue**
**➡️ [docs/SPSC_QUEUE.md](docs/SPSC_QUEUE.md#cross-core-performance-lock-free-spsc-vs-mutex-guarded-queue-on-real-hardware)**

**Preallocated Memory Pool**
**➡️ [docs/MEMORY_POOL.md](docs/MEMORY_POOL#Memory-Pool-vs-Heap-Allocation-(`new`/`delete`))**



## Future Plans

1. **Investigate the dTLB/iTLB miss gap between the SPSC and mutex-guarded queue.**
   The mutex-guarded queue sees ~3x more dTLB load misses and ~5x more iTLB load
   misses per operation than the lock-free queue. The likely explanation is the extra 
   kernel code and data touched on the mutex's contended path thrashing the TLB.
2. **Run the memory pool through `perf` on real hardware.** The current memory pool
   numbers (`docs/MEMORY_POOL.md`) are from an uncontended microbenchmark comparing
   `construct`/`destruct` against `new`/`delete`. The next step is capturing `perf stat` 
   cache-reference/cache-miss, page-fault, and TLB-miss counters for the pool the same way, ideally
   through the real `PriceLevel::add_order`/`remove_order` path rather than the pool in isolation.
3. Build the TCP server to receive order requests from clients and send back responses.
4. Build the order-updates server using multicast UDP to broadcast order updates, so
   clients can maintain an accurate book.