TOP=..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/src/utils
USR_CPPFLAGS += -I$(TOP)/src/remote

PROD_HOST += pvget
pvget_SRCS += pvget.cpp

PROD_HOST += pvput
pvput_SRCS += pvput.cpp

PROD_HOST += pvinfo
pvinfo_SRCS += pvinfo.cpp

PROD_HOST += pvlist
pvlist_SRCS += pvlist.cpp

PROD_HOST += eget
eget_SRCS += eget.cpp

PROD_LIBS += pvAccessCA pvAccess pvData ca Com

PROD_SYS_LIBS_WIN32 += ws2_32

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

