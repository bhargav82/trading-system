#pragma once
#include <vector>
#include <stdlib.h>
#include <iostream>
#include <cassert>


// Mem pool on stack, used when objects are small or there are very few (won't stack overflow)
// template <typename T, size_t N>
// class MemoryPoolStack {
//     T buffer[N];
// };


template <typename T>
class MemoryPoolHeap final {
private:
    T* buffer;
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
        std::byte* temp = new std::byte[n * sizeof(T)];
        buffer = reinterpret_cast<T*>(temp);
        memset(buffer, 0, n);
        is_free_list.resize(n, true);
    };
    // [] [] [] [] []

    // don't allow any other access
    MemoryPoolHeap() = delete;
    MemoryPoolHeap(const MemoryPoolHeap&) = delete;
    MemoryPoolHeap(MemoryPoolHeap&&) = delete;
    MemoryPoolHeap& operator=(const MemoryPoolHeap&) = delete;
    MemoryPoolHeap& operator=(MemoryPoolHeap&&) = delete;


    // use placement new (ptr) T() to construct the object in the next free space, update counter after
    template <typename ...Args>
    [[nodiscard]] T* construct(Args&& ...args) noexcept {
        // 1. Find the address of the next free location
        T* insert_loc = buffer + next_free;

        // 2. construct the T object usign the forwarded args, use placement new to construct the object in place found above
        T* t_ = new (insert_loc) T(std::forward<Args>(args)...);

        // 3. update the bool at this address to be not free
        is_free_list[next_free] = false;

        // 4. update the next free free variable
        update_next_free();

       
        return t_;
    }

    
    void destruct(const T* t_) noexcept {
        // 1. Find the index, by subtracting pointer from state (std::byte so need to device by sizeof(T))
        size_t t_index = t_ - buffer;
        
        // 2. Assert index is valid
        assert(t_index >= 0 && t_index < sz && "t_index is out of range");

        // 3. just need to update is_free of that index and destroy object in that space
        assert(is_free_list[t_index] == false && "t_index is not currently used");
        (buffer + t_index * sizeof(T))->~T();
        is_free_list[t_index] = true;
    }


    ~MemoryPoolHeap() {
        // go through all the objects in free list that = false (meaning there is an object here and call destructor)
        for (size_t i = 0; i < sz; ++i) {
            if (!is_free_list[i]) {
                (buffer + i)->~T();
            }
        }

        // deallocate the byte buffer as well
        delete[] buffer;

    }

};
