#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/remote.h>

using namespace epics::pvAccess;

MAIN(testAtomicBoolean)
{
    testPlan(5);
    testDiag("Tests for AtomicBoolean");

    AtomicBoolean ab;

    testOk(ab.get() == false, "Initial state");
    ab.set();
    testOk(ab.get() == true, "Set to true");
    ab.set();
    testOk(ab.get() == true, "Set to true (again)");
    ab.clear();
    testOk(ab.get() == false, "Set to false");
    ab.clear();
    testOk(ab.get() == false, "Set to again");

    return testDone();
}
