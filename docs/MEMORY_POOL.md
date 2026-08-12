# Memory Pool (`MemoryPoolHeap<T>`)

`header/libraries/mempool.h`

## Why this exists

The order book needs to create and destroy `Order` objects constantly, every new
order, every fill, every cancel touches the allocator. Calling `new`/`delete` (or
`malloc`/`free`) for each of those is costly:

- **Non-deterministic latency.** The general-purpose allocator's internal bookkeeping
  (finding a free block and occasionally calling into the OS for more memory) takes a
  variable length of time. 
- **Heap fragmentation over a long-running process.** Repeated alloc/free of similarly-sized 
  objects causes fragmentation in the heap.
- **Cache locality.** A pool's objects live in one contiguous block, which uses the cache
   better than objects scattered across whatever the allocator handed back.

The fix: allocate a fixed-size block once, up front, give and take fixed-size
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
- **`destruct(ptr)`** looks up which slot `ptr` corresponds to, calls `~T()` on it explicitly, 
  and marks the slot free again.
- **Round-robin free-slot search**, not a LIFO free list. `next_free` just keeps
  scanning forward (wrapping around) until it lands on a free slot, a linear scan in the worst case
  when the pool is nearly full.
- **Wasted-slot capacity scheme.** A pool constructed with size `n` supports `n - 1`
  live objects at once (the same trick the [SPSC queue](SPSC_QUEUE.md) uses to detect
  "full" without a separate counter). The call that would consume the very last slot throws
  `std::runtime_error` instead of silently succeeding.
- **Non-copyable, non-movable.** Add this
- **Destructor cleans up whatever's still live.** If the pool goes out of scope with
  objects still constructed in it, the destructor walks the occupancy list and calls
  `~T()` on anything still marked in-use before freeing the buffer.


## Testing status

**Correctness: tested.** `header/tests/mempool.cpp` covers:
- basic construct/destruct and value correctness across multiple simultaneous slots
- error handling — double-destruct and out-of-range pointers both throw
  `std::runtime_error` rather than corrupting state
- the wasted-slot capacity limit (construct throws once the pool is exhausted)
- reuse after a full drain, and repeated construct/destruct cycles, to check for leaks



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

-**Next Steps:**
- The benchmark tests the pool in isolate to gather relative performance numbers, 
  the next step is to use the order book code (`PriceLevel::add_order`/`remove_order`)
- Benchmark on non-virtualized hardware with `perf` to measure cache misses, page faults
  and TLB misses.

