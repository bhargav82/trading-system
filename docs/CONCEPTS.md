### Memory Ordering:
- Compilers may reorder instructions and CPUs may execute out of order. When multiple cores communicate
  through shared memory, this can cause another core to observe operations in an order different than what was intended.
  
- For example, suppose one thread writes some data and then signals the other thread that the data is ready:
   `// Writer (CPU 1)`
   `1. x = 5;`
   `2. ready.store(true, std::memory_order_relaxed);`
   
   `// Reader (CPU 2)`
   `3. while (!ready.load(std::memory_order_relaxed)) {}`
   `4. std::cout << x;`

- We wanted the reader to observe x == 5 after signaled to exit the spin loop. But `memory_order_relaxed` 
  does not enforce ordering between the write to `x` and the store to `ready`.
  
- The operations could be observed as (2, 3, 4, 1):
   `Writer: sets ready to true `
   `Reader: sees ready == true, then reads stale value of x`
   `Writer: sets x to 5`

- `Memory order relaxed` does not enforce ordering, only that the operation happens atomically. To establish the required ordering
   release/acquire must be used:
   `// Writer (CPU 1)`
   `1. x = 5;`
   `2. ready.store(true, std::memory_order_release);`
   
   `// Reader (CPU 2)`
   `3. while (!ready.load(std::memory_order_acquire)) {}`
   `4. std::cout << x;`
   
- Now, once the reader's acquire load observes the value written by the release store, the write to x happens-before the reader's access to x.
  Between threads, a happens-before relationship occurs when using an acquire/release ordering pair. 