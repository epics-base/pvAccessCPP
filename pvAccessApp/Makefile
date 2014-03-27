TOP = ..
include $(TOP)/configure/CONFIG

PVACCESSAPP = $(TOP)/pvAccessApp

include $(PVACCESSAPP)/pva/Makefile
include $(PVACCESSAPP)/utils/Makefile
include $(PVACCESSAPP)/client/Makefile
include $(PVACCESSAPP)/factory/Makefile
include $(PVACCESSAPP)/remote/Makefile
include $(PVACCESSAPP)/remoteClient/Makefile
include $(PVACCESSAPP)/server/Makefile
include $(PVACCESSAPP)/rpcService/Makefile
include $(PVACCESSAPP)/rpcClient/Makefile
include $(PVACCESSAPP)/ca/Makefile
include $(PVACCESSAPP)/mb/Makefile
include $(PVACCESSAPP)/v3ioc/Makefile

LIBRARY = pvAccess

pvAccess_LIBS += pvData
pvAccess_LIBS += ca
pvAccess_LIBS += Com

# needed for Windows
pvAccess_SYS_LIBS_WIN32 += ws2_32

USR_CXXFLAGS_RTEMS = -D__rtems__

include $(TOP)/configure/RULES
