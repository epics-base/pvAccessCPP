TOP=../..

include $(TOP)/configure/CONFIG

#PROD_HOST += testBlockingUDPSrv
testBlockingUDPSrv_SRCS += testBlockingUDPSrv.cpp
testBlockingUDPSrv_LIBS += pvAccess pvData pvMB Com

#PROD_HOST += testBlockingUDPClnt
testBlockingUDPClnt_SRCS += testBlockingUDPClnt.cpp
testBlockingUDPClnt_LIBS += pvAccess pvData pvMB Com

PROD_HOST += testRemoteClientImpl
testRemoteClientImpl_SRCS += testRemoteClientImpl.cpp
testRemoteClientImpl_LIBS += pvAccess pvData pvMB Com

PROD_HOST += testChannelConnect
testChannelConnect_SRCS += testChannelConnect.cpp
testChannelConnect_LIBS += pvAccess pvData pvMB Com

#PROD_HOST += testBeaconEmitter
testBeaconEmitter_SRCS += testBeaconEmitter.cpp
testBeaconEmitter_LIBS += pvAccess pvData Com

#PROD_HOST += testBeaconHandler
testBeaconHandler_SRCS += testBeaconHandler.cpp
testBeaconHandler_LIBS += pvAccess pvData Com

#PROD_HOST += testChannelSearchManager
#testChannelSearchManager_SRCS += testChannelSearchManager.cpp
#testChannelSearchManager_LIBS += pvAccess pvData Com

#PROD_HOST += testBlockingTCPSrv
testBlockingTCPSrv_SRCS += testBlockingTCPSrv.cpp
testBlockingTCPSrv_LIBS += pvAccess pvData pvMB Com

#PROD_HOST += testBlockingTCPClnt
testBlockingTCPClnt_SRCS += testBlockingTCPClnt.cpp
testBlockingTCPClnt_LIBS += pvAccess pvData pvMB Com

PROD_HOST += testServerContext
testServerContext_SRCS += testServerContext.cpp
testServerContext_LIBS += pvAccess pvData pvMB Com

PROD_HOST += testServer
testServer_SRCS += testServer.cpp
testServer_LIBS += pvAccess pvData pvMB Com

TESTPROD_HOST += testChannelAccess
testChannelAccess_SRCS = testChannelAccess channelAccessIFTest
testChannelAccess_LIBS += pvAccess pvData pvMB Com
TESTS += testChannelAccess

TESTPROD_HOST += testCodec
testCodec_SRCS = testCodec
testCodec_LIBS += pvAccess pvData pvMB Com
TESTS += testCodec

PROD_HOST += testGetPerformance
testGetPerformance_SRCS += testGetPerformance.cpp
testGetPerformance_LIBS += pvAccess pvData pvMB Com

PROD_HOST += testMonitorPerformance
testMonitorPerformance_SRCS += testMonitorPerformance.cpp
testMonitorPerformance_LIBS += pvAccess pvData pvMB Com

PROD_HOST += rpcServiceExample
rpcServiceExample_SRCS += rpcServiceExample.cpp
rpcServiceExample_LIBS += pvAccess pvData pvMB Com

PROD_HOST += rpcWildServiceExample
rpcWildServiceExample_SRCS += rpcWildServiceExample.cpp
rpcWildServiceExample_LIBS += pvAccess pvData pvMB Com

PROD_HOST += rpcClientExample
rpcClientExample_SRCS += rpcClientExample.cpp
rpcClientExample_LIBS += pvAccess pvData pvMB Com

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

