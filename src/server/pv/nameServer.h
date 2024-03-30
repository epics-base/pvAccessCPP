#ifndef NAME_SERVER_H
#define NAME_SERVER_H

#include <map>
#include <list>

#include <pv/pvData.h>
#include <pv/pvAccess.h>
#include <pv/pvaDefs.h>
#include <pv/channelDiscovery.h>
#include <pv/serverContext.h>

#include <shareLib.h>

namespace epics { namespace pvAccess {

class NameServerChannelProvider
    : public ChannelProvider
    , public std::tr1::enable_shared_from_this<NameServerChannelProvider>
{
public:
    POINTER_DEFINITIONS(NameServerChannelProvider);
    static const std::string PROVIDER_NAME;

    NameServerChannelProvider();
    virtual ~NameServerChannelProvider();
    void initialize();
    virtual std::string getProviderName();
    virtual void destroy();
    virtual ChannelFind::shared_pointer channelFind(const std::string& channelName, const ChannelFindRequester::shared_pointer& channelFindRequester);
    virtual ChannelFind::shared_pointer channelList(const ChannelListRequester::shared_pointer& channelListRequester);
    virtual Channel::shared_pointer createChannel(const std::string& channelName, const ChannelRequester::shared_pointer& channelRequester, short priority);
    virtual Channel::shared_pointer createChannel(const std::string& channelName, const ChannelRequester::shared_pointer& channelRequester, short priority, const std::string& address);
    void updateChannelMap(const ChannelDiscovery::ChannelMap& updatedChannelMap);
    std::string getChannelServerAddress(const std::string& channelName);
    void setChannelEntryExpirationTime(double expirationTime);
    void setStaticChannelEntries(const ChannelDiscovery::ChannelMap& channelMap);

private:
    ChannelFind::shared_pointer nsChannelFind;
    mutable epics::pvData::Mutex channelMapMutex;
    ChannelDiscovery::ChannelMap channelMap;
    double channelEntryExpirationTime;
};

class epicsShareClass NameServer
{
public:
    POINTER_DEFINITIONS(NameServer);
    NameServer(const epics::pvAccess::Configuration::shared_pointer& conf);
    virtual ~NameServer();

    virtual void setPollPeriod(double pollPeriod);
    virtual void setPvaTimeout(double timeout);
    virtual void setAutoDiscovery(bool autoDiscovery);
    virtual void setStaticServerAddresses(const std::string& serverAddresses);
    virtual void setStaticChannelEntries(const ChannelDiscovery::ChannelMap& channelMap);
    virtual void setChannelEntryExpirationTime(double expirationTime);
    virtual void run(double runtime);

    virtual bool addUniqueServerToList(const std::string& serverAddress, ChannelDiscovery::ServerAddressList& serverAddressList);
    virtual void addServersFromAddresses(ChannelDiscovery::ServerAddressList& serverAddressList);
    virtual void discoverChannels(ChannelDiscovery::ServerAddressList& serverAddressList, ChannelDiscovery::ChannelMap& channelMap);
    virtual void shutdown();

    virtual void discoverServers(ChannelDiscovery::ServerAddressList& serverAddressList) = 0;
    virtual void discoverServerChannels(const std::string& serverAddress, ChannelDiscovery::ChannelMap& channelMap) = 0;


protected:

    std::string inetAddrToString(const osiSockAddr& addr);

    ServerContext::shared_pointer context;
    ServerGUID nameServerGuid;
    NameServerChannelProvider::shared_pointer channelProvider;
    double pollPeriod;
    double timeout;
    bool autoDiscovery;
    std::string serverAddresses;
};

}}

#endif // NAME_SERVER_H
