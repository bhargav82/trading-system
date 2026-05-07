#include "../header/mempool.h"
#include "../header/common.h"

int main(int argc, char* argv[]) {
    
    MemoryPoolHeap<Object> mp(4);
    Object* o1 = mp.construct(1, "a"); o1->print();
    Object* o2 = mp.construct(2, "b"); o2->print();
    Object* o3 = mp.construct(3, "c"); o3-> print();
    mp.destruct(o1);
    Object* o5 = mp.construct(5, "e"); o5->print();
    mp.destruct(o5);
    Object* o6 = mp.construct(6, "f"); o6->print();
    Object* o7 = mp.construct(7, "g"); 
    
    return 0;
}