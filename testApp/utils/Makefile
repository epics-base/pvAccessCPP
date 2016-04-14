# This is a Makefile fragment, see ../Makefile

SRC_DIRS += $(PVACCESS_TEST)/utils

TESTPROD_HOST += testAtomicBoolean
testAtomicBoolean_SRCS = testAtomicBoolean.cpp
testHarness_SRCS += testAtomicBoolean.cpp
TESTS += testAtomicBoolean

TESTPROD_HOST += testHexDump
testHexDump = testHexDump.cpp
testHarness_SRCS += testHexDump.cpp
TESTS += testHexDump

TESTPROD_HOST += testInetAddressUtils
testInetAddressUtils = testInetAddressUtils.cpp
# Avoid errors from inlined htonl() etc. used as template argument
# TODO this is gcc only
testInetAddressUtils_CXXFLAGS = -O0
# needed for 64-bit Windows
testInetAddressUtils_SYS_LIBS_WIN32 += ws2_32
testHarness_SRCS += testInetAddressUtils.cpp
TESTS += testInetAddressUtils

#TESTPROD_HOST += loggerTest
#loggerTest_SRCS += loggerTest.cpp
#testHarness_SRCS += loggerTest.cpp
#TESTS += loggerTest

#TESTPROD_HOST += introspectionRegisterTest
#introspectionRegisterTest_SRCS += introspectionRegistryTest.cpp
#testHarness_SRCS += introspectionRegistryTest.cpp
#TESTS += introspectionRegistryTest

#TESTPROD_HOST += transportRegisterTest
#transportRegisterTest_SRCS += transportRegistryTest.cpp
#testHarness_SRCS += transportRegistryTest.cpp
#TESTS += transportRegistryTest

#TESTPROD_HOST += namedLockPatternTest
#namedLockPatternTest_SRCS += namedLockPatternTest.cpp
#testHarness_SRCS += namedLockPatternTest.cpp
#TESTS += namedLockPatternTest

TESTPROD_HOST += configurationTest
configurationTest_SRCS += configurationTest.cpp
configurationTest_SYS_LIBS_WIN32 += ws2_32
#testHarness_SRCS += configurationTest.cpp
TESTS += configurationTest

TESTPROD_HOST += testFairQueue
testFairQueue_SRCS += testFairQueue
TESTS += testFairQueue

TESTPROD_HOST += testWildcard
testWildcard = testWildcard.cpp
testHarness_SRCS += testWildcard.cpp
TESTS += testWildcard
