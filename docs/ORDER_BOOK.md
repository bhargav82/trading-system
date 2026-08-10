# Order Book & Matching Engine

`header/libraries/matching_engine.h`

## Overview

This is a price-time-priority limit order book: at any given price, orders are filled
in the order they arrived (FIFO). The best available price matched first when orders cross:
(bestBuy >= bestSell)

```
MatchingEngine
  └── orders_update_q: SPSC Queue                   (sends order update to order update server)
  └── client_req_q: SPSC Queue                      (receives client request from order server)
  └── client_resp_q: SPSC Queue                     (sends client response to order server)
  └── books: unordered_map<ticker_id, Book*>        (one book per ticker) 
        Book
          ├── buy_book:  HalfBook
          └── sell_book: HalfBook
                HalfBook
                  └── levels: deque<PriceLevel<Order>>   (indexed by price)
                        PriceLevel<Order>
                          ├── orders: MemoryPoolHeap<Order>   (see MEMORY_POOL.md)
                          └── head/tail: doubly-linked list of Order
```

## `PriceLevel<T>` — one price, FIFO order

Each price level is an doubly-linked list (orders carry their own `next`/`prev` pointers)
with sentinel `head`/`tail` nodes to ease insert/remove operations. 
The list's nodes are allocated from a `MemoryPoolHeap<Order>`: one pool per price
level, so adding or removing an order at that price never touches the general-purpose
allocator on the hot path.

- `add_order()` appends to the tail: FIFO within the level, in `O(1)`.
- `remove_order()` unlinks a node given its pointer and returns it to the pool, also
  `O(1)`.
- `top()` returns the earliest (best-priority) live order at this level.

## `HalfBook` — one side of the market

A `HalfBook` (buy side or sell side) owns a `std::deque<PriceLevel<Order>>` indexed
directly by price (`levels[price - 1]`) — a `deque` rather than a `vector` specifically
because `PriceLevel` is neither copyable nor default-constructible (it owns a
`MemoryPoolHeap`), so it needs to be constructed in place rather than requiring
reallocation-driven copies/moves the way a growing `vector` would.

Prices currently run `1..128` (see `ME_MAX_PRICE_LEVELS`) using direct array indexing
into price levels gives `O(1)` access at the cost of a fixed, currently-small price
range. Tick size is $1.


## `Book` — both sides, plus best bid/ask
`Book` owns both `HalfBook`s and tracks `bestBuy`/`bestSell`:

- **On insert**, updating the best price is `O(1)` — just compare the new order's price
  against the current best.
- **On removal**, if the removed order *was* the best price, the code walks toward the
  next occupied price level (`bestBuy` downward, `bestSell` upward) until it finds one
  that's non-empty. This is the one place that isn't strictly `O(1)`, but it's by how 
  many consecutive empty price levels exist between the old best and the next real one.
- **`handle_trades()`** is where actual matching happens: an incoming order walks the
  opposite side's price levels from the best price outward, filling against resting
  orders until either the incoming order is fully filled or no more price levels can
  match. Whatever quantity is left over (if any) gets inserted as a new resting order.
  Every fill, partial or complete, emits an `OrdersUpdate` onto an `SPSCQueue`,
  this is the connection point to [SPSC_QUEUE.md](SPSC_QUEUE.md).


## `MatchingEngine` — the public entry point
Owns the `ticker_id -> Book*` map and an `order_id -> Order*` map for `O(1)` cancel by
ID, plus the three `SPSCQueue`s that carry requests in and responses/updates out. This
is the type the rest of the system (network layer, once it exists) is meant to talk to.

## Testing status

`header/tests/matching_engine.cpp` covers:
- Best-buy/best-sell tracking as orders are inserted and removed across many price
  levels, including the "walk to the next occupied level" removal path.
- Trade matching on both the buy side and the sell side, including partial fills,
  complete fills, and fills against multiple resting orders from a single incoming
  order.

Tests use `FRIEND_TEST` to reach into `Book`'s private state (`bestBuy`, `bestSell`,
the underlying `buy_book`/`sell_book`). 

## Known gaps / next steps

- **No order modification support yet**, only new orders and full cancels are done.
- **Network layer isn't wired in yet.** `tcp_socket.h`/`socket_utils.h` exist as
  standalone pieces (batching sends, framed message parsing) but the matching engine
  doesn't yet receive `ClientRequest`s or emit `ClientResponse`s over an actual socket,
  today it's driven directly by test code calling `add_order()`/`handle_trades()`.
