
#include "spsc_queue.h"
#include "gtest/gtest_prod.h"
#include "common.h"
#include "mempool.h"
#include <atomic>
#include <array>
#include <unordered_map>
#include <limits>

constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;
constexpr size_t ME_MAX_TICKERS = 8;
constexpr size_t ME_MAX_CLIENT_UPDATES = 1024;
constexpr size_t ME_MAX_MARKET_UPDATES = 1024;
constexpr size_t ME_MAX_NUM_CLIENTS = 256;
constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;
constexpr size_t ME_MAX_PRICE_LEVELS = 128;


typedef SPSCQueue<ClientRequest> ClientRequestQueue; // receive orders from order server
typedef SPSCQueue<ClientResponse> ClientResponseQueue; // send confirmation messages (or failure) to order server
typedef SPSCQueue<OrdersUpdate> OrdersUpdateQueue; // send updates on matches 

typedef uint32_t ticker_id;
typedef uint64_t market_order;
std::atomic<market_order> market_order_id = 0;



template <typename T>
class PriceLevel {
public:
    explicit PriceLevel(size_t level_size) : orders(level_size) {
        head = new Order{nullptr, nullptr, 0, 0, 0, Side::DEFAULT};
        tail = new Order{nullptr, nullptr, 0, 0, 0, Side::DEFAULT};

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

    void print() {
        T* curr = head;
        while (curr) {
            if (curr == head) {
                std::cout << "head ";
            } else if (curr == tail) {
                std::cout << "tail ";
            } else {
                std::cout << curr->market_order_id << " " << curr->price;
            }
            curr = curr->next;
        }
    }


private:
    MemoryPoolHeap<T> orders;
    // dummy pointers, head and tail will never be actual order objects just pointers to them
    T* head;
    T* tail;
};


class HalfBook {
public:
    HalfBook() = delete;
    explicit HalfBook(size_t level_size) {
        for (size_t i = 0; i < level_size; ++i) {
            levels.emplace_back(level_size);
        }
    }

    Order* insert_order(ClientRequest* order) {
        // add the order to the level it belongs to
        assert(order->price > 0 && order->price < 129);
        return levels[order->price - 1].add_order(order);
    }

    void remove_order(Order* order) {
        assert(order->price > 0 && order->price < 129);
        levels[order->price - 1].remove_order(order);
    }

    Order* top(uint32_t price) {
        assert(price > 0 && price < 129);
        return levels[price - 1].top();
    }

    size_t size() {
        return levels.size();
    }

private:
    // Keep an deque of memory pool heap of T objects
    // array doesn't work since Pricelevel isn't default constructible, vector doesn't work since PriceLevel isn't copyable
    std::deque<PriceLevel<Order>> levels;
};



// Consider using a dispatch table to avoid branches
// operations: insert, cancel, modify, check for crossing
class Book {
    FRIEND_TEST(PriceLevelTopBuy, MatchingEngine);
    FRIEND_TEST(PriceLevelTopSell, MatchingEngine);
    FRIEND_TEST(MakeTradesBuySide, MatchingEngine);
    FRIEND_TEST(MakeTradesSellSide, MatchingEngine);
    FRIEND_TEST(MakeTradesBothSides, MatchingEngine);
public:
    
    Book() = delete;
    explicit Book (size_t level_size) : sell_book(level_size), buy_book(level_size) {} // initializer list skips default construction

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
            // remove the order
            buy_book.remove_order(order);

