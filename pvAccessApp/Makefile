TOP = ..
include $(TOP)/configure/CONFIG

PVACCESS = $(TOP)/pvAccessApp/


SRC_DIRS += $(PVACCESS)/pva
INC += pvaConstants.h
INC += pvaVersion.h
INC += clientFactory.h
LIBSRCS += pvaVersion.cpp
LIBSRCS += clientFactory.cpp


SRC_DIRS += $(PVACCESS)/utils
INC += hexDump.h
INC += inetAddressUtil.h
INC += logger.h
INC += introspectionRegistry.h
INC += namedLockPattern.h
INC += referenceCountingLock.h
INC += configuration.h
INC += likely.h
LIBSRCS += hexDump.cpp
LIBSRCS += inetAddressUtil.cpp
LIBSRCS += logger.cpp
LIBSRCS += introspectionRegistry.cpp
LIBSRCS += configuration.cpp
LIBSRCS += referenceCountingLock.cpp

SRC_DIRS += $(PVACCESS)/client
INC += pvAccess.h
LIBSRCS += pvAccess.cpp

SRC_DIRS += $(PVACCESS)/factory
LIBSRCS += ChannelAccessFactory.cpp
LIBSRCS += CreateRequestFactory.cpp

SRC_DIRS += $(PVACCESS)/remote
INC += remote.h
INC += blockingUDP.h
INC += beaconHandler.h
INC += blockingTCP.h
INC += channelSearchManager.h
INC += simpleChannelSearchManagerImpl.h
INC += transportRegistry.h
INC += serializationHelper.h
LIBSRCS += blockingUDPTransport.cpp
LIBSRCS += blockingUDPConnector.cpp
LIBSRCS += beaconHandler.cpp
LIBSRCS += blockingTCPTransport.cpp
LIBSRCS += blockingClientTCPTransport.cpp
LIBSRCS += blockingTCPConnector.cpp
LIBSRCS += blockingServerTCPTransport.cpp
LIBSRCS += simpleChannelSearchManagerImpl.cpp
LIBSRCS += abstractResponseHandler.cpp
LIBSRCS += blockingTCPAcceptor.cpp
LIBSRCS += transportRegistry.cpp
LIBSRCS += serializationHelper.cpp

SRC_DIRS += $(PVACCESS)/remoteClient
INC += clientContextImpl.h
LIBSRCS += clientContextImpl.cpp

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

SRC_DIRS += $(PVACCESS)/rpcService
INC += rpcService.h
INC += rpcServer.h
LIBSRCS += rpcServer.cpp

SRC_DIRS += $(PVACCESS)/rpcClient
INC += rpcClient.h
LIBSRCS += rpcClient.cpp

SRC_DIRS += $(PVACCESS)/ca
INC += caProvider.h
INC += caChannel.h
LIBSRCS += caProvider.cpp
LIBSRCS += caChannel.cpp

SRC_DIRS += $(PVACCESS)/mb
INC += pvAccessMB.h
LIBSRCS += pvAccessMB.cpp

LIBRARY = pvAccess
pvAccess_LIBS += Com
pvAccess_LIBS += pvData
pvAccess_LIBS += ca

include $(TOP)/configure/RULES
