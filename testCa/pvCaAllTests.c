/*
 *
 */

#include <stdio.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <epicsExit.h>

int testCaProvider(void);

void pvCaAllTests(void)
{
    testHarness();
    runTest(testCaProvider);

    epicsExit(0);   /* Trigger test harness */
}
