
#include "spsc_queue.h"
#include "common.h"


typedef SPSCQueue<ClientRequest> ClientRequestQueue;
typedef SPSCQueue<ClientResponse> ClientResponseQueue;
typedef SPSCQueue<OrdersUpdate> OrdersUpdateQueue;

typedef uint32_t ticker_id;

class MatchingEngine {

public:


    // operatios:
    //  -need a way to handle requests from multiple clients
    //  -need a way to communicate back to these clients

    //  -need a separate structure to hold outgoing messages
    //      -need a queue to the update server that sends messages to all clients over UDP

    //  -need a separate structure to recieve messages (MATCHING ENGINE DOES NOT NEED THAT)

    
    // requires a collection of sell and buy orders -> should it be a hashmap of ticker_ids, and then a linked list of diff tickers and inside each is a heap of orders, ordered by best price, then 
    // use a hashmap where each index is a ticker_id : heap of orders
    // buy order should be ordered based on lowest buy (min heap), higher quantity, time
    // sell order should be ordered on highest sell (max heap), lower quantity, time


private:

    OrdersUpdateQueue orders_update_queue;
    ClientRequestQueue client_request_queue;
    ClientResponseQueue client_response_queue;

    std::unordered_map<ticker_id, std::priority_queue<SellOrder>> sell_orders;
    std::unordered_map<ticker_id, std::priority_queue<BuyOrder>> buy_orders;
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