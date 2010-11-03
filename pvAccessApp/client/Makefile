TOP=../..

include $(TOP)/configure/CONFIG

INC += pvAccess.h

LIBSRCS += ChannelAccessFactory.cpp

LIBRARY=pvAccessClient

pvAccessClient_LIBS += pvMisc
pvMisc_DIR = $(PVDATA_HOME)/lib/$(EPICS_HOST_ARCH)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

