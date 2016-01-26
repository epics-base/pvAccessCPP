# Makefile for the pvAccess library

TOP = ..
include $(TOP)/configure/CONFIG

EPICS_PVA_MAJOR_VERSION = 4
EPICS_PVA_MINOR_VERSION = 2
EPICS_PVA_MAINTENANCE_VERSION = 0
EPICS_PVA_DEVELOPMENT_FLAG = 1

EXPANDVARS += EPICS_PVA_MAJOR_VERSION
EXPANDVARS += EPICS_PVA_MINOR_VERSION
EXPANDVARS += EPICS_PVA_MAINTENANCE_VERSION
EXPANDVARS += EPICS_PVA_DEVELOPMENT_FLAG

EXPANDFLAGS += $(foreach var,$(EXPANDVARS),-D$(var)="$(strip $($(var)))")

INSTALL_INCLUDE = $(INSTALL_LOCATION)/include/pv
USR_INCLUDES += -I$(INSTALL_LOCATION)/include

PVACCESS_SRC = $(TOP)/src

include $(PVACCESS_SRC)/pva/Makefile
include $(PVACCESS_SRC)/utils/Makefile
include $(PVACCESS_SRC)/client/Makefile
include $(PVACCESS_SRC)/factory/Makefile
include $(PVACCESS_SRC)/remote/Makefile
include $(PVACCESS_SRC)/remoteClient/Makefile
include $(PVACCESS_SRC)/server/Makefile
include $(PVACCESS_SRC)/rpcService/Makefile
include $(PVACCESS_SRC)/rpcClient/Makefile
include $(PVACCESS_SRC)/pipelineService/Makefile
include $(PVACCESS_SRC)/ca/Makefile
include $(PVACCESS_SRC)/mb/Makefile
include $(PVACCESS_SRC)/ioc/Makefile

LIBRARY = pvAccess

SHRLIB_VERSION = $(EPICS_PVA_MAJOR_VERSION).$(EPICS_PVA_MINOR_VERSION).$(EPICS_PVA_MAINTENANCE_VERSION)

pvAccess_LIBS += pvData
pvAccess_LIBS += ca
pvAccess_LIBS += Com

# needed for Windows
pvAccess_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES

# Can't use EXPAND as generated headers must appear
# in O.Common, but EXPAND emits rules for O.$(T_A)
../O.Common/pvaVersionNum.h: ../pva/pvaVersionNum.h@
	$(EXPAND_TOOL) $(EXPANDFLAGS) $($@_EXPANDFLAGS) $< $@
