/*
 * Run all caProvider tests
 */

#include <stdio.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <epicsExit.h>

int testCaProvider(void);
int testConveyor(void);

void pvCaAllTests(void)
{
    testHarness();
    runTest(testConveyor);
    runTest(testCaProvider);

    epicsExit(0);   /* Trigger test harness */
}
