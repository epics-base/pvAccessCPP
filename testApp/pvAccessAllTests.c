/*
 * Run pvData tests as a batch.
 *
 * Do *not* include performance measurements here, they don't help to
 * prove functionality (which is the point of this convenience routine).
 */

#include <stdio.h>
#include <epicsThread.h>
#include <epicsUnitTest.h>
#include <epicsExit.h>

/* utils */
int testAtomicBoolean(void);
int testHexDump(void);
int testInetAddressUtils(void);

/* remote */
int testCodec(void);
int testChannelAccess(void);

void pvAccessAllTests(void)
{
    testHarness();

    /* utils */
    runTest(testAtomicBoolean);
    runTest(testHexDump);
    runTest(testInetAddressUtils);

    /* remote */
    runTest(testCodec);
    runTest(testChannelAccess);

    epicsExit(0);   /* Trigger test harness */
}
