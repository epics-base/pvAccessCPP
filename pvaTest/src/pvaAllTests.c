/*
 * Run PvaPVA tests as a batch.
 *
 * Do *not* include performance measurements here, they don't help to
 * prove functionality (which is the point of this convenience routine).
 */

#include <stdio.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>

int testPvaGetData(void);
int testPvaPutData(void);
int testPvaMonitorData(void);
int testPvaPutGetMonitor(void);
int testPvaPutGet(void);
int testPvaMultiDouble(void);
int testPvaNTMultiChannel(void);

void easyAllTests(void)
{
    testHarness();
    runTest(testPvaGetData);
    runTest(testPvaPutData);
    runTest(testPvaMonitorData);
    runTest(testPvaPutMonitor);
    runTest(testPvaPut);
    runTest(testPvaMultiDouble);
    runTest(testPvaNTMultiChannel);
}

