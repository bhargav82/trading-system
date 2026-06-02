#include <deque>
#include <mutex>
#include <iostream>
#include <utility>
#include <new>
#include <vector>
#include "log.h"


// Normal thread-safe queue using locks
// Lock_Guard ensures that lock is released at end of scope, even if an exception is thrown
// All operations are blocking, concurrent called serialize on mutex
template <typename T>
class LockedQueue {
private:
    std::deque<T> queue;
    std::mutex m;

public:
    // Pushes a copy of val onto the back of the queue
    void push(const T& val) {
        std::lock_guard<std::mutex> lock(m);
        queue.push_back(val);
    }; 

    // Removes the front element
    void pop() {
        std::lock_guard<std::mutex> lock(m);
        if (!queue.empty()) {
            queue.pop_front();
        }
    };

    // Returns a copy of the front element, without removing it
    T& top() {
        std::lock_guard<std::mutex> lock(m);
        if (!queue.empty()) {
            return queue.at(0);
        }
    };
};


// Use compiler-provided cache line size, fall back to 64 bytes
#if defined(__cpp_lib_hardware_destructive_interference_size)
static constexpr size_t cache_line = std::hardware_destructive_interference_size;
#else
static constexpr size_t cache_line = 64;
#endif


// Lock-free single-producer single-consumer ring buffer.
// Invariants:
//   - head_ptr never overtakes tail_ptr (no reading from empty slots)
//   - tail_ptr never overtakes head_ptr (no overwriting unconsumed slots)
//   - push/emplace called only from the producer thread
//   - pop/top/peek called only from the consumer thread
// Violating the single-thread-per-side constraint breaks the lock-free guarantees.
template <typename T>
class SPSCQueue {
public:

    
    // Allocates cap + 1 slots internally — the extra slot is the "wasted slot"
    // used to distinguish full from empty without a separate counter.
    // The caller-visible capacity is still cap.
    // Requires: cap >= 1.
    explicit SPSCQueue(uint64_t cap) : queue{new T[cap + 1], cap + 1} {}


    SPSCQueue() = delete;                                                       
    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue(SPSCQueue&& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(SPSCQueue&& other) = delete;



    // Requires: queue is not full.
    // Modifies: constructs T in-place at tail, advances tail_ptr.
    // Effects:  forwards args directly into buffer — no copy or move of T.
    template<typename ...Args>
    void emplace_back(Args&& ...args) { 
        std::pair<bool, uint64_t> ret = try_insert();

        if (ret.first) {
            new (queue.buffer + prod.tail_ptr.load(std::memory_order_acquire)) T(std::forward<Args>(args)...);
            LOG("emplace_back: Inserted at position " << prod.tail_ptr);
            prod.tail_ptr.store(ret.second, std::memory_order_release);
        }
    }


    // Requires: queue is not full.
    // Modifies: move-assigns other into buffer at tail, advances tail_ptr.
    // Effects:  prefer emplace_back if constructing from scratch.
    void push_back(T&& other) {
        std::pair<bool, uint64_t> ret = try_insert();

        if (ret.first) {
            queue.buffer[prod.tail_ptr.load(std::memory_order_acquire)] = std::forward<T>(other);
            LOG("push_back: Inserted at position " << prod.tail_ptr);
            prod.tail_ptr.store(ret.second, std::memory_order_release);
        }
    }


    // Requires: nothing.
    // Modifies: may update cached_head from cons.head_ptr if queue appeared full.
    // Effects:  returns {true, next_tail} if space is available, {false, 0} if full.
    //           next_tail is pre-computed so callers can write first, then store atomically.
    [[nodiscard]] inline std::pair<bool, uint64_t> try_insert() {
        uint64_t next = prod.tail_ptr.load(std::memory_order_relaxed) + 1;
        if (next == queue.capacity) [[unlikely]] {
            next = 0;
        }
        
        // Load the atomic value only when necessary (CHECK MEMORY ordering)
        if (next == prod.cached_head) {
            prod.cached_head = cons.head_ptr.load(std::memory_order_acquire);

            if (next == prod.cached_head) {
                LOG("emplace_back: Could not insert (Full queue).");
                return {false, 0};
            }
        }

        return {true, next};
    }


    // Requires: nothing.
    // Modifies: nothing.
    // Effects:  returns pointer to the front element, or nullptr if empty.
    T* top() {
        if (!peek()) {
            return nullptr;
        }
        return queue.buffer + cons.head_ptr.load(std::memory_order_acquire);
    }


    // Requires: nothing.
    // Modifies: calls ~T() and zeroes the slot at head, advances head_ptr.
    // Effects:  no-op if queue is empty.
    void pop() {
        if (top()) {
            uint64_t head = cons.head_ptr.load(std::memory_order_acquire);
            (queue.buffer + head)->~T();
            std::memset(queue.buffer + head, 0, sizeof(T));

            ++head;
            if (head == queue.capacity) [[unlikely]] {
                head = 0;
            }

            cons.head_ptr.store(head, std::memory_order_release);
            LOG("pop: Popped the top off.");
        }
    }

    // Requires: nothing.
    // Modifies: may update cached_tail from prod.tail_ptr if queue appeared empty.
    // Effects:  returns true if at least one element is available to consume.
    [[nodiscard]] bool inline peek() {
        uint64_t head = cons.head_ptr.load(std::memory_order_acquire);
        if (cons.cached_tail == head) {
            cons.cached_tail = prod.tail_ptr.load(std::memory_order_acquire);

            if (cons.cached_tail == head) {
                LOG("peek: Queue is full.");
                return false;
            }
        }
        return true;
    }

    
    // Requires: nothing.
    // Modifies: calls ~T() on all live elements if T is not trivially destructible, frees buffer.
    // Effects:  safe to call even if queue is partially filled.
    ~SPSCQueue() { 
        if (!std::is_trivially_destructible_v<T>) {
            for (uint64_t i = 0; i < queue.capacity; ++i) {
                (queue.buffer + i)->~T();
            }
        }

        delete[] queue.buffer;
    }

    
    // Requires: nothing.
    // Modifies: nothing.
    // Effects:  prints every slot in the buffer including empty ones, debug only.
    void print() {
        for (uint64_t i = 0; i < queue.capacity; ++i) {
            std::cout << "element at " << i << " " << *(queue.buffer + i);
        }
    }

   

    // Drain the whole queue into a vector via top()/pop()
    std::vector<T> drain() {
        std::vector<T> out;
        while (T* p = top()) {
            out.push_back(*p);
            pop();
        }
        return out;
    }

    


private:
    
    // Owned exclusively by consumer thread.
    // Empty when head_ptr == tail_ptr.
    // Advance head pointer only on pop().
    struct alignas(cache_line) Consumer {
        std::atomic<uint64_t> head_ptr{0};    // written by consumer, read by producer
        uint64_t cached_tail{0};              // local snapshot of tail, refresh from producer when queue appears empty
    };

    // Owned exclusively by the producer thread.
    // Full when next(tail_ptr) == head_ptr (wasted-slot scheme).
    // Writes to tail_ptr (back of queue), advancing it forward on each push.
    struct alignas(cache_line) Producer {
        std::atomic<uint64_t> tail_ptr{0};    // written by producer, read by consumer 
        uint64_t cached_head{0};              // local snapshot of head, refrsh from consumer when queue appears full
    };

    struct alignas(cache_line) Queue {
        T* buffer;
        uint64_t capacity;
    };
   
    Consumer cons;
    Producer prod;
    Queue queue;
};
