#include <gtest/gtest.h>
#include "../libraries/common.h"
#include "../libraries/spsc_queue.h"




TEST(SPSCQueue, BasicTest) {
    SPSCQueue<SimpleObj> obj_queue(4);    
    obj_queue.emplace_back(1, "first");
    obj_queue.emplace_back(2, "second");
    obj_queue.emplace_back(3, "third");
    obj_queue.emplace_back(4, "fourth");

    obj_queue.pop();
    obj_queue.pop();
    obj_queue.emplace_back(5, "fifth");

    std::vector<SimpleObj> exp_result { {3, "third"}, {4, "fourth"}, {5, "fifth"} };
    std::vector<SimpleObj> ret_result = obj_queue.drain();

    EXPECT_EQ(exp_result, ret_result);
}


TEST(SPSCQueue, PeekOnEmptyReturnsFalse) {
    SPSCQueue<SimpleObj> q(4);
    // top() calls peek() internally; nullptr means empty
    EXPECT_EQ(q.top(), nullptr);
}


TEST(SPSCQueue, InsertBeyondCapacityIsDropped) {
    SPSCQueue<SimpleObj> q(3);           // internal capacity = 4 slots, usable = 3
    q.emplace_back(1, "a");
    q.emplace_back(2, "b");
    q.emplace_back(3, "c");
    q.emplace_back(4, "overflow");       // must be dropped
 
    auto got = q.drain();
    ASSERT_EQ(got.size(), 3u);
    EXPECT_EQ(got[0], (SimpleObj{1, "a"}));
    EXPECT_EQ(got[1], (SimpleObj{2, "b"}));
    EXPECT_EQ(got[2], (SimpleObj{3, "c"}));
}


TEST(SPSCQueue, EmplaceAndPopSingleElement) {
    SPSCQueue<SimpleObj> q(4);
    q.emplace_back(1, "first");
 
    SimpleObj* front = q.top();
    ASSERT_NE(front, nullptr);
    EXPECT_EQ(*front, (SimpleObj{1, "first"}));
 
    q.pop();
    EXPECT_EQ(q.top(), nullptr);
}

TEST(SPSCQueue, WrapAroundAfterPartialDrain) {
    SPSCQueue<SimpleObj> q(4);
    q.emplace_back(1, "first");
    q.emplace_back(2, "second");
    q.emplace_back(3, "third");
    q.emplace_back(4, "fourth");
 
    q.pop();
    q.pop();
 
    q.emplace_back(5, "fifth");
    q.emplace_back(6, "sixth");
 
    auto got = q.drain();
    ASSERT_EQ(got.size(), 4u);
    EXPECT_EQ(got[0], (SimpleObj{3, "third"}));
    EXPECT_EQ(got[1], (SimpleObj{4, "fourth"}));
    EXPECT_EQ(got[2], (SimpleObj{5, "fifth"}));
    EXPECT_EQ(got[3], (SimpleObj{6, "sixth"}));
}
 


 
// Fill → drain completely → refill → verify no stale data
TEST(SPSCQueue, FullDrainThenRefill) {
    SPSCQueue<SimpleObj> q(3);
    q.emplace_back(1, "a");
    q.emplace_back(2, "b");
    q.emplace_back(3, "c");
    q.drain();
 
    q.emplace_back(4, "d");
    q.emplace_back(5, "e");
 
    auto got = q.drain();
    ASSERT_EQ(got.size(), 2u);
    EXPECT_EQ(got[0], (SimpleObj{4, "d"}));
    EXPECT_EQ(got[1], (SimpleObj{5, "e"}));
}
 
// Repeatedly fill and drain to exercise multiple wrap-arounds
TEST(SPSCQueue, MultipleWrapArounds) {
    SPSCQueue<SimpleObj> q(2);
 
    for (int round = 0; round < 5; ++round) {
        q.emplace_back(round * 2,     "even");
        q.emplace_back(round * 2 + 1, "odd");
 
        auto got = q.drain();
        ASSERT_EQ(got.size(), 2u) << "round " << round;
        EXPECT_EQ(got[0].val, round * 2);
        EXPECT_EQ(got[1].val, round * 2 + 1);
    }
}
 
// ─── push_back (move) ────────────────────────────────────────────────────────
 
TEST(SPSCQueue, PushBackMove) {
    SPSCQueue<SimpleObj> q(4);
    SimpleObj obj{42, "moved"};
    q.push_back(std::move(obj));
 
    SimpleObj* front = q.top();
    ASSERT_NE(front, nullptr);
    EXPECT_EQ(front->val,   42);
    EXPECT_EQ(front->str, "moved");
}
 
// ─── Capacity-1 edge case ────────────────────────────────────────────────────
 
TEST(SPSCQueue, CapacityOne) {
    SPSCQueue<SimpleObj> q(1);           // internal = 2 slots, usable = 1
    q.emplace_back(1, "only");
 
    ASSERT_NE(q.top(), nullptr);
    EXPECT_EQ(*q.top(), (SimpleObj{1, "only"}));
 
    // Second insert must be dropped (queue full)
    q.emplace_back(2, "dropped");
    EXPECT_EQ(q.top()->val, 1);
 
    q.pop();
    EXPECT_EQ(q.top(), nullptr);
}