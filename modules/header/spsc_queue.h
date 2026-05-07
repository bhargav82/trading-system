#include <deque>
#include <mutex>

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
       
    };

    SPSCQueue() = delete;
    SPSCQueue(const SPSCQueue& other) = delete;
    SPSCQueue(SPSCQueue&& other) = delete;
    SPSCQueue& operator=(const SPSCQueue& other) = delete;
    SPSCQueue& operator=(SPSCQueue&& other) = delete;

    
    template <Args ...args>
    void emplace(Args&& ...args) {
        // inplace construction of object
    }

    inline [[nodiscard]] bool try_emplace() { 
        // check if object can be placed in (not full)
        return !(is_full);
    }
    

    template<tpyename ...Args>
    void push(Args&& ...args) {
        // try to push the object into the end of the queue
        // check if the queue is full first
        // no inplace construction, build object, then push using forwarding 
    }

    inline [[nodiscard]] bool try_push() {
        return !(is_full);
    }

    T* front() {
        // return the front of the queue if not empty
        if (!is_empty()) {
            return buffer[pop_ptr];
        }

        return nullptr;
    }

    T& pop() {
        T* top = front();

        //TODO: check if this will destroy the obhect
        if (!top) {
            buffer[pop_ptr++]->T~();
        }

        return *top; 
    }


    inline bool is_full() {
        return sz == capacity;
    }

    inline bool is_empty() {
        return push_ptr - pop_ptr == 0;
    }

    SPSCQueue~() {
        for (size_t i = 0; i < sz; ++i) {
            if (!std::is_triviailly_destructible(cap[i])) {
                buffer[i]->T~();
            }
        }

        delete[] buffer;
    }


private:
    T* buffer;
    size_t push_ptr;
    size_t pop_ptr;
    size_t capacity;
    size_t sz;
};