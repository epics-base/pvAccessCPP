/*
 * growingCircularBufferTest.cpp
 *
 *  Created on: Nov 11, 2010
 *      Author: Miha Vitorovic
 */

#include <growingCircularBuffer.h>

#include <iostream>
#include <epicsAssert.h>

using namespace epics::pvAccess;
using std::cout;
using std::endl;

const size_t CAPACITY = 10;

void testSimpleType() {
    GrowingCircularBuffer<int> cb(CAPACITY);

    cout<<"Testing circular buffer simple type."<<endl;

    assert(cb.capacity()==CAPACITY);
    assert(cb.size()==0);

    // insert, get test
    bool first = cb.insert(1);
    assert(first);
    assert(cb.size()==1);
    assert(cb.extract()==1);
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
}

void testPointerType() {
    GrowingCircularBuffer<int*> cb(CAPACITY);
    int testVals[] = {0,1,2,3,4,5,6,7,8,9,11,12,13,14,15,16,17,18,19,20};

    cout<<"Testing circular buffer pointer type."<<endl;

    assert(cb.capacity()==CAPACITY);
    assert(cb.size()==0);

    // insert, get test
    bool first = cb.insert(&testVals[1]);
    assert(first);
    assert(cb.size()==1);
    assert(cb.extract()==&testVals[1]);
    assert(cb.size()==0);

    for(size_t i = 0; i<2*CAPACITY; i++) {
        first = cb.insert(&testVals[i]);
        assert(cb.size()==i+1);
        assert((cb.size() == 1)==first);
    }
    assert(cb.size()==2*CAPACITY);

    for(size_t i = 0; i<2*CAPACITY; i++) {
        assert(cb.extract()==&testVals[i]);
        assert(cb.size()==2*CAPACITY-i-1);
    }
    assert(cb.size()==0);

    cout<<"\nPASSED!\n";
}


int main(int argc, char *argv[]) {
    testSimpleType();
    cout<<endl;
    testPointerType();

    return 0;
}
