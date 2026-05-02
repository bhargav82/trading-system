#include <vector>
#include <stdlib.h>



// Mem pool on stack, used when objects are small or there are very few (won't stack overflow)
template <typename T, size_t N>
class MemoryPoolStack {
    T buffer[N];
};


// mem pool allocated on heap, 1 vector to improve cache hits (free list and obj list)
template <typename T>
class MemoryPoolHeap final {
private:
    struct Object {
        T t_;
        bool is_free;

        Object() t_() : is_free(true) {};
    };
    std::vector<Object> buffer;
    size_t next_free;

public:
    explicit MemoryPoolHeap(size_t n) : buffer(n), next_free(0) {
        // make sure that the first object in the struct is actually the t object -> ensures we deallocate the right members
        ASSERT(reinterpret_cast<Object*>&((buffer[0].t_)) == &(buffer[0]));
    };

    // don't allow any other access
    MemoryPoolHeap() = delete;
    MemoryPoolHeap(const MemoryPoolHeap&) = delete;
    MemoryPoolHeap(MemoryPoolHeap&&) = delete;
    MemoryPoolHeap& operator=(const MemoryPoolHeap&) = delete;
    MemoryPoolHeap& operator=(MemoryPoolHeap&&) = delete;

};
