TOP=../..

include $(TOP)/configure/CONFIG

#PROD_HOST += testChannelAccessFactory
testChannelAccessFactory_SRCS = testChannelAccessFactory.cpp
testChannelAccessFactory_LIBS = pvData pvAccess Com

PROD_HOST += testCreateRequest
testCreateRequest_SRCS = testCreateRequest.cpp
testCreateRequest_LIBS = pvData pvAccess pvMB Com

#PROD_HOST += testMockClient
testMockClient_SRCS = testMockClient.cpp MockClientImpl.cpp
testMockClient_LIBS = pvData pvAccess Com

PROD_HOST += testStartStop
testStartStop_SRCS = testStartStop.cpp
testStartStop_LIBS = pvData pvAccess pvMB Com

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

