#pragma once
#include <iostream>
#include <limits>
#include <string>
#include "log.h"


// set global variables

struct Object {
    int val;
    std::string str;

    Object() = delete;
    Object(int v, std::string s) : val(v), str(s) {};
    void print() {
        std::cout << this->val << " " << this->str << std::endl;
    }

    bool operator==(const Object& other) const {
        return this->val == other.val && this->str == other.str;
    }
    friend std::ostream& operator<<(std::ostream& os, const Object& obj) {
        return os << obj.val << " " << obj.str << "\n";
    }
};



struct SimpleObj {
    int val;
    std::string str;

    SimpleObj() = default;
    SimpleObj(int v, std::string s) : val(v), str(s) {};
    friend std::ostream& operator<<(std::ostream& os, const SimpleObj& obj) {
        return os << obj.val << " " << obj.str << "\n";
    }

    bool operator==(const SimpleObj& other) const {
        return this->val == other.val && this->str == other.str;
    }
};

inline size_t get_timestamp_us() {
    return 1;
}



enum class Side : int8_t {
    INVALID = 0,
    BUY = 1,
    SELL = 2,
    DEFAULT = 3
};


inline std::string sideToString(Side& side) {
switch (side) {
    case Side::BUY:
        return "BUY";
    case Side::SELL:
        return "SELL";
    case Side::INVALID:
        return "INVALID";
    case Side::DEFAULT:
        return "DEFAULT";
}
    return "UNKNOWN";
}

enum class RequestType : uint8_t {
    INVALID = 0,
    NEW = 1,
    CANCEL = 2,
};
inline std::string requestTypeToString(RequestType& rt) {
    switch (rt) {
        case RequestType::NEW:
            return "NEW";
        case RequestType::CANCEL:
            return "CANCEL";
        default:
            return "INVALID";
    }
}

enum class ResponseType : uint8_t {
    INVALID = 0,
    NEW = 1,
    CANCEL = 2,
    FILLED = 3,
    CANCEL_REJECTED = 4
};

inline std::string responseTypeToString(ResponseType& rt) {
    switch (rt) {
        case ResponseType::NEW:
            return "NEW";
        case ResponseType::CANCEL:
            return "CANCEL";
        case ResponseType::FILLED:
            return "FILLED";
        case ResponseType::CANCEL_REJECTED:
            return "CANCEL_REJECTED";
        default:
            return "INVALID";
    }
}

enum class UpdateType : uint8_t {
    INVALID = 0,
    ADD = 1,
    MODIFY = 2,
    CANCEL = 3,
    TRADE = 4
};


inline std::string updateTypeToString(UpdateType& rt) {
    switch (rt) {
        case UpdateType::ADD:
            return "ADD";
        case UpdateType::MODIFY:
            return "MODIFY";
        case UpdateType::CANCEL:
            return "CANCEL";
        case UpdateType::TRADE:
            return "TRADE";
        default:
            return "INVALID";
    }
}

#pragma pack(push, 1)
struct ClientRequest {
    uint64_t order_id;
    uint32_t ticker_id;
    uint32_t client_id;
    uint32_t price;
    uint32_t qty;
    RequestType rt;
    Side side;

    ClientRequest() : order_id(0), ticker_id(0), client_id(0), price(0), qty(0), rt(RequestType::INVALID), side(Side::DEFAULT) {}
    ClientRequest(uint64_t o_id, uint32_t t_id, uint32_t c_id, uint32_t p, uint32_t q, RequestType rt_, Side s_) : order_id(o_id), ticker_id(t_id), client_id(c_id), price(p), qty(q), rt(rt_), side(s_) {}

    void print() {
        LOG("ClientRequest a " << (side == Side::BUY ? "buy" : "sell") << " of ticker_id " << ticker_id << " for client " << client_id << " at price " << price << " at qty " << qty);
    }
};

struct ClientResponse {
    uint64_t client_order_id; // should be same as the client request
    uint64_t market_order_id; // represents the order id in the market (two clients can have same client request id)
    uint32_t client_id;       // should match the client_id of the request
    uint32_t qty_filled;      // amount of quantity filled up, should always be ≤ qty in request
    uint32_t qty_remaining;   // for partially filled orders, client needs to know if any is left, multiple messages sent for 1 order if requires multiple orders
    uint32_t price;
    Side side;

    ClientResponse() : client_order_id(0), market_order_id(0), client_id(0), qty_filled(0), qty_remaining(0), price(0), side(Side::DEFAULT) {}
};


struct OrdersUpdate {
    uint64_t market_order_id; // id of the market order that is finished
    uint32_t qty_filled;
    uint32_t qty_remaining;   // data needed to update clients match book
    uint32_t price;           // price order was matched at
    Side side;
    UpdateType ut;    // so client knows whether it was a new, a cancelled

    OrdersUpdate() : market_order_id(0), qty_filled(0), qty_remaining(0), price(0), side(Side::DEFAULT), ut(UpdateType::INVALID) {}
    OrdersUpdate(uint64_t mo_id, uint32_t qty_f, uint32_t qty_r, uint32_t p, Side s, UpdateType ut_) : market_order_id(mo_id), qty_filled(qty_f), qty_remaining(qty_r), price(p), side(s), ut(ut_) {};
    
    void print() {
        LOG("OrdersUpdate for " << (side == Side::BUY ? "buy" : "sell") << " at price " << price << " at quantity " << qty_filled);
    }
};


// Comes from client request
struct Order {
    Order* next;
    Order* prev;


    uint64_t market_order_id; // need to fill this somewhere
    uint64_t client_id;
    uint32_t qty;
    uint32_t price;
    uint32_t ticker_id; // not sure if this is even needed, since we get ticker from client request to look up the book
    Side side;

    Order() = delete;
    Order(Order* n_, Order* prev_, uint64_t c_id_, uint64_t q_, uint64_t p_, Side s_) : next(n_), prev(prev_), client_id(c_id_), qty(q_), price(p_), side(s_) {}

};



#pragma pack(pop)