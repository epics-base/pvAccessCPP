# Makefile for the ca tests

TOP = ..
include $(TOP)/configure/CONFIG

# Access to uninstalled headers
USR_CPPFLAGS += -I$(TOP)/src/ca

TESTPROD_HOST += testConveyor
testConveyor_SRCS += testConveyor.cpp
TESTS += testConveyor

TESTPROD_HOST += testCaProvider
testCaProvider_SRCS += testCaProvider.cpp

PROD_LIBS += pvAccess pvAccessCA pvData $(EPICS_BASE_IOC_LIBS)
PROD_SYS_LIBS_WIN32 += netapi32 ws2_32

ifdef BASE_3_16
  PROD_SRCS_RTEMS += rtemsTestData.c
  testCaProvider_SRCS += testIoc_registerRecordDeviceDriver.cpp
  REGRDDFLAGS = -l

  TESTS += testCaProvider
  TESTFILES += ../testCaProvider.db $(COMMON_DIR)/testIoc.dbd

  # Build test scripts for hosts
  TESTSCRIPTS_HOST += $(TESTS:%=%.t)

  ifneq ($(filter $(T_A),$(CROSS_COMPILER_RUNTEST_ARCHS)),)
    TESTPROD = $(TESTPROD_HOST)
    TESTSCRIPTS = $(TESTSCRIPTS_HOST)
  endif

  # Ensure EPICS_HOST_ARCH is set in the environment
  export EPICS_HOST_ARCH

  caTestHarness_SRCS += testConveyor.cpp
  caTestHarness_SRCS += $(testCaProvider_SRCS)
  caTestHarness_SRCS += pvCaAllTests.c

  # Build harness for vxWorks and RTEMS
  PROD_vxWorks = caTestHarness
  PROD_RTEMS = caTestHarness
  caTestHarness_SRCS_RTEMS += rtemsTestHarness.c
endif

include $(TOP)/configure/RULES

ifdef BASE_3_16
  $(COMMON_DIR)/testIoc.dbd: $(EPICS_BASE)/dbd/softIoc.dbd
	$(RM) $@
	$(CP) $< $@

  rtemsTestData.c : $(TESTFILES) $(TOOLS)/epicsMakeMemFs.pl
	$(PERL) $(TOOLS)/epicsMakeMemFs.pl $@ epicsRtemsFSImage $(TESTFILES)
endif
