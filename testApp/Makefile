# Makefile for the pvAccess tests

TOP = ..
include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/src/utils
USR_CPPFLAGS += -I$(TOP)/src/server
USR_CPPFLAGS += -I$(TOP)/src/remote
USR_CPPFLAGS += -I$(TOP)/src/remoteClient

PVACCESS_TEST = $(TOP)/testApp

PROD_LIBS += pvAccess pvData Com

include $(PVACCESS_TEST)/utils/Makefile
include $(PVACCESS_TEST)/remote/Makefile

# The testHarness runs all the test programs in a known working order.
testHarness_SRCS += pvAccessAllTests.c

PROD_vxWorks = vxTestHarness
vxTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_vxWorks = vxTestHarness.$(MUNCH_SUFFIX); pvAccessAllTests

PROD_RTEMS += pvaTestHarness
pvaTestHarness_SRCS += rtemsTestHarness.c rtemsConfig.c
pvaTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_RTEMS = pvaTestHarness.$(MUNCH_SUFFIX); pvAccessAllTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
