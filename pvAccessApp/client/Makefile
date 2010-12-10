TOP=../..

include $(TOP)/configure/CONFIG

INC += pvAccess.h

LIBSRCS += ChannelAccessFactory.cpp

LIBRARY=pvAccessClient

pvAccessClient_LIBS += pvData
pvData_DIR = $(PVDATA_HOME)/lib/$(EPICS_HOST_ARCH)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

