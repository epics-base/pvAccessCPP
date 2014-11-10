# This is a Makefile fragment, see ../Makefile

SRC_DIRS += $(PVACCESS_TEST)/remote

TESTPROD_HOST += testChannelAccess
testChannelAccess_SRCS = testChannelAccess channelAccessIFTest
testHarness_SRCS += testChannelAccess.cpp channelAccessIFTest.cpp
TESTS += testChannelAccess

TESTPROD_HOST += testCodec
testCodec_SRCS = testCodec
testHarness_SRCS += testCodec.cpp
TESTS += testCodec


TESTPROD_HOST += testRemoteClientImpl
testRemoteClientImpl_SRCS += testRemoteClientImpl.cpp

TESTPROD_HOST += testChannelConnect
testChannelConnect_SRCS += testChannelConnect.cpp

TESTPROD_HOST += testServerContext
testServerContext_SRCS += testServerContext.cpp



PROD_HOST += testServer
testServer_SRCS += testServer.cpp

PROD_HOST += testGetPerformance
testGetPerformance_SRCS += testGetPerformance.cpp

PROD_HOST += testMonitorPerformance
testMonitorPerformance_SRCS += testMonitorPerformance.cpp

PROD_HOST += rpcServiceExample
rpcServiceExample_SRCS += rpcServiceExample.cpp

PROD_HOST += rpcWildServiceExample
rpcWildServiceExample_SRCS += rpcWildServiceExample.cpp

PROD_HOST += rpcClientExample
rpcClientExample_SRCS += rpcClientExample.cpp
