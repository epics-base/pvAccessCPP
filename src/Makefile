# Makefile for the pvAccess library

TOP = ..
include $(TOP)/configure/CONFIG

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
include $(PVACCESS_SRC)/ca/Makefile
include $(PVACCESS_SRC)/mb/Makefile
include $(PVACCESS_SRC)/ioc/Makefile

LIBRARY = pvAccess

pvAccess_LIBS += pvData
pvAccess_LIBS += ca
pvAccess_LIBS += Com

# needed for Windows
pvAccess_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES

