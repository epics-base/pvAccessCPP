TOP = ..
include $(TOP)/configure/CONFIG

PVACCESS = $(TOP)/pvAccessApp/


SRC_DIRS += $(PVACCESS)/ca
INC += caConstants.h
INC += version.h
LIBSRCS += version.cpp


SRC_DIRS += $(PVACCESS)/utils
INC += hexDump.h
INC += wildcharMatcher.h
INC += arrayFIFO.h
INC += growingCircularBuffer.h
INC += inetAddressUtil.h
INC += logger.h
LIBSRCS += hexDump.cpp
LIBSRCS += wildcharMatcher.cpp
LIBSRCS += inetAddressUtil.cpp
LIBSRCS += logger.cpp


SRC_DIRS += $(PVACCESS)/client
INC += pvAccess.h


SRC_DIRS += $(PVACCESS)/factory
LIBSRCS += ChannelAccessFactory.cpp




LIBRARY = pvAccess
pvAccess_LIBS += Com

include $(TOP)/configure/RULES
