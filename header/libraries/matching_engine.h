
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
typedef uint64_t market_order;
std::atomic<market_order> market_order_id = 0;


class MatchingEngine {
public:

    // before calling this, make sure that the request is between 1-1024
    void add_order(ClientRequest* client_req) {
        try {
            Order* inserted_order = books[client_req->ticker_id]->insert_order(client_req);
            order_map[market_order_id++] = inserted_order;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            std::abort; // don't know if we need this abort
        }
    }

    void handle_trades(ClientRequest* client_req) {
        try {
            books[client_req->ticker_id]->handle_trades(client_req, &orders_update_queue);
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            std::abort();
        }    
    }

    // user needs to put in the market_order_id -> should probably be some form of checking, maybe handle upstream 
    void cancel_order(uint64_t market_order_id) {
        Order* order = order_map[market_order_id];
        try {
            books[order->ticker_id]->remove_order(order);
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
    std::unordered_map<market_order, Order*> order_map; // used for o(1) cancel/modification
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
// operations: insert, cancel, modify, check for crossing
class Book {
public:
    
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

    void remove_order(Order* order) {
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
    

    // need to handle that the quantities won't necessarily match --> buy order at $20 for 5 qty shoudn't match with a sell order at $10 for 3 qty
    void handle_trades(ClientRequest* client_req, SPSCQueue<OrdersUpdate>* orders_update_q) {
        if (client_req->side == Side::BUY) {
            buy_order_trade(client_req, orders_update_q);
        } else {
            sell_order_trade(client_req, orders_update_q);
        }
    }


private:

    void sell_order_trade(ClientRequest* client_req, SPSCQueue<OrdersUpdate>* orders_update_q) {
        uint64_t sell_order_price = client_req->price;
        uint32_t sell_order_qty = client_req->qty;

        // trades can be made as long as this sell price is smaller than the best buy
        while (sell_order_price <= bestBuy && sell_order_qty) {
            Order* buy_order = buy_book.top(bestBuy);
            int matched_qty = std::min(buy_order->qty, sell_order_qty);

            bool buy_order_finished = matched_qty >= buy_order->qty;
            if (buy_order_finished) {
                try {
                    orders_update_q->emplace_back(buy_order->market_order_id, matched_qty, 0, buy_order->price, Side::BUY, UpdateType::TRADE);
                    buy_book.remove_order(buy_order);
                    sell_order_qty -= matched_qty;
                } catch (const std::runtime_error& e) {
                    throw;
                }
            } else { // sell order must have matched and now we are done with this order
                try {
                    orders_update_q->emplace_back(market_order_id++, matched_qty, 0, sell_order_price, Side::SELL, UpdateType::TRADE);
                    sell_order_qty -= matched_qty;
                } catch (const std::runtime_error& e) {
                    throw;
                }
            }

            if (sell_order_qty) {
                client_req->qty = sell_order_qty;
                sell_book.insert_order(client_req);
            }
        }

        
    }


    void buy_order_trade(ClientRequest* client_req, SPSCQueue<OrdersUpdate>* orders_update_q) {
        uint64_t buy_order_price = client_req->price;
        uint32_t buy_order_quantity = client_req->qty;
        while (buy_order_price >= bestSell && buy_order_quantity) {
            Order* sell_order = sell_book.top(bestSell);
            int matched_qty = std::min(buy_order_quantity, sell_order->qty);
            bool sell_order_finished = matched_qty >= sell_order->qty; 

            if (sell_order_finished) {
                try {   
                    orders_update_q->emplace_back(sell_order->market_order_id, sell_order->qty, 0, sell_order->price, Side::SELL, UpdateType::TRADE);
                    sell_book.remove_order(sell_order); // updates best Sell automatically
                    buy_order_quantity -= matched_qty;
                } catch (const std::runtime_error& e) {
                    throw;
                }
            } else { // buy order qty must be strictly smaller -> but able to finish, just insert into queue, don't add back in
                try {
                    // never got placed into the book so it never got a market id
                    orders_update_q->emplace_back(market_order_id++, matched_qty, 0, buy_order_price, Side::BUY, UpdateType::TRADE);
                    buy_order_quantity -= matched_qty;
                } catch (const std::runtime_error& e) {
                    throw;
                }
            }
        }

        if (buy_order_quantity) {
            // create an order and insert it into the book
            client_req->qty = buy_order_quantity;
            buy_book.insert_order(client_req);
        }
    }



    HalfBook sell_book;
    HalfBook buy_book;
    size_t bestBuy = 0;
    size_t bestSell = std::numeric_limits<size_t>::max();
};