# This is a Makefile fragment, see ../Makefile

TOP = ..
include $(TOP)/configure/CONFIG

INSTALL_INCLUDE = $(INSTALL_LOCATION)/include/pv
USR_INCLUDES += -I$(INSTALL_LOCATION)/include

LIBRARY += pva

INC += pva.h
INC += pvaMultiDouble.h
INC += pvaNTMultiChannel.h

LIBSRCS += pva.cpp
LIBSRCS += pvaPutData.cpp
LIBSRCS += pvaGetData.cpp
LIBSRCS += pvaMonitorData.cpp
LIBSRCS += pvaChannel.cpp
LIBSRCS += pvaProcess.cpp
LIBSRCS += pvaGet.cpp
LIBSRCS += pvaPut.cpp
LIBSRCS += pvaMonitor.cpp
LIBSRCS += pvaPutGet.cpp
LIBSRCS += pvaMultiChannel.cpp
LIBSRCS += pvaMultiDouble.cpp
LIBSRCS += pvaNTMultiChannel.cpp
#LIBSRCS += pvaRPC.cpp

pva_LIBS += pvAccess pvData nt Com
pva_LIBS +=  $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

