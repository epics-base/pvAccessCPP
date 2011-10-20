TOP = ..
include $(TOP)/configure/CONFIG

PVACCESS = $(TOP)/pvAccessApp/


SRC_DIRS += $(PVACCESS)/ca
INC += caConstants.h
INC += version.h
INC += clientFactory.h
LIBSRCS += version.cpp
LIBSRCS += clientFactory.cpp


SRC_DIRS += $(PVACCESS)/utils
INC += hexDump.h
INC += inetAddressUtil.h
INC += logger.h
INC += introspectionRegistry.h
INC += transportRegistry.h
INC += namedLockPattern.h
INC += referenceCountingLock.h
INC += configuration.h
LIBSRCS += hexDump.cpp
LIBSRCS += inetAddressUtil.cpp
LIBSRCS += logger.cpp
LIBSRCS += introspectionRegistry.cpp
LIBSRCS += transportRegistry.cpp
LIBSRCS += configuration.cpp
LIBSRCS += referenceCountingLock.cpp

SRC_DIRS += $(PVACCESS)/client
INC += pvAccess.h
LIBSRCS += pvAccess.cpp

SRC_DIRS += $(PVACCESS)/server
INC += serverContext.h
INC += responseHandlers.h
INC += serverChannelImpl.h
INC += baseChannelRequester.h
INC += beaconEmitter.h
INC += beaconServerStatusProvider.h
LIBSRCS += responseHandlers.cpp
LIBSRCS += serverContext.cpp
LIBSRCS += serverChannelImpl.cpp
LIBSRCS += baseChannelRequester.cpp
LIBSRCS += beaconEmitter.cpp
LIBSRCS += beaconServerStatusProvider.cpp

SRC_DIRS += $(PVACCESS)/factory
LIBSRCS += ChannelAccessFactory.cpp
LIBSRCS += CreateRequestFactory.cpp


SRC_DIRS += $(PVACCESS)/remote
INC += remote.h
INC += blockingUDP.h
INC += beaconHandler.h
INC += blockingTCP.h
INC += channelSearchManager.h
LIBSRCS += blockingUDPTransport.cpp
LIBSRCS += blockingUDPConnector.cpp
LIBSRCS += beaconHandler.cpp
LIBSRCS += blockingTCPTransport.cpp
LIBSRCS += blockingClientTCPTransport.cpp
LIBSRCS += blockingTCPConnector.cpp
LIBSRCS += blockingServerTCPTransport.cpp
LIBSRCS += channelSearchManager.cpp
LIBSRCS += abstractResponseHandler.cpp
LIBSRCS += blockingTCPAcceptor.cpp


SRC_DIRS += $(PVACCESS)/remoteClient
INC += clientContextImpl.h
LIBSRCS += clientContextImpl.cpp


LIBRARY = pvAccess
pvAccess_LIBS += Com
pvAccess_LIBS += pvData

include $(TOP)/configure/RULES
