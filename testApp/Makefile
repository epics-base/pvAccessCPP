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

# pvAccessAllTests runs all the test programs in a known working order.
testHarness_SRCS += pvAccessAllTests.c

# Name the application pvaTestHarness
pvaTestHarness_SRCS = $(testHarness_SRCS)

# Build for vxWorks
PROD_vxWorks = pvaTestHarness
TESTSPEC_vxWorks = pvaTestHarness.$(MUNCH_SUFFIX); pvAccessAllTests

# Build for RTEMS, with harness code & configuration
PROD_RTEMS += pvaTestHarness
pvaTestHarness_SRCS_RTEMS += rtemsTestHarness.c
TESTSPEC_RTEMS = pvaTestHarness.$(MUNCH_SUFFIX); pvAccessAllTests

# Build test scripts for hosts
TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
