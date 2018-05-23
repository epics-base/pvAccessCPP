#ifndef CHANNELACCESSIFTEST_HPP
#define CHANNELACCESSIFTEST_HPP

#include <pv/pvAccess.h>
#include "syncTestRequesters.h"


class ChannelAccessIFTest {

    ChannelProvider::shared_pointer m_provider;
public:

    int runAllTest();

    virtual ~ChannelAccessIFTest() {}

protected:

    static const std::string TEST_COUNTER_CHANNEL_NAME;
    static const std::string TEST_SIMPLECOUNTER_CHANNEL_NAME;
    static const std::string TEST_CHANNEL_NAME;
    static const std::string TEST_VALUEONLY_CHANNEL_NAME;
    static const std::string TEST_SUMRPC_CHANNEL_NAME;
    static const std::string TEST_ARRAY_CHANNEL_NAME;


    ChannelProvider::shared_pointer getChannelProvider() { return m_provider; }
    long getTimeoutSec() {return 3;}
    bool isLocal() {return false;}


    Channel::shared_pointer createChannel(std::string channelName, bool debug = false );


    Channel::shared_pointer syncCreateChannel(std::string channelName, bool debug = false );


    SyncChannelGetRequesterImpl::shared_pointer syncCreateChannelGet(
        Channel::shared_pointer const & channel, std::string const & request, bool debug = false );


    SyncChannelPutRequesterImpl::shared_pointer syncCreateChannelPut(
        Channel::shared_pointer const & channel, std::string const & request, bool debug = false );


    SyncChannelPutGetRequesterImpl::shared_pointer syncCreateChannelPutGet(
        Channel::shared_pointer const & channel, std::string const & request, bool debug = false );


    SyncChannelRPCRequesterImpl::shared_pointer syncCreateChannelRPC(
        Channel::shared_pointer const & channel, bool debug = false);


    SyncMonitorRequesterImpl::shared_pointer syncCreateChannelMonitor(
        Channel::shared_pointer const & channel, std::string const & request, bool debug = false);

    SyncChannelArrayRequesterImpl::shared_pointer syncCreateChannelArray(
        Channel::shared_pointer const & channel, PVStructure::shared_pointer pvRequest, bool debug = false);


private:

    void test_implementation();

    void test_providerName();

    void test_createEmptyChannel();

    void test_createChannelWithInvalidPriority();

    void test_createChannel();

    void test_recreateChannelOnDestroyedProvider();

    void test_findEmptyChannel();

    void test_findChannel();

    void test_channel();

    void test_channelGetWithInvalidChannelAndRequester();

    void test_channelGetNoProcess();

    void test_channelGetIntProcess();

    void test_channelGetNotYetConnected();

    void test_channelGetIntProcessInternal(Channel::shared_pointer channel, std::string const & testMethodName);

    void test_channelGetTestNoConnection();

    void test_channelPutWithInvalidChannelAndRequester();

    void test_channelPutNoProcess();

    void test_channelPutIntProcess();

    void test_channelPutNotYetConnected();

    void test_channelPutIntProcessInternal(Channel::shared_pointer channel, std::string const & testMethodName);

    void test_channelPutTestNoConnection();

    void test_channelGetFieldAll();

    void test_channelGetFieldValue();

    void test_channelGetFieldInvalid();

    void test_channelProcess();

    void test_channelProcesstWithInvalidRequesterAndRequest();

    void test_channelProcessNoConnection();

    void test_channelPutGetWithInvalidRequesterAndRequest();

    void test_channelPutGetNoProcess_putGet();

    void test_channelPutGetNoProcess_getPut();

    void test_channelPutGetNoProcess_getGet();

    void test_channelPutGetNoProcess_destroy();

    void test_channelPutGetIntProcess();

    void test_channelRPC();

    void test_channelRPC_destroy();

    void test_channelRPCWithInvalidRequesterAndRequest();

    void test_channelMonitorWithInvalidRequesterAndRequest();

    void test_channelMonitor(int queueSize);

    void test_channelArray();

    void test_channelArray_destroy();

    void test_channelArrayTestNoConnection();

    void test_stressPutAndGetLargeArray();

    void test_stressConnectDisconnect();

    void test_stressConnectGetDisconnect();

    void test_stressMonitorAndProcess();

    PVStructure::shared_pointer createSumArgumentStructure(int a, int b);

    PVStructure::shared_pointer createArrayPvRequest();
};

#endif
