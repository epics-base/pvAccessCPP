TOP=..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/src/utils
USR_CPPFLAGS += -I$(TOP)/src/remote

PROD_HOST += pvget
pvget_SRCS += pvget.cpp
pvget_SRCS += pvutils.cpp

PROD_HOST += pvmonitor
pvmonitor_SRCS += pvmonitor.cpp
pvmonitor_SRCS += pvutils.cpp

PROD_HOST += pvput
pvput_SRCS += pvput.cpp
pvput_SRCS += pvutils.cpp

PROD_HOST += pvcall
pvcall_SRCS += pvcall.cpp
pvcall_SRCS += pvutils.cpp

PROD_HOST += pvinfo
pvinfo_SRCS += pvinfo.cpp
pvinfo_SRCS += pvutils.cpp

PROD_HOST += pvlist
pvlist_SRCS += pvlist.cpp

PROD_LIBS += pvAccessCA pvAccess pvData ca Com

PROD_SYS_LIBS_WIN32 += netapi32 ws2_32

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

