#include "../libraries/matching_engine.h"
#include <gtest/gtest.h>
#include <iostream>
#include <string>

// TICKER IS ALWAYS 0
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
ClientRequest* default_request(bool buy) {
    ClientRequest* cr = new ClientRequest(0, 0, 0, 0, 0, RequestType::NEW, buy ? Side::BUY : Side::SELL);
    
    return cr;
}



TEST(PriceLevelTopBuy, MatchingEngine) {

   
    MatchingEngine me;  
    // Insert the order in -> price level 40, and best Buy == 40
    ClientRequest* first_40 = default_request(true);
    first_40->price = 40;
    first_40->client_id = 1;
    me.add_order(first_40);

    Order* first_order_top = me.books[0]->buy_book.top(first_40->price);
    EXPECT_EQ(first_order_top->price, first_40->price);
    EXPECT_EQ(me.books[0]->bestBuy, 40);

    // Insert another order in -> price level 50, id 1 makes best Buy == 50
    ClientRequest* first_50 = default_request(true);
    first_50->price = 50;
    first_50->client_id = 1;
    me.add_order(first_50);

    EXPECT_EQ(me.books[0]->bestBuy, 50);

    // Insert another order in -> price level 50. id 2 -> best Buy should not change from 50, and the client must still be = 1
    ClientRequest* second_50 = default_request(true);
    second_50->price = 50;
    second_50->client_id = 2;
    me.add_order(second_50);

    int bestBuy = me.books[0]->bestBuy;
    EXPECT_EQ(bestBuy, 50);
    EXPECT_EQ(me.books[0]->buy_book.top(bestBuy)->client_id, 1); 

    // current state:
    // [40: first_40]
    // [50: first_50, second_50]

     std::deque<ClientRequest*> requests;
    // create 50 newr requests, best price should keep changing
    for (size_t order = 51; order < 100; ++order) {
        ClientRequest* curr_order = default_request(true);
        curr_order->price = order;
        curr_order->client_id = order;
        me.add_order(curr_order);
        requests.push_back(curr_order);

        bestBuy = me.books[0]->bestBuy; 

        // this new order is the new best buy
        EXPECT_EQ(order, bestBuy);
        EXPECT_EQ(me.books[0]->buy_book.top(bestBuy)->client_id, curr_order->client_id);
        EXPECT_EQ(me.books[0]->buy_book.top(bestBuy)->price, curr_order->price);
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



// Test top and best sell, ticker is always 0
TEST(PriceLevelTopSell, MatchingEngine) {
    MatchingEngine me;

    // add a new sell request, price 20 id 1, bestSell should = 20
    ClientRequest* first_20 = default_request(false);
    first_20->price = 20;
    first_20->client_id = 1;
    me.add_order(first_20);
    int bestSell = first_20->price;
    EXPECT_EQ(me.books[0]->bestSell, bestSell);

    // the top order should be with id = 1
    Order* first_order_top = me.books[0]->sell_book.top(20);
    EXPECT_EQ(first_order_top->client_id, 1);
    
    // add a new request at the same price, but old order should stil be the top
    ClientRequest* second_20 = default_request(false);
    second_20->price = 20;
    second_20->client_id = 2;
    me.add_order(second_20);

    // best sell doesn't change = 20, and id doesn't change = 1
    Order* second_order_top = me.books[0]->sell_book.top(bestSell);
    EXPECT_EQ(me.books[0]->bestSell, bestSell); 
    EXPECT_EQ(second_order_top->client_id, 1); 

    // add a new order at 10 -> best sell should change to 10, and id = 3
    ClientRequest* third_19 = default_request(false);
    third_19->price = 19;
    third_19->client_id = 3;
    me.add_order(third_19);
    bestSell = me.books[0]->bestSell;
    EXPECT_EQ(bestSell, 19);

    // the top should now have id = 3
    Order* new_top = me.books[0]->sell_book.top(bestSell);
    EXPECT_EQ(new_top->client_id, 3);

    std::deque<ClientRequest*> requests;
    // add a new price level from 18-1. can't insert 0
    for (size_t i = 19; i-- > 1; ) {
        ClientRequest* new_order = default_request(false);
        new_order->price = i;
        new_order->client_id = i;
        me.add_order(new_order);
        requests.push_back(new_order);
        bestSell = me.books[0]->bestSell;
        EXPECT_EQ(bestSell, new_order->price);
        EXPECT_EQ(me.books[0]->sell_book.top(bestSell)->client_id, new_order->client_id);
        EXPECT_EQ(me.books[0]->sell_book.top(bestSell)->price, new_order->price);

    }

    // go through the ones we just added 1-18 and remove them and the best sell should increase by 1
    while (!requests.empty()) {
        // the best sell should be whataever is at the end of requests (whatever we added last)
        Order* old_best_sell = me.books[0]->sell_book.top(bestSell);
        EXPECT_EQ(old_best_sell->client_id, requests.back()->client_id);
        EXPECT_EQ(old_best_sell->price, requests.back()->price);

        // remove this request from both deque and me
        requests.pop_back();
        me.books[0]->remove_order(old_best_sell);
        bestSell = me.books[0]->bestSell;

    }


    // state is backed to
    // [19: third_19]
    // [20: first_20, second_20]
    
    // check that the top is = {19, 3}, then remove it
    Order* test_third_19 = me.books[0]->sell_book.top(bestSell);
    EXPECT_EQ(test_third_19->price, 19);
    EXPECT_EQ(test_third_19->client_id, 3);
    me.books[0]->remove_order(test_third_19);

    // after removing the best sell should be 20
    bestSell = me.books[0]->bestSell;
    EXPECT_EQ(bestSell, 20);
    Order* test_first_20 = me.books[0]->sell_book.top(bestSell);
    EXPECT_EQ(test_first_20->client_id, 1);
    EXPECT_EQ(test_first_20->price, 20);
    me.books[0]->remove_order(test_first_20);

    // after removing the first 20, only second 20 should be there with id = 2
    bestSell = me.books[0]->bestSell;
    EXPECT_EQ(bestSell, 20);
    Order* test_second_20 = me.books[0]->sell_book.top(bestSell);
    EXPECT_EQ(test_second_20->price, 20);
    EXPECT_EQ(test_second_20->client_id, 2);
    me.books[0]->remove_order(test_second_20);

    // after removing the last order, the best sell shoud be 129
    bestSell = me.books[0]->bestSell;
    EXPECT_EQ(bestSell, 129);

}


TEST(MakeTradesSellSide, MatchingEngine) {
    MatchingEngine me;
    
    // Test 1 bid/sell 
    ClientRequest* buy_40 = default_request(true);
    buy_40->price = 40;
    buy_40->qty = 5;
    buy_40->client_id = 1;
    me.add_order(buy_40);
    ASSERT_EQ(me.books[0]->bestBuy, 40);
    ASSERT_EQ(me.books[0]->bestSell, 129);

    ClientRequest* sell_35 = default_request(false);
    sell_35->qty = 3;
    sell_35->price = 35;
    sell_35->client_id = 1;
    // should print executing sell order trade
    me.handle_trades(sell_35);

    // should have the buy remaining with price 40 qty 2 from client 1
    ASSERT_EQ(me.books[0]->bestBuy, 40);
    ASSERT_EQ(me.books[0]->buy_book.top(40)->qty, 2);
    ASSERT_EQ(me.books[0]->bestSell, 129);

    // me.orders_update_queue.print();

    // Test 2:
    // insert 5 buy requests, and then insert 1 sell to fill them all
    for (size_t i = 0; i < 5; ++i) {
        ClientRequest* buy_i = default_request(true);
        buy_i->qty = 5;
        buy_i->price = i + 40; // makes buy orders at price, 40, 41, 42, 43, 44
        me.add_order(buy_i);
        ASSERT_EQ(me.books[0]->bestBuy, i + 40);
    }
    
    // insert a sell to fill all 
    ClientRequest* sell_i = default_request(false);
    sell_i->qty = 25;
    sell_i->price = 30;
    me.handle_trades(sell_i); // should print executing sell order 5 times


  
}

TEST(MakeTradesBuySide, MatchingEngine) {
      // test matches for buy side
    MatchingEngine me2;

    // make a sell order at 45
    ClientRequest* sell_45 = default_request(false);
    sell_45->price = 45;
    sell_45->qty = 5;
    me2.add_order(sell_45);
    // make a buy request that fills this request
    ClientRequest* buy_50 = default_request(true);
    buy_50->price = 50;
    buy_50->qty = 5;
    me2.handle_trades(buy_50);
    // should print execute buy order -> does not insert the buy order since it was ful
    ASSERT_EQ(me2.books[0]->bestBuy, 0);
    ASSERT_EQ(me2.books[0]->bestSell, 129);

    // now there are no orders
    // insert 2 sells and have them all fill up 1 buy
    ClientRequest* sell_45_2 = default_request(false);
    sell_45_2->price = 45;
    sell_45_2->qty = 5;
    me2.add_order(sell_45_2);

    ClientRequest* sell_45_3 = default_request(false);
    sell_45_3->price = 45;
    sell_45_3->qty = 6;
    me2.add_order(sell_45_3);

    // there should be 2 sell orders at price 45, with order [qty5, qty6]
    ASSERT_EQ(me2.books[0]->bestSell, 45);
    ASSERT_EQ(me2.books[0]->sell_book.top(45)->qty, 5);

    // add a new buy order that can fill both
    ClientRequest* buy_50_2 = default_request(true);
    buy_50_2->price = 50;
    buy_50_2->qty = 15;
    me2.handle_trades(buy_50_2); 
    // should print execute buy order -> inserts buy order that has 4 qty remaining
    ASSERT_EQ(me2.books[0]->bestBuy, 50);
    ASSERT_EQ(me2.books[0]->buy_book.top(50)->qty, 4);

    // add a sell order that can fill the remaining 4 qty of the prev buy
    ClientRequest* sell_order_filler = default_request(false);
    sell_order_filler->price = 30;
    sell_order_filler->qty = 4;
    me2.handle_trades(sell_order_filler);
    // should print executing sell order
    
    // should have no remaining sell orders and buy orders remaining
    ASSERT_EQ(me2.books[0]->bestBuy, 0);
    ASSERT_EQ(me2.books[0]->bestSell, 129);
}

TEST(MakeTradesBothSides, MatchingEngine) {
    // test both sides trading at scale

    
}