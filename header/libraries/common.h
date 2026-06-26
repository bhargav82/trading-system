#pragma once
#include <iostream>
#include <limits>
#include <string>


constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;
constexpr size_t ME_MAX_TICKERS = 8;
constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024;
constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;
constexpr size_t ME_MAX_NUM_CLIENTS = 256;
constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;
constexpr size_t ME_MAX_PRICE_LEVELS = 256;

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

size_t get_timestamp_us() {
    return 1;
}


template <typename T>
constexpr T INVALID = std::numeric_limits<T>::max();

template <typename T>
inline std::string toString(T& t) {
    if (t == INVALID<T>)[[unlikely]] {
        return "INVALID";
    }

    return std::to_string(t);
}

enum class Side : int8_t {
    INVALID = 0,
    BUY = 1,
    SELL = -1
};
inline std::string sideToString(Side& side) {
switch (side) {
    case Side::BUY:
    return "BUY";
    case Side::SELL:
    return "SELL";
    case Side::INVALID:
    return "INVALID";
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
};

struct ClientResponse {
    uint64_t client_order_id; // should be same as the client request
    uint64_t market_order_id; // represents the order id in the market (two clients can have same client request id)
    uint32_t client_id;       // should match the client_id of the request
    uint32_t qty_filled;      // amount of quantity filled up, should always be ≤ qty in request
    uint32_t qty_remaining;   // for partially filled orders, client needs to know if any is left, multiple messages sent for 1 order if requires multiple orders
    uint32_t price;
    Side side;
};


struct OrdersUpdate {
    uint64_t market_order_id; // id of the market order that is finished
    uint64_t qty_filled;
    uint64_t qtr_remaining;   // data needed to update clients match book
    uint32_t price;           // price order was matched at
    Side side;
    UpdateType ut;    // so client knows whether it was a new, a cancelled
};


struct SellOrder {
    uint64_t qty;
    uint64_t time_placed;
    uint64_t client_id;
    uint32_t price;

    bool operator<(const SellOrder& other) const {
        if (this->price == other.price) [[unlikey]] {
            if (this->qty == other.qty) [[unlikey]] {
                return this->
            }

            // return true -> bubble up larger qty
            return this->qty > other.qty;
        }
        
        // if false -> bubble up child (other)
        return this->price < other.price;
    }
};

struct BuyOrder {


};

#pragma pack(pop)