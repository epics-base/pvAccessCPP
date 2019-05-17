# Makefile for the ca tests

TOP = ..
include $(TOP)/configure/CONFIG

# Need access to caProviderPvt.h
USR_CPPFLAGS += -I$(TOP)/src/ca

PROD_LIBS += pvAccess pvAccessCA pvData $(EPICS_BASE_IOC_LIBS)
PROD_SYS_LIBS_WIN32 += netapi32 ws2_32

TESTPROD_HOST += testCaProvider
testCaProvider_SRCS += testCaProvider.cpp
TESTS += testCaProvider
ifdef BASE_3_16
  testCaProvider_SRCS += testIoc_registerRecordDeviceDriver.cpp
  REGRDDFLAGS = -l
endif
caTestHarness_SRCS += $(testCaProvider_SRCS)

# Ensure EPICS_HOST_ARCH is set in the environment
export EPICS_HOST_ARCH

ifdef BASE_3_16
  # Embedded OSes need Base-3.16.2 or higher to pass tests

  # Code that runs all tests in the collection
  caTestHarness_SRCS += pvCaAllTests.c

  # Build for vxWorks
  PROD_vxWorks = caTestHarness
  TESTSPEC_vxWorks = caTestHarness.$(MUNCH_SUFFIX); pvCaAllTests

  # Build for RTEMS, with harness code & configuration
  PROD_RTEMS += caTestHarness
  caTestHarness_SRCS_RTEMS += rtemsTestHarness.c
  TESTSPEC_RTEMS = caTestHarness.$(MUNCH_SUFFIX); pvCaAllTests
endif

# Build test scripts for hosts
TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

ifdef BASE_3_16
  $(COMMON_DIR)/testIoc.dbd: $(EPICS_BASE)/dbd/softIoc.dbd
	$(RM) $@
	$(CP) $< $@
endif