            // if the best order. top now points at tail (within this index (index is a doubly linked list)) which has price == 0
            ssize_t start = bestBuy;
            for ( ; start > 0 && buy_book.top(start)->price == 0; --start) {};
            bestBuy = start; // = 0 when no orders remaining
            
        } else {
            sell_book.remove_order(order);

            // same as above but go down in prices for sells, the best sell is someone selling for 1
            ssize_t start = bestSell;
            // can do all the way til price = 128, because we - 1 in top
            for ( ; start <= sell_book.size() && sell_book.top(start)->price == 0; ++start) {};
            bestSell = start; // = 129 when no order remaining
            
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
        uint64_t og_sell_qty = client_req->qty;
        uint64_t sell_order_price = client_req->price;
        uint32_t sell_order_qty = client_req->qty;

        // trades can be made as long as this sell price is smaller than the best buy
        while (sell_order_price <= bestBuy && sell_order_qty) {
            Order* buy_order = buy_book.top(bestBuy);
            int matched_qty = std::min(buy_order->qty, sell_order_qty);
            LOG("Executing sell order trade at " << sell_order_price << " for " << matched_qty);

            bool buy_order_finished = matched_qty >= buy_order->qty;
            if (buy_order_finished) {
                try {
                    orders_update_q->emplace_back(buy_order->market_order_id, buy_order->qty, 0, sell_order_price, Side::BUY, UpdateType::TRADE); // this buy trade was filled by the sell, but could be a diff price then it originally had
                    orders_update_q->emplace_back(client_req->order_id, matched_qty, sell_order_qty - matched_qty, sell_order_price, Side::SELL, UpdateType::TRADE); // this sell that filled also needs to be added to updates even if it isn't completely done
                    remove_order(buy_order); // updates best buy
                } catch (const std::runtime_error& e) {
                    throw;
                }
            } else { // sell order must have matched and now we are done with this order
                buy_order->qty -= matched_qty;
            }
            sell_order_qty -= matched_qty;

        }

        if (sell_order_qty) {
            LOG("Inserting sell order at price " << sell_order_price << " for " << sell_order_qty);
            client_req->qty = sell_order_qty;
            Order* new_sell_order = insert_order(client_req);
            orders_update_q->emplace_back(new_sell_order->market_order_id, new_sell_order->qty, og_sell_qty - client_req->qty, new_sell_order->price, Side::SELL, UpdateType::ADD);
        }
        
    }


    void buy_order_trade(ClientRequest* client_req, SPSCQueue<OrdersUpdate>* orders_update_q) {
        uint64_t og_buy_qty = client_req->qty;
        uint64_t buy_order_price = client_req->price;
        uint32_t buy_order_quantity = client_req->qty;
        while (buy_order_price >= bestSell && buy_order_quantity) {
            Order* sell_order = sell_book.top(bestSell);
            int matched_qty = std::min(buy_order_quantity, sell_order->qty);
            LOG("Executing buy order trade at " << buy_order_price << " for " << matched_qty);

            bool sell_order_finished = matched_qty >= sell_order->qty; 

            if (sell_order_finished) {
                try {   
                    orders_update_q->emplace_back(sell_order->market_order_id, sell_order->qty, 0, sell_order->price, Side::SELL, UpdateType::TRADE); // add an update for the filled sell
                    orders_update_q->emplace_back(client_req->order_id, sell_order->qty, buy_order_quantity - matched_qty, sell_order->price, Side::BUY, UpdateType::TRADE); // add an update for the part of the buy that filled 
                    remove_order(sell_order); // updates best Sell automatically
                } catch (const std::runtime_error& e) {
                    throw;
                }
            } else { // buy order qty must be strictly smaller -> but able to finish, just insert into queue, don't add back in
                    sell_order->qty -= matched_qty;
            }
            
            buy_order_quantity -= matched_qty;
           
        }

        if (buy_order_quantity) {
            LOG("Inserting buy order at " << buy_order_price << " for " << buy_order_quantity);
            // create an order and insert it into the book
            client_req->qty = buy_order_quantity;
            Order* new_buy_order = insert_order(client_req);
            orders_update_q->emplace_back(new_buy_order->market_order_id, new_buy_order->qty, og_buy_qty - new_buy_order->qty, new_buy_order->price, Side::BUY, UpdateType::ADD);
        }
    }


    HalfBook sell_book;
    HalfBook buy_book;

    size_t bestBuy = 0; // ranges from [0, 128]
    size_t bestSell = 129; // ranges from [0-128]
};



class MatchingEngine {
    FRIEND_TEST(PriceLevelTopBuy, MatchingEngine);
    FRIEND_TEST(PriceLevelTopSell, MatchingEngine);
    FRIEND_TEST(MakeTradesBuySide, MatchingEngine);
    FRIEND_TEST(MakeTradesSellSide, MatchingEngine);
    FRIEND_TEST(MakeTradesBothSides, MatchingEngine);
public:

    MatchingEngine() : client_request_queue(ME_MAX_CLIENT_UPDATES), client_response_queue(ME_MAX_CLIENT_UPDATES), orders_update_queue(ME_MAX_MARKET_UPDATES) {}

    // before calling this, make sure that the price is between 1-128
    void add_order(ClientRequest* client_req) {
        ticker_id t_id = client_req->ticker_id;
        auto it = books.find(t_id);

        if (it == books.end()) {
            books[t_id] = new Book(ME_MAX_PRICE_LEVELS); 
        }
        try {
            Order* inserted_order = books[client_req->ticker_id]->insert_order(client_req);
            order_map[market_order_id++] = inserted_order;
        } catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
            std::abort(); // don't know if we need this abort
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

    // std::array<Book, 1024> books; // use tickers to index into this 
    std::unordered_map<market_order, Order*> order_map; // used for o(1) cancel/modification
};

