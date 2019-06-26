
#include <sstream>

#include <testMain.h>

#include <pv/pvUnitTest.h>
#include <pv/hexDump.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

MAIN(testHexDump)
{
    testPlan(1);
    testDiag("Tests for hexDump");

    char TO_DUMP[] = "pvAccess dump test\0\1\2\3\4\5\6\xfd\xfe\xff";

    std::ostringstream msg;
    msg<<HexDump(TO_DUMP, sizeof(TO_DUMP)-1);

    testEqual(msg.str(), "0x00 70764163 63657373 2064756d 70207465  pvAc cess  dum p te\n"
                         "0x10 73740001 02030405 06fdfeff           st.. .... ....\n");

    return testDone();
}
