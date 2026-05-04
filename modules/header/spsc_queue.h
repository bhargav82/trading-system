#include <deque>
#include <mutex>

// Normal thread-safe queue using locks
// No condition variable -> 
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