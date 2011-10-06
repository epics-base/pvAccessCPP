#include <gtest/gtest.h>

#include <remote.h>

using namespace epics::pvAccess;

TEST(atomicBooleanTest, atomicBoolean)
{
    AtomicBoolean ab;

    EXPECT_FALSE(ab.get());
    ab.set();
    EXPECT_TRUE(ab.get());
    ab.clear();
    EXPECT_FALSE(ab.get());
}
