# SPSC Queue (`SPSCQueue<T>`)

`header/libraries/spsc_queue.h`

## Why this exists

Different parts of the system run on different threads by design, for example, the
matching engine's core loop and the servers handling order ingestion and broadcasting
updates shouldn't share a thread, or a slow network operation could stall order matching.
Instead, those components should run on separate threads, but still need to share data. 

Using a lock to protect a queue means every `push`/`pop` can potentially block on the OS scheduler.
A typical `lock()` first attempts to acquire the lock by performing an atomic `test_and_set` in userspace. 
If the lock is already held, the thread may enter the kernel and go to sleep while waiting for the lock to 
become available. Once it is signaled, the thread still has to wait to be scheduled and then contend for the lock again.

This introduces scheduling overhead and makes latency less predictable. For the specific case of exactly one writer and one reader, 
no lock is needed at all. A ring buffer with atomic `head` and `tail` indices can provide correct `push`/`pop` operations as long 
as the single-producer/single-consumer (SPSC) contract is respected: only one thread ever calls the `push` side, only one ever calls the `pop` side.



## Design

```cpp
struct Consumer { std::atomic<uint64_t> head_ptr; uint64_t cached_tail; }; // consumer-owned (pop, top)
struct Producer { std::atomic<uint64_t> tail_ptr; uint64_t cached_head; }; // producer-owned (emplace, push)
struct Queue    { T* buffer; uint64_t capacity; };
```

- **Wasted-slot capacity scheme:** The queue allocates `cap + 1` slots internally for a
  caller-requested capacity of `cap`. `Full`: (tail + 1) % capacity == head. `Empty`: (tail == head).
  This design reserves a slot to create a distinction between empty and full, instead of using an extra atomic variable
  keeping track of size. An extra atomic variable would need to be synchronized with every operation, degrading performance.


- **False-sharing avoidance:** `Consumer` and `Producer` are each `alignas(cache_line)`,
  so the producer's writes to `tail_ptr` and the consumer's writes to `head_ptr` never
  bounce the same cache line back and forth between cores. 


- **Memory ordering:**
   - The producer writes the element, then updates `tail_ptr` with `memory_order_release`. 
     The consumer reads `tail_ptr` with `memory_order_acquire` before reading the element. This
      guarantees that once the consumer sees the new index, it also sees the element data. If a more
      relaxed ordering was used, the consumer could see the new index but stale element data. 
   - The symmetric case applies to `head_ptr` for the producer to know when a slot has been
      freed by the consumer.
   - All local index arithmetic and buffer indexing use `memory_order_relaxed` since they don't
   require cross-thread synchronization. Using `seq_cst` everywhere would impose stronger ordering
   than necessary, degrading performance.


- **Index caching.** Rather than doing an `acquire` load of the opposite side's index on
  every single push/pop, each side keeps a local cached copy and only rereads the atomic
  when the cached value suggests the queue is full (producer) or empty (consumer). This
  cuts atomic loads, and the cross-core cache traffic that comes with them on the hot path.

To see more information about how memory ordering and false sharing affects performance see:
**➡️ [docs/CONCEPTS.md](CONCEPTS.md)**



## Benchmarks

### Lock-Free SPSC Queue vs. Mutex-Guarded Queue

Single-threaded, **uncontended** microbenchmarks, no thread was ever waiting
on the lock. This measures the fixed per-call overhead of lock acquisition/release,
not contention cost.

| Operation | Lock-Guarded Queue | Lock-Free SPSC Queue | Speedup |
|-----------|---------------------|------------------------|---------|
| Emplace   | 6087 ns             | 1484 ns                | **~4.1x** |
| Pop       | 5812 ns             | 976 ns                 | **~6.0x** |

Even in the best case for the mutex (zero contention), the lock-free queue is
4–6x faster per call. Under real contention, the gap is expected to widen
further, since a blocking `std::mutex` typically falls back to a syscall
and OS-level context switch when a thread loses the race. A lock-free queue does not 
have this same loss. A contended, multi-threaded benchmark is planned to validate this.


