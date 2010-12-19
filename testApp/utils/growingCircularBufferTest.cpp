/*
 * growingCircularBufferTest.cpp
 *
 *  Created on: Nov 11, 2010
 *      Author: Miha Vitorovic
 */

#include "growingCircularBuffer.h"

#include <iostream>
#include <epicsAssert.h>

using namespace epics::pvAccess;
using std::cout;
using std::endl;

const size_t CAPACITY = 10;

int main(int argc, char *argv[]) {
    GrowingCircularBuffer<int> cb(CAPACITY);

    cout<<"Testing circular buffer."<<endl;

    assert(cb.capacity()==CAPACITY);
    assert(cb.size()==0);

    // insert, get test
    bool first = cb.insert(1);
    assert(cb.size()==1);
    assert(cb.extract()==1);
    assert(first);
    assert(cb.size()==0);

    for(size_t i = 0; i<2*CAPACITY; i++) {
        first = cb.insert(i);
        assert(cb.size()==i+1);
        assert((cb.size() == 1)==first);
    }
    assert(cb.size()==2*CAPACITY);

    for(size_t i = 0; i<2*CAPACITY; i++) {
        assert(cb.extract()==(int)i);
        assert(cb.size()==2*CAPACITY-i-1);
    }
    assert(cb.size()==0);

    cout<<"\nPASSED!\n";
    return 0;

}
