TOP=../..

include $(TOP)/configure/CONFIG

PROD_HOST += hexDumpTest
hexDumpTest_SRCS += hexDumpTest.cpp
hexDumpTest_LIBS += pvAccess pvData

PROD_HOST += wildcharMatcherTest
wildcharMatcherTest_SRCS += wildcharMatcherTest.cpp
wildcharMatcherTest_LIBS += pvAccess pvData Com

PROD_HOST += inetAddressUtilsTest
inetAddressUtilsTest_SRCS += inetAddressUtilsTest.cpp
inetAddressUtilsTest_LIBS += pvAccess pvData Com

PROD_HOST += loggerTest
loggerTest_SRCS += loggerTest.cpp
loggerTest_LIBS += pvAccess pvData Com

PROD_HOST += introspectionRegisterTest
introspectionRegisterTest_SRCS += introspectionRegistryTest.cpp
introspectionRegisterTest_LIBS += pvAccess Com pvData

PROD_HOST += transportRegisterTest
transportRegisterTest_SRCS += transportRegistryTest.cpp
transportRegisterTest_LIBS += pvAccess Com pvData

PROD_HOST += namedLockPatternTest
namedLockPatternTest_SRCS += namedLockPatternTest.cpp
namedLockPatternTest_LIBS += pvAccess Com pvData

PROD_HOST += configurationTest
configurationTest_SRCS += configurationTest.cpp
configurationTest_LIBS += pvAccess Com pvData

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
