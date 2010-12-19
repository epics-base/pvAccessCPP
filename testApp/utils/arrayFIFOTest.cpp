/*
 * arrayFIFOTest.cpp
 *
 *  Created on: Nov 9, 2010
 *      Author: Miha Vitorovic
 */

#define ARRAY_FIFO_DEBUG 1
#include "arrayFIFO.h"

#include <iostream>
#include <epicsAssert.h>

using namespace epics::pvAccess;
using std::cout;
using std::endl;

int main(int argc, char *argv[]) {
    ArrayFIFO<int> fifoInt;

    assert(fifoInt.size()==0);
    assert(fifoInt.isEmpty());

    cout<<"Testing clear."<<endl;
    fifoInt.push(3);
    assert(fifoInt.size()==1);

    fifoInt.clear();
    assert(fifoInt.isEmpty());

    cout<<"Testing push/pop."<<endl;
    fifoInt.push(5);
    fifoInt.push(6);
    fifoInt.push(7);
    assert(fifoInt.size()==3);

    assert(fifoInt.pop()==7);
    assert(fifoInt.size()==2);

    assert(fifoInt.pop()==6);
    assert(fifoInt.size()==1);

    assert(fifoInt.pop()==5);
    assert(fifoInt.size()==0);

    cout<<"Testing FIFO ops (first/last)."<<endl;
    fifoInt.addFirst(100);
    fifoInt.addFirst(200);
    fifoInt.addFirst(300);
    fifoInt.addFirst(400);
    fifoInt.addFirst(500);

    assert(fifoInt.size()==5);
    assert(fifoInt.pollLast()==100);
    assert(fifoInt.pollLast()==200);
    assert(fifoInt.pollLast()==300);
    assert(fifoInt.pollLast()==400);
    assert(fifoInt.pollLast()==500);
    assert(fifoInt.isEmpty());

    cout<<"Testing FIFO ops (last/first)."<<endl;
    fifoInt.addLast(150);
    fifoInt.addLast(250);
    fifoInt.addLast(350);
    fifoInt.addLast(450);
    fifoInt.addLast(550);
    assert(fifoInt.size()==5);

    assert(fifoInt.pollFirst()==150);
    assert(fifoInt.pollFirst()==250);
    assert(fifoInt.pollFirst()==350);
    assert(fifoInt.pollFirst()==450);
    assert(fifoInt.pollFirst()==550);
    assert(fifoInt.isEmpty());

    cout<<"Testing remove, peek."<<endl;
    fifoInt.addFirst(1000);
    fifoInt.addFirst(2000);
    fifoInt.addFirst(3000);
    fifoInt.addFirst(4000);
    fifoInt.addFirst(5000);
    fifoInt.addFirst(6000);
    fifoInt.addFirst(7000);
    // - - - - - - - - - - - -
    fifoInt.addFirst(8000);
    fifoInt.addFirst(9000);
    fifoInt.addFirst(10000);

    assert(fifoInt.peekFirst()==10000);
    assert(fifoInt.peekLast()==1000);

    assert(fifoInt.size()==10);
    assert(fifoInt.remove(9000));
    assert(fifoInt.size()==9);
    assert(!fifoInt.remove(3500));
    assert(fifoInt.size()==9);

    assert(fifoInt.pollLast()==1000);
    assert(fifoInt.pollLast()==2000);
    assert(fifoInt.pollLast()==3000);
    assert(fifoInt.pollLast()==4000);
    assert(fifoInt.pollLast()==5000);
    assert(fifoInt.pollLast()==6000);
    assert(fifoInt.pollLast()==7000);
    // - - - - - - - - - - - -
    assert(fifoInt.pollLast()==8000);
    assert(fifoInt.pollLast()==10000);
    assert(fifoInt.isEmpty());

    cout<<"Testing increase buffer."<<endl;
    fifoInt.addLast(100100);
    fifoInt.addLast(100200);
    fifoInt.addLast(100300);
    fifoInt.addLast(100400);
    fifoInt.addLast(100500);
    fifoInt.addLast(100600);
    fifoInt.addLast(100700);
    fifoInt.addLast(100800);
    fifoInt.addLast(100900);
    fifoInt.addLast(101000);
    fifoInt.addLast(101100);
    fifoInt.addLast(101200);
    fifoInt.addLast(101300);
    fifoInt.addLast(101400);
    fifoInt.addLast(101500);
    fifoInt.addLast(101600);
    fifoInt.addLast(101700);
    fifoInt.addLast(101800);

    assert(fifoInt.size()==18);
    fifoInt.debugState();
    fifoInt.clear();
    assert(fifoInt.isEmpty());

    cout<<"\nPASSED!\n";
    return 0;
}
