
#include "spsc_queue.h"
#include "common.h"
#include "mempool.h"
#include <atomic>
#include <array>

typedef SPSCQueue<ClientRequest> ClientRequestQueue; // receive orders from order server
typedef SPSCQueue<ClientResponse> ClientResponseQueue; // send confirmation messages (or failure) to order server
typedef SPSCQueue<OrdersUpdate> OrdersUpdateQueue; // send updates on matches 

typedef uint32_t ticker_id;
typedef uint64_t market_order_id;
std::atomic<market_order_id> order_id = 0;



// ENSURE THAT THE MEMORY HEAP IS CORRECT
class MatchingEngine {
public:

    // requires a collection of sell and buy orders -> should it be a hashmap of ticker_ids, and then a linked list of diff tickers and inside each is a heap of orders, ordered by best price, then 
    // use a hashmap where each index is a ticker_id : heap of orders
    // for each ticker -> maintain an order boook (limit prices from $1-$1025)

    void handle_order(ClientRequest& incoming_order) {
        // Figure out what side it is
        // Check if there is any crossing
        //      if there a perfect match -> don't insert this in remove the best other, and send a message
        //      if there is a partial match -> insert the partial in, remove the other and send a message
        //      if there are multiple matches -> dont insert this in, remove all others, create messages for all
        
        
    }


private:

    OrdersUpdateQueue orders_update_queue; // producer of successful orders (or any updates)
    ClientRequestQueue client_request_queue; // consumer of order messages
    ClientResponseQueue client_response_queue; // producer of response messages

    // make own lock free hash tables
    std::unordered_map<ticker_id, Book> 
};

/*
per order:
    order_id: identify orders
    ticket_id: identify trading instrument
    client_id: identify which client
    price: prices of instruments
    qty: hold quantites of instruments
    priority: position in a FIFO queue at price level
    side: identify buy or sell side in an order


*/


/*
    book class needs to maintain an array of orders (either sell or buy) -> each index should be a pointer to a sell/buy order



    use mempool to get heap access -> need since stack will be too small for lots of tickers and their books

    on a buy order --> what needs to happen:
        buy order is sent in from order server
        matching engine looks up the order by ticker 
        inserts it into the right position 

    two options:
        when an order comes in --> inserted into the right place, then separate thread polls checking for crossings
WINNER: when an order comes in --> check for matches, then insert if necessary


Engine
└── array[ticker_id] → Book

Book
├── array[1..1025] → PriceLevel  (buy side)
├── array[1..1025] → PriceLevel  (sell side)
├── best_bid: u16
├── best_ask: u16
└── order_map: HashMap<order_id, *OrderNode>

PriceLevel
├── head: *OrderNode
├── tail: *OrderNode
└── total_qty: u64

OrderNode
├── order_id: u64
├── qty: u64
├── price: u16
├── side: enum { Buy, Sell }
├── prev: *OrderNode
└── next: *OrderNode
Insert

Index into array[price] — O(1)
Append OrderNode to tail of doubly linked list — O(1)
Update total_qty — O(1)
Insert into order_map — O(1)
Update best_bid / best_ask if needed — O(1)

Cancel

Lookup order_map[order_id] → get *OrderNode — O(1)
Unlink via prev/next — O(1)
Decrement array[order.price].total_qty — O(1)
Remove from order_map — O(1)
If level is now empty, scan inward to update best_bid/best_ask — O(spread) ≈ O(1) in practice
Return node to arena free list — O(1)

Modify

If only qty changes (downward) → just update qty on the node in place, no queue repositioning — O(1)
If price changes, or qty increases (loses queue priority) → cancel + re-insert — O(1)

The order_map sits at the Book level rather than globally so order IDs only need to be unique per ticker, which is typically how exchanges define them anyway.

*/



template <typename T>
struct HalfBook {
    // Keep an array of memory pool heap of T objects
    std::array<MemoryPoolHeap<T>, 1024> limits;
    size_t bestPrice; // Best Price is the first non-empty index
};


// Consider using a dispatch table to avoid branches
class Book {
public:
    
    // operations: insert, cancel, modify, check for crossing

    // can get o(1) access to the order by using the market_order_id, on insert, 

    void insert_buy(Order& buy_order) {
        // first compare against sell orders 
        if (buy_order.price < sell_book.bestPrice) {
            // in this scenario, this new buy order cannot match with any sells
        } else {
            // match with sell order, may have partial matches or full matches
        }


        // with whatever is remaining, insert it into the function

        if (buy_order.qty > 0) {
            // insert it into the correct price level 
            // call construct so that memory pool can put it in next free space but still has pointers
            for (Order* curr = buy_book.limits[buy_order.price].first_ptr(); curr != nullptr; curr = curr->next) {

            }
        }
    };

    void match_sells(Order& ) {

    }

private:
   
    HalfBook<Order> sell_book;
    HalfBook<Order> buy_book;

    std::unordered_map<market_order_id, Order*> order_map; // used for o(1) cancel/modification

};