
#include "spsc_queue.h"
#include "common.h"
#include "mempool.h"
#include <atomic>
#include <array>
#include <limits>

typedef SPSCQueue<ClientRequest> ClientRequestQueue; // receive orders from order server
typedef SPSCQueue<ClientResponse> ClientResponseQueue; // send confirmation messages (or failure) to order server
typedef SPSCQueue<OrdersUpdate> OrdersUpdateQueue; // send updates on matches 

typedef uint32_t ticker_id;
typedef uint64_t market_order_id;
std::atomic<market_order_id> order_id = 0;


class MatchingEngine {
public:

    // before calling this, make sure that the request is between 1-1024
    void add_order(ClientRequest* client_req) {
        try {
            Order* inserted_order = books[client_req->ticker_id]->insert_order(client_req);
            order_map[order_id++] = inserted_order;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            std::abort; // don't know if we need this abort
        }
    }

    void make_trades(ClientRequest& client_req) {

    }

    // user needs to put in the market_order_id -> should probably be some form of checking, maybe handle upstream 
    void cancel_order(uint64_t market_order_id) {
        Order* order = order_map[market_order_id];
        try {
            books[order->ticker_id]->cancel_order(order);
            order_map.erase(market_order_id);
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            std::abort(); // probably don't need to abort since we are just throwing error
        }
    }

private:
    OrdersUpdateQueue orders_update_queue; // producer of successful orders (or any updates)
    ClientRequestQueue client_request_queue; // consumer of order messages
    ClientResponseQueue client_response_queue; // producer of response messages

    // make own lock free hash tables
    std::unordered_map<ticker_id, Book*> books;
    std::unordered_map<market_order_id, Order*> order_map; // used for o(1) cancel/modification
};


template <typename T>
class PriceLevel {
public:
    explicit PriceLevel(size_t level_size) : orders(level_size) {
        head = new Order{nullptr, nullptr, 0, 0, 0, 0};
        tail = new Order{nullptr, nullptr, 0, 0, 0, 0};

        head->next = tail;
        tail->prev = head;
        
    };
    
    T* add_order(ClientRequest* new_order) {
        // just add to the end if possible, construct this object with its values before hand
        try {
            // constructs an order since memory pool only holds Order objects
            tail->prev->next = orders.construct(tail, tail->prev, new_order->client_id, new_order->qty, new_order->price, new_order->side);
            tail->prev = tail->prev->next;
            return tail->prev;
        } catch (const std::runtime_error& e) {
            throw; // propagate error up
        }
    }

    void remove_order(T* remove_order) {
        if (!remove_order) {
            throw std::runtime_error("matching_engine: Order is null");
        }

        // swap pointers, call destructor
        remove_order->prev->next = remove_order->next;
        remove_order->next->prev = remove_order->prev;

        orders.destruct(remove_order);
        // make sure to delete key from global map
    }

    T* top() {
        // Make sure to check that this is not nullptr when calling top
        return head->next;
    }


private:
    MemoryPoolHeap<T> orders;
    // dummy pointers, head and tail will never be actual order objects just pointers to them
    T* head;
    T* tail;
};


class HalfBook {
public:

    Order* insert_order(ClientRequest* order) {
        // add the order to the level it belongs to
        return limits[order->price - 1].add_order(order);
    }

    void remove_order(Order* order) {
        limits[order->price - 1].remove_order(order);
    }

    Order* top(uint32_t price) {
        return limits[price - 1].top();
    }

    size_t size() {
        return limits.size();
    }

private:
    // Keep an array of memory pool heap of T objects
    std::array<PriceLevel<Order>, 1024> limits;
};



// Consider using a dispatch table to avoid branches
class Book {
public:
    
    // operations: insert, cancel, modify, check for crossing
    [[nodiscard]] Order* insert_order(ClientRequest* order) {
        Order* inserted_order = nullptr;
        if (order->side == Side::BUY) {
            inserted_order = buy_book.insert_order(order);
            // higher prices are better -> can make more matches
            if (order->price > bestBuy) {
                bestBuy = order->price;
            }
        } else {
            inserted_order = sell_book.insert_order(order);
            if (order->price < bestSell) {
                bestSell = order->price;
            }
        }

        return inserted_order;
    }

    void cancel_order(Order* order) {
        if (order->side == Side::BUY) {
            buy_book.remove_order(order);
            // if the current order is the best price, and after we remove it head is the only order in this queue -> must move up until thats not true
            if (bestBuy == order->price && buy_book.top(order->price)->price == 0) {
                size_t start = bestBuy;
                for ( ; start < buy_book.size() && buy_book.top(start)->price == 0; ++start) {};
                bestBuy = start == 1024 ? 0 : start;
            }
        } else {
            sell_book.remove_order(order);
            // same as above but go down in prices for sells, the best sell is someone selling for 1
            if (bestSell == order->price && sell_book.top(order->price)->price == 0) {
                ssize_t start = bestSell;
                for ( ; start >= 0 && sell_book.top(start)->price == 0; --start) {};
                bestSell = start == -1 ? std::numeric_limits<size_t>::max() : start;
            }
        }
    }
    

    // do handle trades
    void make_trade(Order& buy_order, Order& sell_order) {

    }


private:
    HalfBook sell_book;
    HalfBook buy_book;

    size_t bestBuy = 0;
    size_t bestSell = std::numeric_limits<size_t>::max();
};