# SPSC Queue (`SPSCQueue<T>`)

`header/libraries/spsc_queue.h`

## Why this exists

Different parts of the system run on different threads by design, for example, the
matching engine's core loop and whatever's talking to the network shouldn't share a
thread, or a slow network write could stall order matching. Those threads need to hand
data to each other. 

Using a lock to protect a queue means every push/pop can potentially block on the OS scheduler:
(lock() tries to test_and_set an atomic in userspace, but if it fails, switches to kernel mode
and the thread sleep before it goes into a waiting queue. once signaled, it must wait to be scheduled and contend for the lock). 
This is slow and unpredicatable and is avoided by using a lock-free data structure.

For the specific case of exactly one writer and one reader, you don't need a lock at
all: a ring buffer with atomic head/tail indices gives you correct, push/pop
as long as the single-producer/single-consumer contract is respected (only one thread
ever calls the "push" side, only one ever calls the "pop" side).

## Design

```cpp
struct Consumer { std::atomic<uint64_t> head_ptr; uint64_t cached_tail; }; // consumer-owned (pop, top)
struct Producer { std::atomic<uint64_t> tail_ptr; uint64_t cached_head; }; // producer-owned (emplace, push)
struct Queue    { T* buffer; uint64_t capacity; };
```

- **Wasted-slot capacity scheme.** The queue allocates `cap + 1` slots internally for a
  caller-requested capacity of `cap`. "Full" is detected when "the next tail position
  would equal the current head". Instead of using a separate counter that would need to
  be sychronized between both sides, we can use an empty slot to indicate fullness.


- **False-sharing avoidance.** `Consumer` and `Producer` are each `alignas(cache_line)`,
  so the producer's writes to `tail_ptr` and the consumer's writes to `head_ptr` never
  bounce the same cache line back and forth between cores. True sharing can't be avoided.


### Memory ordering
- The producer updates `tail_ptr` with `memory_order_release` after writing the
  element. The consumer reads `tail_ptr` with `memory_order_acquire` before reading that
  element. This release/acquire pair is what makes the element's data visible to the
  consumer once it observes the updated index. Without it, the consumer could see the
  new index but stale (or torn) element data. The ordering acts like 2 1-way fences preventing
  instruction and execution reordering.
- The symmetric case applies to `head_ptr` for the producer to know when a slot has been
  freed by the consumer.
- Everything else (local index arithmetic, buffer indexing) uses
  `memory_order_relaxed`, since there's no cross-thread visibility requirement for those.
  Using `seq_cst` everywhere would cost more than this design needs to pay for
  correctness. 
- **Index caching.** Rather than doing an `acquire` load of the opposite side's index on
  every single push/pop, each side keeps a local cached copy and only rereads the atomic
  when the cached value suggests the queue is full (producer) or empty (consumer). This
  cuts atomic loads, and the cross-core cache traffic that comes with them on the hot path.


## Testing status
**Correctness: thoroughly tested.** `header/tests/spsc_queue.cpp` covers basic
push/pop, empty-queue behavior, capacity limits (inserting beyond capacity is dropped,
not undefined), wraparound after a partial drain, multiple repeated wraparounds, a
full-drain-then-refill cycle, `push_back`'s move semantics, and the capacity-1 edge
case.

**Cross-core performance: benchmark exists, numbers are not finalized.**
`header/tests/spsc_queue_bench.cpp` includes `SPSC_CROSS_CORE`, which pins a real
producer thread and a real consumer thread to separate cores (via `launch_thread`/
`set_thread_affinity`) and times a large number of push/pop pairs using the CPU's
timestamp counter (`rdtsc`/`rdtscp`), with `Repetitions(20)` and aggregate reporting
(mean/median/stddev/coefficient of variation).

**Why those numbers aren't published in this doc yet** 

2. **No hardware performance-counter access on the development machine.** `perf stat`
   reports `cycles`/`instructions`/`branch-misses` as `<not supported>` in the current
   dev environment, so there's no independent hardware-level cross-check against the
   `rdtsc`-based cycle counts.
3. **Development so far has happened on a shared, virtualized machine.** vCPU-to-
   physical-core placement is controlled by the hypervisor and invisible from inside the
   guest — "pinned to core 2" at the guest OS level doesn't guarantee a stable physical
   core underneath, which directly undermines a benchmark specifically trying to measure
   cross-*physical*-core cache-coherence cost. The timestamp counter itself may also be
   virtualized rather than passed through, which would affect whether `rdtsc`-based
   cycle counts map cleanly to real time.
4. **Early runs showed unexplained variance** — including one run roughly an order of
   magnitude slower than the rest with tight internal consistency (i.e., not just noisy
   outliers within that run), and a ~2x spread between the fastest and slowest "normal"
   runs. That's not yet root-caused.



**What testing will look like once these are resolved:**
1. Fix and re-verify thread affinity (confirm actual core placement, not just "no error
   was returned").
2. Re-run with a larger repetition count and characterize run-to-run variance
   explicitly, not just within-run variance.
3. Add a `LockedQueue` baseline run through the same cross-core harness, for a direct
   lock-free vs. mutex-based comparison.
4. Vary queue capacity to see how wraparound frequency affects throughput.
5. Run on non-virtualized hardware if available, since several of the open issues above
   are specifically hypervisor-related and may simply not apply there. 


