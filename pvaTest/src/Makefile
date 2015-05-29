# Makefile for the pvAccess tests

TOP = ..
include $(TOP)/configure/CONFIG

TESTPROD_HOST += testPvaPutData
testPvaPutData_SRCS = testPvaPutData 
testHarness_SRCS += testPvaPutData.cpp 
TESTS += testPvaPutData

TESTPROD_HOST += testPvaGetData
testPvaGetData_SRCS = testPvaGetData 
testHarness_SRCS += testPvaGetData.cpp 
TESTS += testPvaGetData

TESTPROD_HOST += testPvaMonitorData
testPvaMonitorData_SRCS = testPvaMonitorData 
testHarness_SRCS += testPvaMonitorData.cpp 
TESTS += testPvaMonitorData

TESTPROD_HOST += testPvaPutGetMonitor
testPvaPutGetMonitor_SRCS = testPvaPutGetMonitor 
testHarness_SRCS += testPvaPutGetMonitor.cpp 
TESTS += testPvaPutGetMonitor

TESTPROD_HOST += testPvaPutGet
testPvaPutGet_SRCS = testPvaPutGet
testHarness_SRCS += testPvaPutGet.cpp 
TESTS += testPvaPutGet

TESTPROD_HOST += testPvaMultiDouble
testPvaMultiDouble_SRCS = testPvaMultiDouble
testHarness_SRCS += testPvaMultiDouble.cpp 
TESTS += testPvaMultiDouble

TESTPROD_HOST += testPvaNTMultiChannel
testPvaNTMultiChannel_SRCS = testPvaNTMultiChannel
testHarness_SRCS += testPvaNTMultiChannel.cpp 
TESTS += testPvaNTMultiChannel


PROD_LIBS += pva pvAccess pvData nt Com

testHarness_SRCS += easyAllTests.c

PROD_vxWorks = vxTestHarness
vxTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_vxWorks = vxTestHarness.$(MUNCH_SUFFIX); easyAllTests

PROD_RTEMS += rtemsTestHarness
rtemsTestHarness_SRCS += rtemsTestHarness.c rtemsConfig.c
rtemsTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_RTEMS = rtemsTestHarness.$(MUNCH_SUFFIX); easyAllTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
