#include "../libraries/mempool.h"
#include <gtest/gtest.h>
#include <iostream>
#include <string>

// Don't even use this, used this to test first
// Moved first to price level

int global_count = 0;
struct Object {

    int val;

    Object() = delete;
    Object(int v) : val(v) {};
   
};

TEST(Mempool, FirstPtrTest) {
      std::array<MemoryPoolHeap<Object>, 3> pools = {{
        MemoryPoolHeap<Object>(2),
        MemoryPoolHeap<Object>(3),
        MemoryPoolHeap<Object>(4)
    }};

    // Fill in first array
    pools[0].construct(1);

    // // Fill in second array
    pools[1].construct(2);
    pools[1].construct(3);

    // // Fill in third array
    pools[2].construct(4);
    pools[2].construct(5);
    pools[2].construct(6);
    

    int first_val = pools[0].first_ptr()->val;
    int second_val = pools[1].first_ptr()->val;
    int third_val = pools[2].first_ptr()->val;


    EXPECT_EQ(first_val, 1);
    EXPECT_EQ(second_val, 2);
    EXPECT_EQ(third_val, 4);
}

TEST(Mempool, DestructAndFirst) {
    std::array<MemoryPoolHeap<Object>, 3> pools = {{
        MemoryPoolHeap<Object>(2),
        MemoryPoolHeap<Object>(3),
        MemoryPoolHeap<Object>(4)
    }};

    // Fill in first array
    auto first = pools[0].construct(1);

    // // Fill in second array
    auto second = pools[1].construct(2);
    pools[1].construct(3);

    // // Fill in third array
    auto third = pools[2].construct(4);
    auto fourth = pools[2].construct(5);
    pools[2].construct(6);
    
    

    pools[0].destruct(first);
    EXPECT_EQ(pools[0].first_ptr(), nullptr);

    pools[1].destruct(second);
    EXPECT_EQ(pools[1].first_ptr(), 3);

    pools[2].destruct(third);
    pools[2].destruct(fourth);
    EXPECT_EQ(pools[2].first_ptr(), 6);


    // what happens if we try to delete a pointer that doesn't belong to this array?
    // what should happen -> shouldn't be allowed right, how can you enforce if all these array are the same and destruct takes a pointer to object

}
