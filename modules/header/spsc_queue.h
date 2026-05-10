#include <deque>
#include <mutex>
#include <iostream>
#include "log.h"

// Normal thread-safe queue using locks
template <typename T>
class LockedQueue {
private:
    std::deque<T> queue;
    std::mutex m;

public:
    // lock before accessing queue
    void push(const T& val) {
        std::lock_guard<std::mutex> lock(m);
        queue.push_back(val);
    }; // auto unlocks at end of scope

    T& pop() {
        // without using a guard can be dangerous if critical section throws an exception -> thread never releases lock -> deadlock
        m.lock();
        queue.pop_front();
        m.unlock();
    };

    T& top() {
        m.lock();
        queue.at(0);
        m.unlock();
    };
};



// Implement a SPSC circular ring buffer (array), allowing pop and push using indices (no locks)

template <typename T>
class SPSCQueue {
public:
    explicit SPSCQueue(size_t cap) : buffer(new T[cap]), capacity(cap) {};

    SPSCQueue() = delete;
    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue(SPSCQueue&& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(SPSCQueue&& other) = delete;

    
    template <typename ...Args>
    void emplace(Args&& ...args) {
        if (try_insert()) {
            new (buffer + prod.tail_ptr) T(std::forward<Args>(args)...);
            LOG("Emplaced back at position: " << prod.tail_ptr);
        } 

    }

    
    void push(const T& other) {
        if (try_insert()) {
            buffer[prod.tail_ptr] = other;
            LOG("Pushed back at position: " << prod.tail_ptr);
        }
    }

    
    [[nodiscard]] inline bool try_insert() {
        size_t next = prod.tail_ptr + 1;
        if (next == capacity) [[unlikely]] { // skip modulo operation
            next = 0;
        }

        if (next == prod.cached_head) { // check if the queue is full, double check with the loaded 
            prod.cached_head = cons.head_ptr.load();
            if (next == prod.cached_head) { // check memory ordering
                LOG("Could not insert, full queue.");
                return false;
            }
        }

        prod.tail_ptr.store(next); // update the tail ptr with the next ptr, check memory ordering
        return true;
    }


    T* front() {
        // return the front of the queue if not empty, don't update the head ptr
        if (!check_top()) {
            return (buffer + cons.head_ptr);
        }

        return nullptr;
    }

    T& pop() {
        T* top = front();

        if (top) {
            size_t pop_ptr = cons.head_ptr.load();
            (buffer + pop_ptr)->~T();
            std::memset(buffer + pop_ptr, 0, sizeof(T));
            ++pop_ptr;
            if (pop_ptr == capacity) [[unlikely]] {
                pop_ptr = 0;
            }
            cons.head_ptr.store(pop_ptr);
            std::cout << "Popped the top off" << "\n";
        }

        return *top; 
    }

    [[nodiscard]] inline bool check_top() {
        /*
            1. find current position to pop off from
            2. check if this position is equal to cached head, if so double check by loading in actual head
            3. return true if you get past this
        */
        if (cons.head_ptr == cons.cached_tail) {
            cons.cached_tail = prod.tail_ptr.load();
            if (cons.head_ptr == cons.cached_tail) {
                LOG("Can not pop, empty queue.");
                return false;
            }
        }

        return true;
    }



    ~SPSCQueue() {
        if (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < capacity; ++i) {
                (buffer + i)->~T();
            }
        }

        delete[] buffer;
    }

    // may be printing too much 
    void print() {
        for (size_t i = 0; i < capacity; ++i) {
            std::cout << "element at " << i << " " << *(buffer + i);
        }
    }


private:
struct alignas(64) Producer {
    std::atomic<size_t> tail_ptr{0}; // always write to this (don't need to load since there is only 1 producer, will need to store), actually push_ptr
    size_t cached_head{0}; // keep a local cached head that only needs to check if it reaches the capacity, then load it the actual head_ptr, and check if it hit the end
};

struct alignas(64) Consumer {
    std::atomic<size_t> head_ptr{0};
    size_t cached_tail{0};
};

    Producer prod;
    Consumer cons;
    T* buffer;
    size_t capacity;
    
};

// Invariant: Push ptr can never equal pop ptr after it starts, 