**Correctness: thoroughly tested.** `header/tests/spsc_queue.cpp` covers basic
push/pop, empty-queue behavior, capacity limits (inserting beyond capacity is dropped,
not undefined), wraparound after a partial drain, multiple repeated wraparounds, a
full-drain-then-refill cycle, `push_back`'s move semantics, and the capacity-1 edge
case.

**Cross-core performance: benchmark exists, numbers are not finalized.**
`header/tests/spsc_queue_bench.cpp` includes `SPSC_CROSS_CORE`, which pins a real
producer thread and a real consumer thread to separate cores (via `launch_thread`/
`set_thread_affinity`) and times 1 million push/pop ops using the CPU's
timestamp counter (`rdtsc`/`rdtscp`).

**Why those numbers aren't published in this doc yet** 
**➡️ [README.md/NextSteps](../README.md#Next-Steps)**



# SPSC Queue (`SPSCQueue<T>`)

`header/libraries/spsc_queue.h`

## Why this exists

Different parts of the system run on different threads by design, for example, the
matching engine's core loop and the servers handling order ingestion and broadcasting
updates shouldn't share a thread, or a slow network operation could stall order matching.
Instead, those components should run on separate threads, but still need to share data. 

Using a lock to protect a queue means every `push`/`pop` can potentially block on the OS scheduler.
A typical `lock()` first attempts to acquire the lock by performing an atomic `test_and_set` in userspace. 
If the lock is already held, the thread may enter the kernel and go to sleep while waiting for the lock to 
become available. Once it is signaled, the thread still has to wait to be scheduled and then contend for the lock again.

This introduces scheduling overhead and makes latency less predictable. For the specific case of exactly one writer and one reader, 
no lock is needed at all. A ring buffer with atomic `head` and `tail` indices can provide correct `push`/`pop` operations as long 
as the single-producer/single-consumer (SPSC) contract is respected: only one thread ever calls the `push` side, only one ever calls the `pop` side.



## Design

```cpp
struct Consumer { std::atomic<uint64_t> head_ptr; uint64_t cached_tail; }; // consumer-owned (pop, top)
struct Producer { std::atomic<uint64_t> tail_ptr; uint64_t cached_head; }; // producer-owned (emplace, push)
struct Queue    { T* buffer; uint64_t capacity; };
```

- **Wasted-slot capacity scheme:** The queue allocates `cap + 1` slots internally for a
  caller-requested capacity of `cap`. `Full`: (tail + 1) % capacity == head. `Empty`: (tail == head).
  This design reserves a slot to create a distinction between empty and full, instead of using an extra atomic variable
  keeping track of size. An extra atomic variable would need to be synchronized with every operation, degrading performance.


- **False-sharing avoidance:** `Consumer` and `Producer` are each `alignas(cache_line)`,
  so the producer's writes to `tail_ptr` and the consumer's writes to `head_ptr` never
  bounce the same cache line back and forth between cores. 


- **Memory ordering:**
   - The producer writes the element, then updates `tail_ptr` with `memory_order_release`. 
     The consumer reads `tail_ptr` with `memory_order_acquire` before reading the element. This
      guarantees that once the consumer sees the new index, it also sees the element data. If a more
      relaxed ordering was used, the consumer could see the new index but stale element data. 
   - The symmetric case applies to `head_ptr` for the producer to know when a slot has been
      freed by the consumer.
   - All local index arithmetic and buffer indexing use `memory_order_relaxed` since they don't
   require cross-thread synchronization. Using `seq_cst` everywhere would impose stronger ordering
   than necessary, degrading performance.


- **Index caching.** Rather than doing an `acquire` load of the opposite side's index on
  every single push/pop, each side keeps a local cached copy and only rereads the atomic
  when the cached value suggests the queue is full (producer) or empty (consumer). This
  cuts atomic loads, and the cross-core cache traffic that comes with them on the hot path.

To see more information about how memory ordering and false sharing affects performance see:
**➡️ [docs/CONCEPTS.md](CONCEPTS.md)**



## Benchmarks

### Lock-Free SPSC Queue vs. Mutex-Guarded Queue

Single-threaded, **uncontended** microbenchmarks, no thread was ever waiting
on the lock. This measures the fixed per-call overhead of lock acquisition/release,
not contention cost.

| Operation | Lock-Guarded Queue | Lock-Free SPSC Queue | Speedup |
|-----------|---------------------|------------------------|---------|
| Emplace   | 6087 ns             | 1484 ns                | **~4.1x** |
| Pop       | 5812 ns             | 976 ns                 | **~6.0x** |

Even in the best case for the mutex (zero contention), the lock-free queue is
4–6x faster per call. Under real contention, the gap is expected to widen
further, since a blocking `std::mutex` typically falls back to a syscall
and OS-level context switch when a thread loses the race. A lock-free queue does not 
have this same loss. 


**Correctness: thoroughly tested.** `header/tests/spsc_queue.cpp` covers basic
push/pop, empty-queue behavior, capacity limits (inserting beyond capacity is dropped,
not undefined), wraparound after a partial drain, multiple repeated wraparounds, a
full-drain-then-refill cycle, `push_back`'s move semantics, and the capacity-1 edge
case.

---

### Cross-core performance: lock-free SPSC vs. mutex-guarded queue, on real hardware

The single-threaded numbers above measure uncontended per-call overhead. However, this queue's
use case is when two different threads on two different physical cores hand data back and forth.

`SPSC_CROSS_CORE` and `LQ_CROSS_CORE` each pin a real producer thread and a real
consumer thread to separate physical cores and drive 1,000,000 pushes and 1,000,000
pops (2,000,000 total queue operations) per benchmark repetition.


#### Methodology

| | |
|---|---|
| **CPU** | Intel Core i7-13700 (13th Gen "Raptor Lake") |
| **Cores used** | Two separate producer and consumer never share a core or a hyperthread sibling |
| **Virtualization** | None — bare-metal Linux |
| **Governor / frequency** | `performance` governor, frequency locked to max |
| **Cache topology** | L1d 48 KiB, L1i 32 KiB, L2 2048 KiB — per-core; L3 30720 KiB shared |
| **Build** | CMake `Release` (`-O3`) |

```bash
perf stat -r 10 \
  -e task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,\
branches,branch-misses,cache-references,cache-misses,dTLB-load-misses,iTLB-load-misses \
  ./build/header/tests/benchmarks --benchmark_filter=SPSC_CROSS_CORE

perf stat -r 10 \
  -e task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,\
branches,branch-misses,cache-references,cache-misses,dTLB-load-misses,iTLB-load-misses \
  ./build/header/tests/benchmarks --benchmark_filter=LQ_CROSS_CORE
```


#### Latency & throughput
| | Mutex-guarded queue | Lock-free SPSC | Improvement |
|---|---|---|---|
| **Throughput** | ~389,332.54 ops/sec (~194,666.14 msg/sec) | ~23.9M ops/sec (~11.8M msg/sec) | **~60x** |
| **Latency / op** | ~2568.3 ns | ~42.5 ns | **~60x** |


The cross-core, real-contention gap (~60x) is substantially larger than the earlier
single-threaded, uncontended gap (~4–6x, see table above). This benchmark adds kernel involvement and
cross-core cache-line contention.

#### Hardware counters

| Metric | Mutex-guarded queue | Lock-free SPSC |
|---|---|---|
| Context switches | 0 | 0 |
| CPU migrations | 0 | 0 |
| Page faults (all minor) | 241 | 169 |
| Cycles | 7,219,237,822 | 465,970,017 |
| Instructions | 5,028,956,758 | 1,110,641,506 |
| **IPC** | **0.70** | **2.38** |
| Branches | 1,490,776,383 | 273,171,463 |
| Branch misses | 11,963,856 (0.80% of branches) | 1,804,459 (0.66% of branches) |
| Cache references (LLC) | 153,995 | 90,212 |
| Cache misses (LLC) | 20,383 (13.24% of refs) | 11,346 (12.58% of refs) |
| dTLB load misses | 6,003 | 1,937 |
| iTLB load misses | 8,196 | 1,593 |



Zero context switches and zero CPU migrations on both benchmarks confirm the physical
pinning held for the entire run. The lock-free queue does not have a lock-acquisition
branch to predict, which accounts for the fewer branches and branch misses. 
