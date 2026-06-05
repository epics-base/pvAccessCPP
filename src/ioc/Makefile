TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessIOC

pvAccessIOC_LIBS += pvAccess pvData
pvAccessIOC_LIBS += $(EPICS_BASE_IOC_LIBS)

SHRLIB_VERSION ?= $(EPICS_PVA_MAJOR_VERSION).$(EPICS_PVA_MINOR_VERSION).$(EPICS_PVA_MAINTENANCE_VERSION)

INC += pv/syncChannelFind.h
INC += pv/iocshelper.h
INC += pv/iocreftrack.h

DBD += PVACommonRegister.dbd
DBD += PVAServerRegister.dbd
DBD += PVAClientRegister.dbd

pvAccessIOC_SRCS += PVAServerRegister.cpp
pvAccessIOC_SRCS += PVAClientRegister.cpp
pvAccessIOC_SRCS += reftrackioc.cpp

# fix false (?) warning by gcc 12 only
reftrackioc_CPPFLAGS += $(reftrackioc_CPPFLAGS_GCC-$(GCC_MAJOR))
reftrackioc_CPPFLAGS_GCC-12 = -Wno-dangling-pointer

include $(TOP)/configure/RULES
