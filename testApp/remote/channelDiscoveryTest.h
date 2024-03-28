#ifndef CHANNEL_DISCOVERY_H
#define CHANNEL_DISCOVERY_H

#include <pv/pvAccess.h>
#include "syncTestRequesters.h"

class ChannelDiscoveryTest {

public:

    int getNumberOfTests();
    int runAllTests();
    virtual ~ChannelDiscoveryTest() {}

protected:

    static const std::string TEST_SIMPLECOUNTER_CHANNEL_NAME;

    ChannelProvider::shared_pointer getChannelProvider() { return m_provider; }
    long getTimeoutSec() {return 5;}
    bool isLocal() {return false;}

    Channel::shared_pointer createChannel(std::string channelName, bool debug=false);
    Channel::shared_pointer syncCreateChannel(std::string channelName, bool debug=false);
    SyncChannelGetRequesterImpl::shared_pointer syncCreateChannelGet(Channel::shared_pointer const & channel, std::string const & request, bool debug=false);

private:
    ChannelProvider::shared_pointer m_provider;

    void test_channelGetInt(Channel::shared_pointer channel, std::string const & testMethodName);
    void test_channelDiscovery();
};

#endif
