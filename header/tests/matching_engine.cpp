#include "../libraries/matching_engine.h"
#include <gtest/gtest.h>
#include <iostream>
#include <string>


/*
ClientRequest is:
    uint64_t order_id;
    uint32_t ticker_id;
    uint32_t client_id;
    uint32_t price;
    uint32_t qty;
    RequestType rt;
    Side side;
*/
ClientRequest* default_buy() {
    ClientRequest* cr = new ClientRequest(0, 0, 0, 0, 0, RequestType::NEW, Side::BUY);
    
    return cr;
}

TEST(PriceLevelTopBuy, PriceLevel) {

   
    MatchingEngine me;  
    // Insert the order in -> price level 40, and best Buy == 40
    ClientRequest* first_40 = default_buy();
    first_40->price = 40;
    first_40->client_id = 1;
    me.add_order(first_40);



    Order* first_order_top = me.books[first_40->ticker_id]->buy_book.top(first_40->price);
    EXPECT_EQ(first_order_top->price, first_40->price);
    EXPECT_EQ(me.books[first_40->ticker_id]->bestBuy, 40);

    // Insert another order in -> price level 50, id 1 makes best Buy == 50
    ClientRequest* first_50 = default_buy();
    first_50->price = 50;
    first_50->client_id = 1;
    me.add_order(first_50);

    EXPECT_EQ(me.books[first_50->ticker_id]->bestBuy, 50);

    // Insert another order in -> price level 50. id 2 -> best Buy should not change from 50, and the client must still be = 1
    ClientRequest* second_50 = default_buy();
    second_50->price = 50;
    second_50->client_id = 2;
    me.add_order(second_50);

    int bestBuy = me.books[second_50->ticker_id]->bestBuy;
    EXPECT_EQ(bestBuy, 50);
    EXPECT_EQ(me.books[second_50->ticker_id]->buy_book.top(bestBuy)->client_id, 1); 

    // current state:
    // [40: first_40]
    // [50: first_50, second_50]

     std::deque<ClientRequest*> requests;
    // create 50 newr requests, best price should keep changing
    for (size_t order = 51; order < 100; ++order) {
        ClientRequest* curr_order = default_buy();
        curr_order->price = order;
        curr_order->client_id = order;
        me.add_order(curr_order);
        requests.push_back(curr_order);

        bestBuy = me.books[curr_order->ticker_id]->bestBuy; 

        // this new order is the new best buy
        EXPECT_EQ(order, bestBuy);
        EXPECT_EQ(me.books[curr_order->ticker_id]->buy_book.top(bestBuy)->client_id, curr_order->client_id);
        EXPECT_EQ(me.books[curr_order->ticker_id]->buy_book.top(bestBuy)->price, curr_order->price);
    }

   

    // get rid of the 51-100 we just added, start removing orders and make sure that there is a new best buy
    while (!requests.empty()) {
        // find what the curret best buy is, it should be at the end of requests
        Order* old_best_buy = me.books[0]->buy_book.top(bestBuy);
        EXPECT_EQ(old_best_buy->client_id, requests.back()->client_id);

        // remove the order from both requests ad the book
        requests.pop_back();
        me.books[0]->remove_order(old_best_buy);

        bestBuy = me.books[0]->bestBuy; 
    }

    // state is back to:
    // [40: first_40]
    // [50: first_50, second_50]

    Order* test_first_50 = me.books[0]->buy_book.top(bestBuy);
    EXPECT_EQ(50, test_first_50->price);
    EXPECT_EQ(1, test_first_50->client_id);
    me.books[0]->remove_order(test_first_50);


    // after removing the best buy should still be 50, but be the second one
    bestBuy = me.books[0]->bestBuy;
    EXPECT_EQ(bestBuy, 50);
    Order* test_second_50 = me.books[0]->buy_book.top(bestBuy);
    EXPECT_EQ(2, test_second_50->client_id);
    EXPECT_EQ(50, test_second_50->price);
    me.books[0]->remove_order(test_second_50);

    // // remove the second 50, best buy shuld be 40, for client 1
    bestBuy = me.books[0]->bestBuy;
    EXPECT_EQ(bestBuy, 40);
    Order* test_first_40 = me.books[0]->buy_book.top(bestBuy);
    EXPECT_EQ(1, test_first_40->client_id);
    EXPECT_EQ(40, test_first_40->price);
    me.books[0]->remove_order(test_first_40);

    // after removing the very first order, the best buy should be 0
    bestBuy = me.books[0]->bestBuy;
    EXPECT_EQ(bestBuy, 0);
    


}
