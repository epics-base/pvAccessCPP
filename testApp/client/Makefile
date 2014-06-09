TOP=../..

include $(TOP)/configure/CONFIG

#PROD_HOST += testChannelAccessFactory
testChannelAccessFactory_SRCS = testChannelAccessFactory.cpp
testChannelAccessFactory_LIBS = pvAccess pvData Com

#PROD_HOST += testMockClient
testMockClient_SRCS = testMockClient.cpp MockClientImpl.cpp
testMockClient_LIBS = pvAccess pvData Com

PROD_HOST += testStartStop
testStartStop_SRCS = testStartStop.cpp
testStartStop_LIBS = pvAccess pvData pvMB Com

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

