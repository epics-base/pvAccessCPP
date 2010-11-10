/*
 * arrayFIFOTest.cpp
 *
 *  Created on: Nov 9, 2010
 *      Author: Miha Vitorovic
 */

#include <iostream>
#include <epicsAssert.h>

#include "arrayFIFO.h"

using namespace epics::pvAccess;
using std::cout;
using std::endl;

int main(int argc, char *argv[]) {
    ArrayFIFO<int> fifo;

    assert(fifo.size()==0);
    assert(fifo.isEmpty());

    cout<<"Testing clear..."<<endl;
    fifo.push(3);
    assert(fifo.size()==1);

    fifo.clear();
    assert(fifo.isEmpty());

    cout<<"Testing push/pop..."<<endl;
    fifo.push(5);
    fifo.push(6);
    fifo.push(7);
    assert(fifo.size()==3);

    assert(fifo.pop()==7);
    assert(fifo.size()==2);

    assert(fifo.pop()==6);
    assert(fifo.size()==1);

    assert(fifo.pop()==5);
    assert(fifo.size()==0);

    cout<<"\nPASSED!\n";
    return 0;
}
