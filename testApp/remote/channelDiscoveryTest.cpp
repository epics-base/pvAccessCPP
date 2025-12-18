// disable buggy boost enable_shared_from_this assert code
#define BOOST_DISABLE_ASSERTS

#include <stdlib.h>

#include <epicsExit.h>
#include <epicsUnitTest.h>

#include <pv/logger.h>
#include <pv/current_function.h>

#include "channelDiscoveryTest.h"

namespace TR1 = std::tr1;
namespace EPVA = epics::pvAccess;

// int value, increment on process
const std::string ChannelDiscoveryTest::TEST_SIMPLECOUNTER_CHANNEL_NAME = "testSimpleCounter";

int ChannelDiscoveryTest::getNumberOfTests()
{
    return 1;
}

int ChannelDiscoveryTest::runAllTests()
{
    testDiag("Starting channel discovery tests");
    m_provider = ChannelProviderRegistry::clients()->getProvider("pva");
    test_channelDiscovery();
    return testDone();
}

Channel::shared_pointer ChannelDiscoveryTest::createChannel(string channelName, bool debug )
{
    TR1::shared_ptr<SyncChannelRequesterImpl> channelReq(new SyncChannelRequesterImpl(debug));
    Channel::shared_pointer channel = getChannelProvider()->createChannel(channelName, channelReq);
    return channel;
}

Channel::shared_pointer ChannelDiscoveryTest::syncCreateChannel(string channelName, bool debug )
{

    TR1::shared_ptr<SyncChannelRequesterImpl> channelReq(new SyncChannelRequesterImpl(debug));
    Channel::shared_pointer channel = getChannelProvider()->createChannel(channelName, channelReq);
    bool isConnected = channelReq->waitUntilConnected(getTimeoutSec());
    if (!isConnected) {
        std::cerr << "[" << channelName << "] failed to connect to the channel. " << std::endl;
        return TR1::shared_ptr<Channel>();
    }

    return channel;
}


SyncChannelGetRequesterImpl::shared_pointer ChannelDiscoveryTest::syncCreateChannelGet(
    Channel::shared_pointer const & channel, string const & request, bool debug )
{

    TR1::shared_ptr<SyncChannelGetRequesterImpl>
    channelGetReq(new SyncChannelGetRequesterImpl(channel->getChannelName(), debug));

    PVStructure::shared_pointer pvRequest = createRequest(request);

    ChannelGet::shared_pointer op(channel->createChannelGet(channelGetReq,pvRequest));
    bool succStatus = channelGetReq->waitUntilGetDone(getTimeoutSec());
    if (!succStatus) {
        std::cerr << "[" << channel->getChannelName() << "] failed to get. " << std::endl;
        return TR1::shared_ptr<SyncChannelGetRequesterImpl>();
    }
    return channelGetReq;
}

void ChannelDiscoveryTest::test_channelGetInt(Channel::shared_pointer channel, string const & testMethodName)
{

    string request = "record[process=true]field(value)";

    SyncChannelGetRequesterImpl::shared_pointer channelGetReq = syncCreateChannelGet(channel, request);
    if (!channelGetReq.get()) {
        testFail("%s: channel get not created ", testMethodName.c_str());
        return;
    }

    TR1::shared_ptr<PVInt> value = channelGetReq->getPVStructure()->getSubField<PVInt>("value");
    int previousValue = value->get();
    epicsThreadSleep(1.0);
    bool succStatus = channelGetReq->syncGet(false, getTimeoutSec());
    if (!succStatus) {
        testFail("%s: sync get failed ", testMethodName.c_str());
        return;
    }
    testOk((previousValue +1) == value->get(), "%s: testing the counter value change %d == %d", testMethodName.c_str(), previousValue +1, (int)value->get());
    channelGetReq->getChannelGet()->destroy();
    channel->destroy();
}

void ChannelDiscoveryTest::test_channelDiscovery()
{
    testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
    Channel::shared_pointer channel = syncCreateChannel(TEST_SIMPLECOUNTER_CHANNEL_NAME);
    if (!channel.get()) {
        testFail("%s: channel not created ", CURRENT_FUNCTION);
        return;
    }
    test_channelGetInt(channel, CURRENT_FUNCTION);
}
