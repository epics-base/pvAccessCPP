TOP = ../..
include $(TOP)/configure/CONFIG

LIBRARY += pvAccessCA
pvAccessCA_LIBS += ca pvAccess pvData Com

INC += pv/caProvider.h

pvAccessCA_SRCS += caProvider.cpp
pvAccessCA_SRCS += caChannel.cpp
pvAccessCA_SRCS += caStatus.cpp

include $(TOP)/configure/RULES
