TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessIOC

pvAccessIOC_LIBS += pvAccess

INC += pv/syncChannelFind.h

DBD += PVAServerRegister.dbd
DBD += PVAClientRegister.dbd

LIBSRCS += PVAServerRegister.cpp
LIBSRCS += PVAClientRegister.cpp

pvAccessIOC_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
