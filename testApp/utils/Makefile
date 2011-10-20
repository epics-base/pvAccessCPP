TOP=../..

include $(TOP)/configure/CONFIG

ifdef GTEST
PROD_HOST += testUtils
testUtils_SRCS += hexDumpTest.cpp
testUtils_SRCS += inetAddressUtilsTest.cpp
testUtils_SRCS += atomicBooleanTest.cpp
testUtils_LIBS += pvAccess pvData Com gtest_main
endif

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
