/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <vector>

#include <pv/fairQueue.h>

#include <epicsUnitTest.h>
#include <testMain.h>

namespace {

struct Qnode : public epics::pvAccess::fair_queue<Qnode>::entry {
    unsigned i;
    Qnode(unsigned i):i(i) {}
};

} // namespace

static unsigned Ninput[]  = {0,0,0,1,0,2,1,0,1,0,0};
static unsigned Nexpect[] = {0,1,2,0,1,0,1,0,0,0,0};

static
void testOrder()
{
    epics::pvAccess::fair_queue<Qnode> Q;
    typedef epics::pvAccess::fair_queue<Qnode>::value_type value_type;

    std::vector<value_type> unique, inputs, outputs;
    unique.resize(3);
    unique[0].reset(new Qnode(0));
    unique[1].reset(new Qnode(1));
    unique[2].reset(new Qnode(2));

    testDiag("Queueing");

    for(unsigned i=0; i<NELEMENTS(Ninput); i++) {
        testDiag("[%u] = %u", i, Ninput[i]);
        Q.push_back(unique[Ninput[i]]);
    }

    testDiag("De-queue");

    {
        for(unsigned i=0; i<=NELEMENTS(Nexpect); i++) {
            value_type E;
            Q.pop_front_try(E);
            if(!E) break;
            outputs.push_back(E);
            testDiag("Dequeue %u", E->i);
        }
    }

    testOk(outputs.size()==NELEMENTS(Nexpect), "sizes match actual %u expected %u",
           (unsigned)outputs.size(), (unsigned)NELEMENTS(Nexpect));

    for(unsigned i=0; i<NELEMENTS(Nexpect); i++) {
        if(i>=outputs.size()) {
            testFail("output truncated");
            continue;
        }
        testOk(outputs[i]->i==Nexpect[i], "[%u] %u == %u",
               i, (unsigned)outputs[i]->i, Nexpect[i]);
    }
}

MAIN(testFairQueue)
{
    testPlan(12);
    testOrder();
    return testDone();
}
