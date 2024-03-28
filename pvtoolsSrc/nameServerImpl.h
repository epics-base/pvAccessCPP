#ifndef NAME_SERVER_IMPL_H
#define NAME_SERVER_IMPL_H

#include <pv/nameServer.h>

namespace epics { namespace pvAccess {

class NameServerImpl
    : public NameServer
    , public std::tr1::enable_shared_from_this<NameServerImpl>
{
public:
    POINTER_DEFINITIONS(NameServerImpl);
    NameServerImpl(const epics::pvAccess::Configuration::shared_pointer& conf);
    virtual ~NameServerImpl();

    virtual void discoverServers(ChannelDiscovery::ServerAddressList& serverAddressList);
    virtual void discoverServerChannels(const std::string& serverAddress, ChannelDiscovery::ChannelMap& channelMap);
};

}}

#endif // NAME_SERVER_IMPL_H
