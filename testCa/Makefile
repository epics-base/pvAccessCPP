# Makefile for the ca tests

TOP = ..
include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/src/ca

CAPROVIDER_TEST = $(TOP)/testCa

PROD_LIBS += pvAccess pvAccessCA pvData Com

TESTPROD_HOST += testCaProvider
testCaProvider_SRCS  += testCaProvider.cpp
testHarness_SRCS += testCaProvider.cpp
TESTS += testCaProvider


# Name the application caTestHarness
caTestHarness_SRCS = $(testHarness_SRCS)

# Build for vxWorks
PROD_vxWorks = caTestHarness
TESTSPEC_vxWorks = caTestHarness.$(MUNCH_SUFFIX); pvCaAllTests

# Build for RTEMS, with harness code & configuration
PROD_RTEMS += caTestHarness
caTestHarness_SRCS_RTEMS += rtemsTestHarness.c
TESTSPEC_RTEMS = caTestHarness.$(MUNCH_SUFFIX); pvCaAllTests

# Build test scripts for hosts
TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
