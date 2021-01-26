TOP=..

include $(TOP)/configure/CONFIG

USR_CPPFLAGS += -I$(TOP)/src/utils
USR_CPPFLAGS += -I$(TOP)/src/remote

PROD_DEFAULT += pvget
pvget_SRCS += pvget.cpp
pvget_SRCS += pvutils.cpp

PROD_DEFAULT += pvmonitor
pvmonitor_SRCS += pvmonitor.cpp
pvmonitor_SRCS += pvutils.cpp

PROD_DEFAULT += pvput
pvput_SRCS += pvput.cpp
pvput_SRCS += pvutils.cpp

PROD_DEFAULT += pvcall
pvcall_SRCS += pvcall.cpp
pvcall_SRCS += pvutils.cpp

PROD_DEFAULT += pvinfo
pvinfo_SRCS += pvinfo.cpp
pvinfo_SRCS += pvutils.cpp

PROD_DEFAULT += pvlist
pvlist_SRCS += pvlist.cpp

PROD_LIBS += pvAccessCA pvAccess pvData ca Com

PROD_SYS_LIBS_WIN32 += netapi32 ws2_32

PROD_vxWorks = -nil-
PROD_RTEMS = -nil-
PROD_iOS = -nil-

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

