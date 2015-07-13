# Makefile for the pvAccess tests

TOP = ..
include $(TOP)/configure/CONFIG

PVACCESS_TEST = $(TOP)/testApp

PROD_LIBS += pvAccess pvData $(MBLIB) Com

include $(PVACCESS_TEST)/utils/Makefile
include $(PVACCESS_TEST)/remote/Makefile

# The testHarness runs all the test programs in a known working order.
testHarness_SRCS += pvAccessAllTests.c

PROD_vxWorks = vxTestHarness
vxTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_vxWorks = vxTestHarness.$(MUNCH_SUFFIX); pvAccessAllTests

PROD_RTEMS += rtemsTestHarness
rtemsTestHarness_SRCS += rtemsTestHarness.c rtemsConfig.c
rtemsTestHarness_SRCS += $(testHarness_SRCS)
TESTSPEC_RTEMS = rtemsTestHarness.$(MUNCH_SUFFIX); pvAccessAllTests

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
