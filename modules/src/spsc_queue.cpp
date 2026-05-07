#include "../header/spsc_queue.h"
#include "../header/common.h"
#include "../header/mempool.h"

int main(int argc, char* argv[]) {
    SPSCQueue<SimpleObj> obj_queue(3);
    obj_queue.emplace(2, "bhargav");
    obj_queue.emplace(1, "bhargav");
    obj_queue.emplace(4, "bhargav");
    obj_queue.emplace(3, "bhargav");

    obj_queue.pop();
    obj_queue.pop();
    obj_queue.emplace(54, "nharag");
    obj_queue.print();


    return 0;
    
}