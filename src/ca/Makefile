TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessCA
pvAccessCA_LIBS += pvAccess pvData ca Com

SHRLIB_VERSION ?= $(EPICS_PVA_MAJOR_VERSION).$(EPICS_PVA_MINOR_VERSION).$(EPICS_PVA_MAINTENANCE_VERSION)

# needed for Windows
LIB_SYS_LIBS_WIN32 += netapi32 ws2_32

INC += pv/caProvider.h

pvAccessCA_SRCS += channelConnectThread.cpp
pvAccessCA_SRCS += monitorEventThread.cpp
pvAccessCA_SRCS += getDoneThread.cpp
pvAccessCA_SRCS += putDoneThread.cpp
pvAccessCA_SRCS += caProvider.cpp
pvAccessCA_SRCS += caChannel.cpp
pvAccessCA_SRCS += dbdToPv.cpp

include $(TOP)/configure/RULES
