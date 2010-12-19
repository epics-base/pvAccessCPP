TOP=../..

include $(TOP)/configure/CONFIG

PROD_HOST += hexDumpTest
hexDumpTest_SRCS += hexDumpTest.cpp
hexDumpTest_LIBS += pvAccess

PROD_HOST += wildcharMatcherTest
wildcharMatcherTest_SRCS += wildcharMatcherTest.cpp
wildcharMatcherTest_LIBS += pvAccess Com

PROD_HOST += arrayFIFOTest
arrayFIFOTest_SRCS += arrayFIFOTest.cpp
arrayFIFOTest_LIBS += pvAccess Com

PROD_HOST += growingCircularBufferTest
growingCircularBufferTest_SRCS += growingCircularBufferTest.cpp
growingCircularBufferTest_LIBS += pvAccess Com

PROD_HOST += inetAddressUtilsTest
inetAddressUtilsTest_SRCS += inetAddressUtilsTest.cpp
inetAddressUtilsTest_LIBS += pvAccess Com pvData
pvData_DIR = $(PVDATA_HOME)/lib/$(EPICS_HOST_ARCH)

PROD_HOST += loggerTest
loggerTest_SRCS += loggerTest.cpp
loggerTest_LIBS += pvAccess Com


include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
