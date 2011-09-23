#include <gtest/gtest.h>

#include <pv/hexDump.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

TEST(hexDumpTest, hexDump)
{
    char TO_DUMP[] = "pvAccess dump test\0\1\2\3\4\5\6\254\255\256";
    
    EXPECT_NO_THROW(hexDump("test", (int8*)TO_DUMP, 18+9));
    EXPECT_NO_THROW(hexDump("only text", (int8*)TO_DUMP, 18));
    EXPECT_NO_THROW(hexDump("22 byte test", (int8*)TO_DUMP, 22));
}
