#include "../header/spsc_queue.h"
#include "../header/common.h"
#include "../header/mempool.h"


int main(int argc, char* argv[]) {
    SPSCQueue<SimpleObj> obj_queue(3);
    obj_queue.emplace_back(1, "first");
    obj_queue.emplace_back(2, "second");
    obj_queue.emplace_back(3, "third");
    obj_queue.emplace_back(4, "fourth");

    obj_queue.pop();
    obj_queue.pop();
    obj_queue.emplace_back(5, "fifth");
    obj_queue.print();

    // need to figure out how to handle when pop ptr goes overboard and wrap around
    return 0;
    
}

