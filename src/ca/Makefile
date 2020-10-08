TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessCA
pvAccessCA_LIBS += pvAccess pvData ca Com

SHRLIB_VERSION ?= $(EPICS_PVA_MAJOR_VERSION).$(EPICS_PVA_MINOR_VERSION).$(EPICS_PVA_MAINTENANCE_VERSION)

# needed for Windows
LIB_SYS_LIBS_WIN32 += netapi32 ws2_32

INC += pv/caProvider.h

pvAccessCA_SRCS += caProvider.cpp
pvAccessCA_SRCS += caContext.cpp
pvAccessCA_SRCS += caChannel.cpp
pvAccessCA_SRCS += dbdToPv.cpp
pvAccessCA_SRCS += notifierConveyor.cpp

include $(TOP)/configure/RULES
