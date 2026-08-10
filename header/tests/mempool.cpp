#include "../libraries/mempool.h"
#include <gtest/gtest.h>
#include <stdexcept>
#include <vector>


// use a class to track how many live objects there are of this type
struct TrackedObject {
    static int alive_count;
    int val;

    TrackedObject() = delete;
    explicit TrackedObject(int v) : val(v) { 
        ++alive_count; 
    }
    ~TrackedObject() { 
        --alive_count; 
    }
};
int TrackedObject::alive_count = 0;





// Test construct/destruct
TEST(MempoolTest, ConstructReturnsUsableObject) {
    MemoryPoolHeap<TrackedObject> pool(4);
    TrackedObject* obj = pool.construct(42);
    ASSERT_NE(obj, nullptr);
    EXPECT_EQ(obj->val, 42);
    EXPECT_EQ(TrackedObject::alive_count, 1);
    pool.destruct(obj);
    EXPECT_EQ(TrackedObject::alive_count, 0);
}

TEST(MempoolTest, MultipleConstructsGetDistinctSlots) {
    MemoryPoolHeap<TrackedObject> pool(4);
    auto a = pool.construct(1);
    auto b = pool.construct(2);
    auto c = pool.construct(3);

    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(a, c);
    EXPECT_EQ(a->val, 1);
    EXPECT_EQ(b->val, 2);
    EXPECT_EQ(c->val, 3);
    EXPECT_EQ(TrackedObject::alive_count, 3);

    pool.destruct(a);
    pool.destruct(b);
    pool.destruct(c);
    EXPECT_EQ(TrackedObject::alive_count, 0);
}



// Checking errors are thrown

TEST(MempoolTest, DoubleDestructThrows) {
    MemoryPoolHeap<TrackedObject> pool(4);
    auto a = pool.construct(1);
    pool.destruct(a);
    EXPECT_THROW(pool.destruct(a), std::runtime_error); // already destroyed this object
}

TEST(MempoolTest, DestructOutOfRangeThrows) {
    MemoryPoolHeap<TrackedObject> pool(4);
    // Heap-allocate a lookalike object that does not belong to the pool's
    // buffer at all -- destruct() should reject it rather than corrupt
    // pool bookkeeping.
    TrackedObject* external = new TrackedObject(999);
    EXPECT_THROW(pool.destruct(external), std::runtime_error);
    delete external;
}

// last slot is used to mark full --> so for a pool of 3 objects, only have 2 slots
TEST(MempoolTest, ConstructCapThrows) {
    MemoryPoolHeap<TrackedObject> pool(3); // usable capacity = 2
    EXPECT_NO_THROW(pool.construct(1));
    EXPECT_NO_THROW(pool.construct(2));
    EXPECT_THROW(pool.construct(3), std::runtime_error);
}



TEST(MempoolTest, RepeatedConstructDestruct) {
    MemoryPoolHeap<TrackedObject> pool(4); // usable capacity = 3
    for (int cycle = 0; cycle < 50; ++cycle) {
        auto* a = pool.construct(cycle);
        auto* b = pool.construct(cycle + 1);
        EXPECT_EQ(TrackedObject::alive_count, 2);
        pool.destruct(a);
        pool.destruct(b);
        EXPECT_EQ(TrackedObject::alive_count, 0);
    }
}


TEST(MempoolTest, TestDestructor) {
    {
        MemoryPoolHeap<TrackedObject> pool(4);
        pool.construct(1);
        pool.construct(2);
        EXPECT_EQ(TrackedObject::alive_count, 2);
        // pool goes out of scope here without destruct() calls, but should delete still
    }
    EXPECT_EQ(TrackedObject::alive_count, 0);
}
