#include <vector>
#include <stdlib.h>



// Mem pool on stack, used when objects are small or there are very few (won't stack overflow)
template <typename T, size_t N>
class MemoryPoolStack {
    T buffer[N];
};


template <typename T>
class MemoryPoolHeap final {
private:
    std::byte* buffer;
    std::vector<bool> is_free_list;
    size_t next_free;
    size_t sz;

    void update_next_free() noexcept {
        size_t current_free_idx = next_free;
        while (!is_free_list[next_free]) {
            ++next_free;
            if (next_free == sz) [[unlikely]] { // wrap around
                next_free = 0;
            }
            if (next_free == current_free_idx) [[unlikely]] { // went full circle
                std::cerr << "Ran out of memory in pool" << std::endl;
                std::abort();
            }
        }
    };

public:
    explicit MemoryPoolHeap(size_t n) : next_free(0), sz(n) {
        buffer = new std::byte[n * sizeof(T)];
        memset(buffer, 0, n * sizeof(T));
        is_free_list.resize(n, true);
        
    };

    // don't allow any other access
    MemoryPoolHeap() = delete;
    MemoryPoolHeap(const MemoryPoolHeap&) = delete;
    MemoryPoolHeap(MemoryPoolHeap&&) = delete;
    MemoryPoolHeap& operator=(const MemoryPoolHeap&) = delete;
    MemoryPoolHeap& operator=(MemoryPoolHeap&&) = delete;


    // allocate space and update next available index 
    // use placement new (ptr) T()
    template <typename ...Args>
    T* construct(Args&& ...args) noexcept {
        // 1. Find the address of the next free location
        // 2. construct the T object usign the forwarded args, use placement new to construct the object in place found above
        // 3. update the bool at this address to be not free
        // 4. update the next free free variable
        // 5. return t object
    }

    void destruct(const T* t_) noexcept {
        // 1. Find the index, by subtracting pointer from state (std::byte so need to device by sizeof(T))
        size_t t_index = (t_ - buffer)/sizeof(T);

        // 2. Assert index is valid
        ASSERT(t_index >= 0 && t_index < sz && "t_index is out of range");

        // 3. just need to update is_free of that index and destroy object in that space
        ASSERT(is_free_list[t_index] == false && "t_index is not currently used")
        (buffer + t_index * sizeof(T))->~T();
        is_free_list[t_index] = true;
    }


    ~MemoryPoolHeap() {
        // go through all the objects in free list that = false (meaning there is an object here and call destructor)
        for (size_t i = 0; i < sz; ++i) {
            if (!is_free_list[i]) {
                (buffer + i * sizeof(T))->~T();
            }
        }

        // deallocate the byte buffer as well
        delete[] buffer;

    }

};
