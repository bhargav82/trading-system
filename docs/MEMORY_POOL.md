# Memory Pool (`MemoryPoolHeap<T>`)

`header/libraries/mempool.h`

## Why this exists

The order book needs to create and destroy `Order` objects constantly -> every new
order, every fill, every cancel touches the allocator. Going through `new`/`delete` (or
`malloc`/`free`) for each of those is a real cost:

- **Non-deterministic latency.** The general-purpose allocator's internal bookkeeping
  (finding a free block, coalescing, occasionally calling into the OS for more memory)
  doesn't take a fixed amount of time. That variance is exactly what you don't want on
  a path that's supposed to be predictable.
- **Heap fragmentation over a long-running process.** A matching engine is meant to run
  for a full trading session (or longer); repeated alloc/free of similarly-sized objects
  is a classic fragmentation pattern.
- **Cache locality.** A pool's objects live in one contiguous block, which plays nicer
  with the cache than objects scattered across whatever the allocator handed back.

The fix: allocate a fixed-size block once, up front, and hand out/reclaim fixed-size
slots from it for the lifetime of the pool. No calls into the general-purpose allocator
on the hot path at all.

## Design

```cpp
template <typename T>
class MemoryPoolHeap final {
    T* buffer;                     // one contiguous block, sized for `n` objects
    std::vector<bool> is_free_list; // occupancy bitmap
    size_t next_free;               // where to look first for the next open slot
    size_t sz;                      // total slot count
};
```

- **Allocation.** The constructor allocates `n * sizeof(T)` bytes once via
  `::operator new` to prevent default construction of T objects and never touches the
  allocator again for the pool's lifetime.
- **`construct(args...)`** finds the next free slot, placement-news a `T` into it with
  the forwarded arguments, marks the slot occupied, and advances `next_free` to the next
  open slot it can find.
- **`destruct(ptr)`** looks up which slot `ptr` corresponds to (via pointer
  subtraction), calls `~T()` on it explicitly, and marks the slot free again.
- **Round-robin free-slot search**, not a LIFO free list. `next_free` just keeps
  scanning forward (wrapping around) until it lands on a free slot, a linear scan in the worst case
  when the pool is nearly full. For the pool sizes this project uses (hundreds to a few
  thousand slots per price level), that trade-off is a non-issue.
- **Wasted-slot capacity scheme.** A pool constructed with size `n` supports `n - 1`
  live objects at once — the same trick the [SPSC queue](SPSC_QUEUE.md) uses to detect
  "full" without a separate counter that both the fill and drain paths would need to
  keep in sync. The call that would consume the very last slot throws
  `std::runtime_error` instead of silently succeeding.
- **Non-copyable, non-movable.** The pool owns raw memory directly, moving would
  double-allocate and copying would have two owners of the same buffer. 
  Both are explicitly deleted.
- **Destructor cleans up whatever's still live.** If the pool goes out of scope with
  objects still constructed in it, the destructor walks the occupancy list and calls
  `~T()` on anything still marked in-use before freeing the buffer.


## Testing status

**Correctness: tested.** `header/tests/mempool.cpp` covers:
- basic construct/destruct and value correctness across multiple simultaneous slots
- destructor actually running (verified via a live-instance counter on a tracked type)
- error handling — double-destruct and out-of-range pointers both throw
  `std::runtime_error` rather than corrupting state
- the wasted-slot capacity limit (construct throws once the pool is exhausted)
- reuse after a full drain, and repeated construct/destruct cycles, to check for leaks



**Speed: a first comparison exists, but treat it as directional only.**
`header/tests/mempool_bench.cpp` uses Google Benchmark to compare plain `new`/`delete`
against pool-backed `construct`/`destruct`, both for a single object per iteration and
for a small batch (closer to how a pool would actually be used).
This is intentionally a *relative* comparison, not a search for exact
numbers —> the goal is just to confirm the pool is meaningfully cheaper than going
through the allocator.

**Why full performance numbers aren't published in this doc yet:**
- The benchmark above exercises the pool in isolation, not through the actual order
  book code path (`PriceLevel::add_order`/`remove_order`) — an end-to-end number through
  real matching-engine usage will look different (and matters more) than a
  microbenchmark of `construct`/`destruct` alone.
- Development so far has happened on a shared, virtualized development machine — the
  same environment where the SPSC cross-core benchmark turned up measurement
  reliability concerns (see [SPSC_QUEUE.md](SPSC_QUEUE.md#testing-status) for the full
  list). The same caveats apply here until this is re-run somewhere more controlled.

**What testing will look like once it happens for real:**
1. Benchmark the pool through the actual `PriceLevel::add_order`/`remove_order` path,
   not just `construct`/`destruct` directly.
2. Add `Repetitions(N)` with aggregate reporting (mean/median/stddev/CV) instead of a
   single sample, matching the methodology already used for the SPSC queue.
3. Run on non-virtualized hardware if available, to get numbers that are trustworthy in
   an absolute sense and not just relative to each other.
4. Publish the resulting numbers, with the methodology and environment noted
   explicitly, in this doc.



### Memory Pool vs. Heap Allocation (`new`/`delete`)

Single-threaded, uncontended microbenchmarks comparing a preallocated,
free-list-backed memory pool against standard heap allocation.

| Benchmark                  | Time      | Iterations   | Speedup vs. heap |
|-----------------------------|-----------|--------------|-------------------|
| `Mempool_NewDelete`         | 34.1 ns   | 19,305,552   | baseline          |
| `Mempool_Preallocated`      | 6.94 ns   | 100,784,681  | **~4.9x**         |
| `Mempool_NewDelete_Batch` (32 objs)    | 2439 ns   | 285,420      | baseline          |
| `Mempool_Preallocated_Batch` (32 objs) | 222 ns    | 3,145,926    | **~11x**          |

The larger speedup in the batch case reflects both avoided allocator overhead
*and* the spatial locality of pulling contiguous slots from the pool, versus
scattered heap addresses from repeated `new` calls incurring additional cache
misses.