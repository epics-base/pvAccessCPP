TOP=../..

include $(TOP)/configure/CONFIG

PROD_LIBS += pvAccess pvData pvMB Com

TESTPROD_HOST += testAtomicBoolean
testAtomicBoolean_SRCS = testAtomicBoolean.cpp
TESTS += testAtomicBoolean

TESTPROD_HOST += testHexDump
testHexDump = testHexDump.cpp
TESTS += testHexDump

TESTPROD_HOST += testInetAddressUtils
testInetAddressUtils = testInetAddressUtils.cpp
# Avoid errors from inlined htonl() etc. used as template argument
# TODO this is gcc only
testInetAddressUtils_CXXFLAGS = -O0
# needed for 64-bit Windows
testInetAddressUtils_SYS_LIBS_WIN32 += ws2_32
TESTS += testInetAddressUtils

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

#PROD_HOST += loggerTest
#loggerTest_SRCS += loggerTest.cpp
#loggerTest_LIBS += pvAccess pvData Com

#PROD_HOST += introspectionRegisterTest
#introspectionRegisterTest_SRCS += introspectionRegistryTest.cpp
#introspectionRegisterTest_LIBS += pvAccess Com pvData

#PROD_HOST += transportRegisterTest
#transportRegisterTest_SRCS += transportRegistryTest.cpp
#transportRegisterTest_LIBS += pvAccess Com pvData

#PROD_HOST += namedLockPatternTest
#namedLockPatternTest_SRCS += namedLockPatternTest.cpp
#namedLockPatternTest_LIBS += pvAccess Com pvData

#PROD_HOST += configurationTest
#configurationTest_SRCS += configurationTest.cpp
#configurationTest_LIBS += pvAccess Com pvData

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
