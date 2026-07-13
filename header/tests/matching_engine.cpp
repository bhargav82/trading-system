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

TEST(PriceLevelTop, PriceLevel) {

    MatchingEngine me;

    ClientRequest* first = default_buy();
    first->price = 40;
    me.add_order(first);

    Order* top = me.books[first->ticker_id]->buy_book.top(first->price);
    EXPECT_EQ(top->price, first->price);

    me.books[first->ticker_id]->buy_book.remove_order(top);
    Order* new_top = me.books[first->ticker_id]->buy_book.top(127);
    EXPECT_EQ(new_top->price, -1);

    // insert 1 order -> top
    
    // insert n orders -> top

    // remove only order -> check top

    // remove head/middle/tail -> verify linked list via print


}
