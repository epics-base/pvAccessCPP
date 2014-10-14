TOP=..

include $(TOP)/configure/CONFIG

PROD_HOST += pvget
pvget_SRCS += pvget.cpp
pvget_LIBS += pvAccess pvData pvMB Com

PROD_HOST += pvput
pvput_SRCS += pvput.cpp
pvput_LIBS += pvAccess pvData pvMB Com

PROD_HOST += pvinfo
pvinfo_SRCS += pvinfo.cpp
pvinfo_LIBS += pvAccess pvData pvMB Com

PROD_HOST += pvlist
pvlist_SRCS += pvlist.cpp
pvlist_LIBS += pvAccess pvData pvMB Com
pvlist_SYS_LIBS_WIN32 += ws2_32

PROD_HOST += eget
eget_SRCS += eget.cpp
eget_LIBS += pvAccess pvData pvMB ca Com

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

