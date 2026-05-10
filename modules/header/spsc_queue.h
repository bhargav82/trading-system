#include <deque>
#include <mutex>
#include <iostream>

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
    explicit SPSCQueue(size_t cap) : buffer(new T[cap]), push_ptr(0), pop_ptr(0), capacity(cap), sz(0) {
        
    }

    SPSCQueue() = delete;
    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue(SPSCQueue&& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(SPSCQueue&& other) = delete;

    
    template <typename ...Args>
    void emplace(Args&& ...args) {
        if (try_emplace()) {
            new (buffer + push_ptr++) T(std::forward<Args>(args)...);

            std::cout << "Inserted at position: " << push_ptr - 1 << "\n";
            if (push_ptr == capacity) [[unlikely]] {
                push_ptr = 0;
            }
            ++sz;
            
        } else {
            std::cout << "Full queue, could not insert at " << push_ptr << "\n";
        }
    }

    [[nodiscard]] inline bool try_emplace() { 
        return !(is_full());
    }
    

    void push(const T& other) {
        if (try_push()) {
            if (push_ptr == capacity) [[unlikely]] {
                push_ptr = 0;
            }
            buffer[push_ptr++] = other;
            ++sz;
        }
    }

    [[nodiscard]] inline bool try_push() {
        return !(is_full());
    }

    T* front() {
        // return the front of the queue if not empty
        if (!is_empty()) {
            return (buffer + pop_ptr);
        }

        return nullptr;
    }

    T& pop() {
        T* top = front();

        //TODO: check if this will destroy the obhect
        if (top) {
            (buffer + pop_ptr)->~T();
            std::memset(buffer + pop_ptr, 0, sizeof(T));
            ++pop_ptr;
            if (pop_ptr == capacity) [[unlikely]] {
                pop_ptr = 0;
            } 
            --sz;
            std::cout << "Popped the top off" << "\n";
        }

        return *top; 
    }


    inline bool is_full() {
        return sz == capacity;
    }

    inline bool is_empty() {
        return push_ptr - pop_ptr == 0 && !sz;
    }

    ~SPSCQueue() {
        if (!std::is_trivially_destructible_v<T>) {
            for (size_t i = 0; i < sz; ++i) {
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
    T* buffer;
    size_t push_ptr;
    size_t pop_ptr;
    size_t capacity;
    size_t sz;
};