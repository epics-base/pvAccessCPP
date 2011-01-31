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
INC += introspectionRegistry.h
INC += transportRegistry.h
INC += namedLockPattern.h
INC += referenceCountingLock.h
INC += configuration.h
LIBSRCS += hexDump.cpp
LIBSRCS += wildcharMatcher.cpp
LIBSRCS += inetAddressUtil.cpp
LIBSRCS += logger.cpp
LIBSRCS += introspectionRegistry.cpp
LIBSRCS += transportRegistry.cpp
LIBSRCS += namedLockPattern.cpp
LIBSRCS += referenceCountingLock.cpp
LIBSRCS += configuration.cpp


SRC_DIRS += $(PVACCESS)/client
INC += pvAccess.h
LIBSRCS += pvAccess.cpp

SRC_DIRS += $(PVACCESS)/server
INC += serverContext.h
INC += responseHandlers.h
LIBSRCS += responseHandlers.cpp
LIBSRCS += serverContext.cpp


SRC_DIRS += $(PVACCESS)/factory
LIBSRCS += ChannelAccessFactory.cpp
LIBSRCS += CreateRequestFactory.cpp


SRC_DIRS += $(PVACCESS)/remote
INC += remote.h
INC += blockingUDP.h
INC += beaconEmitter.h
INC += beaconServerStatusProvider.h
INC += beaconHandler.h
INC += blockingTCP.h
INC += channelSearchManager.h
LIBSRCS += blockingUDPTransport.cpp
LIBSRCS += blockingUDPConnector.cpp
LIBSRCS += beaconEmitter.cpp
LIBSRCS += beaconServerStatusProvider.cpp
LIBSRCS += beaconHandler.cpp
LIBSRCS += blockingTCPTransport.cpp
LIBSRCS += blockingClientTCPTransport.cpp
LIBSRCS += blockingTCPConnector.cpp
LIBSRCS += blockingServerTCPTransport.cpp
LIBSRCS += blockingTCPAcceptor.cpp
LIBSRCS += channelSearchManager.cpp
LIBSRCS += abstractResponseHandler.cpp


SRC_DIRS += $(PVACCESS)/remoteClient
INC += clientContextImpl.h
LIBSRCS += clientContextImpl.cpp


LIBRARY = pvAccess
pvAccess_LIBS += Com

include $(TOP)/configure/RULES
