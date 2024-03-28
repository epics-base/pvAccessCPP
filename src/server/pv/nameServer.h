#ifndef NAME_SERVER_H
#define NAME_SERVER_H

#include <map>
#include <list>

#include <pv/pvData.h>
#include <pv/channelDiscovery.h>
#include <pv/responseHandlers.h>
#include <pv/serverContextImpl.h>

#include <shareLib.h>

namespace epics { namespace pvAccess {

class NameServerChannelFindRequesterImpl
    : public ChannelFindRequester
    , public TransportSender
    , public epics::pvData::TimerCallback
    , public std::tr1::enable_shared_from_this<NameServerChannelFindRequesterImpl>
{
public:
    NameServerChannelFindRequesterImpl(const ServerContextImpl::shared_pointer& context, const PeerInfo::const_shared_pointer& peerInfo, epics::pvData::int32 expectedResponseCount);
    virtual ~NameServerChannelFindRequesterImpl();
    void clear();
    void set(const std::string& channelName, epics::pvData::int32 searchSequenceId, epics::pvData::int32 cid, const osiSockAddr& sendTo, const Transport::shared_pointer& transport, bool responseRequired, bool serverSearch);
    virtual void channelFindResult(const epics::pvData::Status& status, const ChannelFind::shared_pointer& channelFind, bool wasFound) OVERRIDE FINAL;
    virtual std::tr1::shared_ptr<const PeerInfo> getPeerInfo() OVERRIDE FINAL;
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
    virtual void callback() OVERRIDE FINAL;
    virtual void timerStopped() OVERRIDE FINAL;

private:
    mutable epics::pvData::Mutex mutex;
    ServerGUID nameServerGuid;
    std::string nameServerAddress;
    std::string channelName;
    std::string channelServerAddress;
    epics::pvData::int32 searchSequenceId;
    epics::pvData::int32 cid;
    osiSockAddr sendTo;
    Transport::shared_pointer transport;
    bool responseRequired;
    bool channelWasFound;
    const ServerContextImpl::shared_pointer context;
    const PeerInfo::const_shared_pointer peer;
    const epics::pvData::int32 expectedResponseCount;
    epics::pvData::int32 responseCount;
    bool serverSearch;
};

class NameServerSearchHandler
    : public AbstractServerResponseHandler
{
public:
    static const std::string SUPPORTED_PROTOCOL;

    NameServerSearchHandler(const ServerContextImpl::shared_pointer& context);
    virtual ~NameServerSearchHandler();

    virtual void handleResponse(osiSockAddr* responseFrom, const Transport::shared_pointer& transport, epics::pvData::int8 version, epics::pvData::int8 command, std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class NameServerSearchResponseHandler
    : public ResponseHandler
{
public:
    NameServerSearchResponseHandler(const ServerContextImpl::shared_pointer& context);
    virtual ~NameServerSearchResponseHandler();
    virtual void handleResponse(osiSockAddr* responseFrom, const Transport::shared_pointer& transport, epics::pvData::int8 version, epics::pvData::int8 command, std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;

private:
    ServerBadResponse badResponseHandler;
    ServerNoopResponse beaconHandler;
    ServerConnectionValidationHandler validationHandler;
    ServerEchoHandler echoHandler;
    NameServerSearchHandler searchHandler;
    AuthNZHandler authnzHandler;
    ServerCreateChannelHandler createChannelHandler;
    ServerDestroyChannelHandler destroyChannelHandler;
    ServerRPCHandler rpcHandler;

    // Table of response handlers for each command ID.
    std::vector<ResponseHandler*> handlerTable;

};

class NameServerChannelFind
    : public ChannelFind
{
public:
    POINTER_DEFINITIONS(NameServerChannelFind);

    NameServerChannelFind(ChannelProvider::shared_pointer& provider);
    virtual ~NameServerChannelFind();
    virtual void destroy();
    virtual ChannelProvider::shared_pointer getChannelProvider();
    virtual void cancel();

private:
    ChannelProvider::weak_pointer channelProvider;
};

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
    virtual Channel::shared_pointer createChannel(const std::string& channelName, const ChannelRequester::shared_pointer& channelRequester, short /*priority*/, const std::string& address);
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
    ServerContextImpl::shared_pointer context;
    ServerGUID nameServerGuid;
    NameServerChannelProvider::shared_pointer channelProvider;
    double pollPeriod;
    double timeout;
    bool autoDiscovery;
    std::string serverAddresses;
};

}}

#endif // NAME_SERVER_H
