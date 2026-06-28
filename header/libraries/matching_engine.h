
#include "spsc_queue.h"
#include "common.h"
#include "mempool.h"


typedef SPSCQueue<ClientRequest> ClientRequestQueue; // receive orders from order server
typedef SPSCQueue<ClientResponse> ClientResponseQueue; // send confirmation messages (or failure) to order server
typedef SPSCQueue<OrdersUpdate> OrdersUpdateQueue; // send updates on matches 

typedef uint32_t ticker_id;

class MatchingEngine {
public:



    // requires a collection of sell and buy orders -> should it be a hashmap of ticker_ids, and then a linked list of diff tickers and inside each is a heap of orders, ordered by best price, then 
    // use a hashmap where each index is a ticker_id : heap of orders
    // for each ticker -> maintain an order boook (limit prices from $1-$1025)


private:

    OrdersUpdateQueue orders_update_queue;
    ClientRequestQueue client_request_queue;
    ClientResponseQueue client_response_queue;

    // make own lock free hash tables
    std::unordered_map<ticker_id, Book<SellOrder>> sell_books;
    std::unordered_map<ticker_id, Book<BuyOrder>> buy_books;
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
*/
template <typename T>
class Book {
public:
    Book() : {}
    


private:
    MemoryPoolHeap<T*> limit_order_book {1024};
    

};