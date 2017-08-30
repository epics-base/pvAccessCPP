TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessIOC

pvAccessIOC_LIBS += pvAccess pvData
pvAccessIOC_LIBS += $(EPICS_BASE_IOC_LIBS)

INC += pv/syncChannelFind.h
INC += pv/iocshelper.h

DBD += PVAServerRegister.dbd
DBD += PVAClientRegister.dbd

pvAccessIOC_SRCS += PVAServerRegister.cpp
pvAccessIOC_SRCS += PVAClientRegister.cpp
pvAccessIOC_SRCS += reftrackioc.cpp

include $(TOP)/configure/RULES
