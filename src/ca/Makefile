TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessCA
pvAccessCA_LIBS += ca pvAccess pvData
ifdef WITH_MICROBENCH
  LIB_LIBS += pvMB
endif
LIB_LIBS += Com

# needed for Windows
LIB_SYS_LIBS_WIN32 += ws2_32

INC += pv/caProvider.h
INC += pv/caStatus.h

pvAccessCA_SRCS += caProvider.cpp
pvAccessCA_SRCS += caChannel.cpp
pvAccessCA_SRCS += caStatus.cpp

include $(TOP)/configure/RULES
