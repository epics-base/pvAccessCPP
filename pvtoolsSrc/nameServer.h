#ifndef NAME_SERVER_H
#define NAME_SERVER_H

#include <map>
#include <list>
#include <pv/pvData.h>
#include <pv/responseHandlers.h>
#include "pvutils.h"

namespace epics { namespace pvAccess {

/*
 * Name server channel entry
 */
struct ChannelEntry {
    std::string channelName;
    std::string serverAddress;
    epicsTimeStamp updateTime;
};
typedef std::map<std::string, ChannelEntry> ChannelMap;
typedef std::list<std::string> ServerAddressList;

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
    void set(std::string channelName, epics::pvData::int32 searchSequenceId, epics::pvData::int32 cid, osiSockAddr const & sendTo, const Transport::shared_pointer& transport, bool responseRequired, bool serverSearch);
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
    void updateChannelMap(const ChannelMap& updatedChannelMap);
    std::string getChannelServerAddress(const std::string& channelName);
    void setChannelEntryExpirationTime(double expirationTime);
    void setStaticChannelEntries(const ChannelMap& channelMap);

private:
    ChannelFind::shared_pointer nsChannelFind;
    mutable epics::pvData::Mutex channelMapMutex;
    ChannelMap channelMap;
    double channelEntryExpirationTime;
};

class NameServer
    : public std::tr1::enable_shared_from_this<NameServer>
{
public:
    POINTER_DEFINITIONS(NameServer);
    NameServer(const epics::pvAccess::Configuration::shared_pointer& conf);
    virtual ~NameServer();
    void setPollPeriod(double pollPeriod);
    void setPvaTimeout(double timeout);
    void setAutoDiscovery(bool autoDiscovery);
    void setStaticServerAddresses(const std::string& serverAddresses);
    void setStaticChannelEntries(const ChannelMap& channelMap);
    void setChannelEntryExpirationTime(double expirationTime);
    void run(double runtime);

private:
    ServerContextImpl::shared_pointer context;
    NameServerChannelProvider::shared_pointer channelProvider;
    double pollPeriod;
    double timeout;
    bool autoDiscovery;
    std::string serverAddresses;

    bool addUniqueServerToList(const std::string& serverAddress, ServerAddressList& serverAddressList);
    void addServersFromAddresses(ServerAddressList& serverAddressList);
    void discoverServers(ServerAddressList& serverAddressList);
    void discoverServerChannels(const std::string& serverAddress, ChannelMap& channelMap);
    void discoverChannels(ServerAddressList& serverAddressList, ChannelMap& channelMap);
    void shutdown();
};

}}

#endif // NAME_SERVER_H
