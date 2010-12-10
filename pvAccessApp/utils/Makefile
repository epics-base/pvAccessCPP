TOP=../..

include $(TOP)/configure/CONFIG

INC += hexDump.h
INC += wildcharMatcher.h
INC += arrayFIFO.h
INC += growingCircularBuffer.h
INC += inetAddressUtil.h

LIBSRCS += hexDump.cpp
LIBSRCS += wildcharMatcher.cpp
LIBSRCS += inetAddressUtil.cpp

LIBRARY = pvAccessUtils
pvAccessUtils_LIBS += Com
pvAccessUtils_LIBS += pvData

pvData_DIR = $(PVDATA_HOME)/lib/$(EPICS_HOST_ARCH)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

