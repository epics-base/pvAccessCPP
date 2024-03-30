/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/hexDump.h>
#include <pv/logger.h>
#include <pv/sharedPtr.h>
#include <pv/security.h>
#include <pv/stringUtility.h>
#include <pv/remote.h>
#include "pv/responseHandlers.h"
#include "pv/serverContextImpl.h"
#include "pv/nameServer.h"

using namespace std;
using namespace epics::pvData;
using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;
using epics::pvAccess::ChannelDiscovery::ChannelEntry;
using epics::pvAccess::ChannelDiscovery::ChannelMap;
using epics::pvAccess::ChannelDiscovery::ServerAddressList;

namespace epics { namespace pvAccess {

/*
 * Name server channel find requester
 */
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

NameServerChannelFindRequesterImpl::NameServerChannelFindRequesterImpl(const ServerContextImpl::shared_pointer& context_, const PeerInfo::const_shared_pointer& peer_, int32 expectedResponseCount_)
    : mutex()
    , nameServerGuid(context_->getGUID())
    , nameServerAddress(inetAddressToString(*(context_->getServerInetAddress())))
    , sendTo()
    , responseRequired(false)
    , channelWasFound(false)
    , context(context_)
    , peer(peer_)
    , expectedResponseCount(expectedResponseCount_)
    , responseCount(0)
    , serverSearch(false)
{
}

NameServerChannelFindRequesterImpl::~NameServerChannelFindRequesterImpl()
{
}

void NameServerChannelFindRequesterImpl::clear()
{
    Lock lock(mutex);
    channelWasFound = false;
    responseCount = 0;
    serverSearch = false;
}

void NameServerChannelFindRequesterImpl::callback()
{
    channelFindResult(Status::Ok, ChannelFind::shared_pointer(), false);
}

void NameServerChannelFindRequesterImpl::timerStopped()
{
}

void NameServerChannelFindRequesterImpl::set(const std::string& channelName, int32 searchSequenceId, int32 cid, const osiSockAddr& sendTo, const Transport::shared_pointer& transport, bool responseRequired, bool serverSearch)
{
    Lock lock(mutex);
    this->channelName = channelName;
    this->searchSequenceId = searchSequenceId;
    this->cid = cid;
    this->sendTo = sendTo;
    this->transport = transport;
    this->responseRequired = responseRequired;
    this->serverSearch = serverSearch;
}

void NameServerChannelFindRequesterImpl::channelFindResult(const Status& /*status*/, const ChannelFind::shared_pointer& channelFind, bool wasFound)
{
    Lock lock(mutex);
    responseCount++;
    if (responseCount > expectedResponseCount) {
        if ((responseCount+1) == expectedResponseCount) {
            LOG(logLevelDebug,"[NameServerChannelFindRequesterImpl::channelFindResult] More responses received than expected for channel '%s'!", channelName.c_str());
        }
        return;
    }

    if (wasFound && channelWasFound) {
        LOG(logLevelDebug,"[NameServerChannelFindRequesterImpl::channelFindResult] Channel '%s' is hosted by different channel providers!", channelName.c_str());
        return;
    }

    if (wasFound || (responseRequired && (responseCount == expectedResponseCount))) {
        if (wasFound && expectedResponseCount > 1) {
            Lock L(context->_mutex);
            context->s_channelNameToProvider[channelName] = channelFind->getChannelProvider();
        }
        channelWasFound = wasFound;
        if (channelFind && ! channelName.empty()) {
            ChannelProvider::shared_pointer channelProvider = channelFind->getChannelProvider();
            NameServerChannelProvider::shared_pointer nsChannelProvider = dynamic_pointer_cast<NameServerChannelProvider>(channelProvider);
            if (nsChannelProvider) {
                channelServerAddress = nsChannelProvider->getChannelServerAddress(channelName);
            }
        }
        if (transport && transport->getType() == "tcp") {
            TransportSender::shared_pointer thisSender = shared_from_this();
            transport->enqueueSendRequest(thisSender);
        }
        else {
            BlockingUDPTransport::shared_pointer bt = context->getBroadcastTransport();
            if (bt) {
                TransportSender::shared_pointer thisSender = shared_from_this();
                bt->enqueueSendRequest(thisSender);
            }
        }
    }
}

std::tr1::shared_ptr<const PeerInfo> NameServerChannelFindRequesterImpl::getPeerInfo()
{
    return peer;
}

void NameServerChannelFindRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
    std::string sendToStr = inetAddressToString(sendTo);
    LOG(logLevelDebug, "Name server search response will be sent to %s", sendToStr.c_str());
    control->startMessage(CMD_SEARCH_RESPONSE, 12+4+16+2);

    Lock lock(mutex);
    buffer->put(nameServerGuid.value, 0, sizeof(nameServerGuid.value));
    buffer->putInt(searchSequenceId);

    int nameServerPort = context->getServerPort();
    osiSockAddr channelServerAddr;
    channelServerAddr.ia.sin_port = htons(nameServerPort);
    if (stringToInetAddress(channelServerAddress, channelServerAddr)) {
        LOG(logLevelDebug, "Encoding channel server address %s into channel search response", channelServerAddress.c_str());
    }
    else {
        stringToInetAddress(nameServerAddress, channelServerAddr);
        LOG(logLevelDebug, "Encoding name server address %s into channel search response", nameServerAddress.c_str());
    }
    encodeAsIPv6Address(buffer, &channelServerAddr);
    int16 port = ntohs(channelServerAddr.ia.sin_port);
    buffer->putShort(port);

    SerializeHelper::serializeString(ServerSearchHandler::SUPPORTED_PROTOCOL, buffer, control);

    control->ensureBuffer(1);
    buffer->putByte(channelWasFound ? (int8)1 : (int8)0);

    if (!serverSearch) {
        // For now we do not gather search responses
        buffer->putShort((int16)1);
        buffer->putInt(cid);
    }
    else {
        buffer->putShort((int16)0);
    }

    control->setRecipient(sendTo);

    // send immediately
    control->flush(true);
}

/*
 * Name server search handler
 */
class NameServerSearchHandler
    : public AbstractServerResponseHandler
{
public:
    static const std::string SUPPORTED_PROTOCOL;

    NameServerSearchHandler(const ServerContextImpl::shared_pointer& context);
    virtual ~NameServerSearchHandler();

    virtual void handleResponse(osiSockAddr* responseFrom, const Transport::shared_pointer& transport, epics::pvData::int8 version, epics::pvData::int8 command, std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

const std::string NameServerSearchHandler::SUPPORTED_PROTOCOL = "tcp";

NameServerSearchHandler::NameServerSearchHandler(const ServerContextImpl::shared_pointer& context)
    : AbstractServerResponseHandler(context, "Search request")
{
    // initialize random seed
    srand(time(NULL));
}

NameServerSearchHandler::~NameServerSearchHandler()
{
}

void NameServerSearchHandler::handleResponse(osiSockAddr* responseFrom, const Transport::shared_pointer& transport, int8 version, int8 command, size_t payloadSize, ByteBuffer* payloadBuffer)
{
    std::string responseFromStr = inetAddressToString(*responseFrom);
    LOG(logLevelDebug, "Name server search handler is handling request from %s", responseFromStr.c_str());
    AbstractServerResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);
    transport->ensureData(4+1+3+16+2);

    size_t startPosition = payloadBuffer->getPosition();

    const int32 searchSequenceId = payloadBuffer->getInt();
    const int8 qosCode = payloadBuffer->getByte();

    // reserved part
    payloadBuffer->getByte();
    payloadBuffer->getShort();

    osiSockAddr responseAddress;
    memset(&responseAddress, 0, sizeof(responseAddress));
    responseAddress.ia.sin_family = AF_INET;

    // 128-bit IPv6 address
    if (!decodeAsIPv6Address(payloadBuffer, &responseAddress)) {
        return;
    }

    // accept given address if explicitly specified by sender
    if (responseAddress.ia.sin_addr.s_addr == INADDR_ANY) {
        responseAddress.ia.sin_addr = responseFrom->ia.sin_addr;
    }

    int16 port = payloadBuffer->getShort();
    if (port) {
        responseAddress.ia.sin_port = htons(port);
    }
    else {
        LOG(logLevelDebug, "Server search handler is reusing connection port %d", ntohs(responseFrom->ia.sin_port));
        responseAddress.ia.sin_port = responseFrom->ia.sin_port;
    }

    size_t protocolsCount = SerializeHelper::readSize(payloadBuffer, transport.get());
    bool allowed = (protocolsCount == 0);
    for (size_t i = 0; i < protocolsCount; i++) {
        string protocol = SerializeHelper::deserializeString(payloadBuffer, transport.get());
        if (SUPPORTED_PROTOCOL == protocol) {
            allowed = true;
        }
    }

    transport->ensureData(2);
    const int32 count = payloadBuffer->getShort() & 0xFFFF;
    const bool responseRequired = (QOS_REPLY_REQUIRED & qosCode) != 0;

    //
    // locally broadcast if unicast (qosCode & QOS_GET_PUT == QOS_GET_PUT) via UDP
    //
    if ((qosCode & QOS_GET_PUT) == QOS_GET_PUT) {
        BlockingUDPTransport::shared_pointer bt = dynamic_pointer_cast<BlockingUDPTransport>(transport);
        if (bt && bt->hasLocalMulticastAddress()) {
            // RECEIVE_BUFFER_PRE_RESERVE allows to pre-fix message
            size_t newStartPos = (startPosition-PVA_MESSAGE_HEADER_SIZE)-PVA_MESSAGE_HEADER_SIZE-16;
            payloadBuffer->setPosition(newStartPos);

            // copy part of a header, and add: command, payloadSize, NIF address
            payloadBuffer->put(payloadBuffer->getBuffer(), startPosition-PVA_MESSAGE_HEADER_SIZE, PVA_MESSAGE_HEADER_SIZE-5);
            payloadBuffer->putByte(CMD_ORIGIN_TAG);
            payloadBuffer->putInt(16);
            // encode this socket bind address
            encodeAsIPv6Address(payloadBuffer, bt->getBindAddress());

            // clear unicast flag
            payloadBuffer->put(startPosition+4, (int8)(qosCode & ~0x80));

            // update response address
            payloadBuffer->setPosition(startPosition+8);
            encodeAsIPv6Address(payloadBuffer, &responseAddress);

            // set to end of a message
            payloadBuffer->setPosition(payloadBuffer->getLimit());

            bt->send(payloadBuffer->getBuffer()+newStartPos, payloadBuffer->getPosition()-newStartPos, bt->getLocalMulticastAddress());
            return;
        }
    }

    PeerInfo::shared_pointer info;
    if(allowed) {
        info.reset(new PeerInfo);
        info->transport = "pva";
        info->peer = responseFromStr;
        info->transportVersion = version;
    }

    if (count > 0) {
        // regular name search
        for (int32 i = 0; i < count; i++) {
            transport->ensureData(4);
            const int32 cid = payloadBuffer->getInt();
            const string name = SerializeHelper::deserializeString(payloadBuffer, transport.get());
            LOG(logLevelDebug, "Search for channel %s, cid %d", name.c_str(), cid);
            if (allowed) {
                const std::vector<ChannelProvider::shared_pointer>& providers = _context->getChannelProviders();
                unsigned int providerCount = providers.size();
                std::tr1::shared_ptr<NameServerChannelFindRequesterImpl> channelFindRequester(new NameServerChannelFindRequesterImpl(_context, info, providerCount));
                channelFindRequester->set(name, searchSequenceId, cid, responseAddress, transport, responseRequired, false);

                for (unsigned int i = 0; i < providerCount; i++) {
                    providers[i]->channelFind(name, channelFindRequester);
                }
            }
        }
    }
    else {
        // Server discovery ping by pvlist
        if (allowed)
        {
            // ~random hold-off to reduce impact of all servers responding.
            // in [0.05, 0.15]
            double delay = double(rand())/RAND_MAX; // [0, 1]
            delay = delay*0.1 + 0.05;
            std::tr1::shared_ptr<NameServerChannelFindRequesterImpl> channelFindRequester(new NameServerChannelFindRequesterImpl(_context, info, 1));
            channelFindRequester->set("", searchSequenceId, 0, responseAddress, transport, true, true);

            TimerCallback::shared_pointer tc = channelFindRequester;
            _context->getTimer()->scheduleAfterDelay(tc, delay);
        }
    }
}

/*
 * Name server search response handler
 */
class NameServerSearchResponseHandler
    : public ResponseHandler
{
public:
    NameServerSearchResponseHandler(const ServerContextImpl::shared_pointer& context);
    virtual ~NameServerSearchResponseHandler();
    virtual void handleResponse(osiSockAddr* responseFrom, const Transport::shared_pointer& transport, epics::pvData::
int8 version, epics::pvData::int8 command, std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE
 FINAL;

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

NameServerSearchResponseHandler::NameServerSearchResponseHandler(const ServerContextImpl::shared_pointer& context)
    : ResponseHandler(context.get(), "NameServerSearchResponseHandler")
    , badResponseHandler(context)
    , beaconHandler(context, "Beacon")
    , validationHandler(context)
    , echoHandler(context)
    , searchHandler(context)
    , authnzHandler(context.get())
    , createChannelHandler(context)
    , destroyChannelHandler(context)
    , rpcHandler(context)
    , handlerTable(CMD_CANCEL_REQUEST+1, &badResponseHandler)
{
    handlerTable[CMD_BEACON] = &badResponseHandler; /*  0 */
    handlerTable[CMD_CONNECTION_VALIDATION] = &validationHandler; /*  1 */
    handlerTable[CMD_ECHO] = &echoHandler; /*  2 */
    handlerTable[CMD_SEARCH] = &searchHandler; /*  3 */
    handlerTable[CMD_SEARCH_RESPONSE] = &badResponseHandler;
    handlerTable[CMD_AUTHNZ] = &authnzHandler; /*  5 */
    handlerTable[CMD_ACL_CHANGE] = &badResponseHandler; /*  6 - access right change */
    handlerTable[CMD_CREATE_CHANNEL] = &createChannelHandler; /*  7 */
    handlerTable[CMD_DESTROY_CHANNEL] = &destroyChannelHandler; /*  8 */
    handlerTable[CMD_CONNECTION_VALIDATED] = &badResponseHandler; /*  9 */

    handlerTable[CMD_GET] = &badResponseHandler; /* 10 - get response */
    handlerTable[CMD_PUT] = &badResponseHandler; /* 11 - put response */
    handlerTable[CMD_PUT_GET] = &badResponseHandler; /* 12 - put-get response */
    handlerTable[CMD_MONITOR] = &badResponseHandler; /* 13 - monitor response */
    handlerTable[CMD_ARRAY] = &badResponseHandler; /* 14 - array response */
    handlerTable[CMD_DESTROY_REQUEST] = &badResponseHandler; /* 15 - destroy request */
    handlerTable[CMD_PROCESS] = &badResponseHandler; /* 16 - process response */
    handlerTable[CMD_GET_FIELD] = &badResponseHandler; /* 17 - get field response */
    handlerTable[CMD_MESSAGE] = &badResponseHandler; /* 18 - message to Requester */
    handlerTable[CMD_MULTIPLE_DATA] = &badResponseHandler; /* 19 - grouped monitors */

    handlerTable[CMD_RPC] = &rpcHandler; /* 20 - RPC response */
    handlerTable[CMD_CANCEL_REQUEST] = &badResponseHandler; /* 21 - cancel request */
}

NameServerSearchResponseHandler::~NameServerSearchResponseHandler()
{
}

void NameServerSearchResponseHandler::handleResponse(osiSockAddr* responseFrom, const Transport::shared_pointer& transport, int8 version, int8 command, size_t payloadSize, ByteBuffer* payloadBuffer)
{
    if(command<0||command>=(int8)handlerTable.size())
    {
        LOG(logLevelError, "Invalid (or unsupported) command: %x.", (0xFF&command));
        if(IS_LOGGABLE(logLevelError)) {
            std::ios::fmtflags initialflags = std::cerr.flags();
            std::cerr << "Invalid (or unsupported) command: "
                      << std::hex << (int)(0xFF&command) << "\n"
                      << HexDump(*payloadBuffer, payloadSize).limit(256u);
            std::cerr.flags(initialflags);
        }
        return;
    }
    // delegate
    handlerTable[command]->handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);
}

/*
 * Name server channel find
 */
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

NameServerChannelFind::NameServerChannelFind(ChannelProvider::shared_pointer& provider)
    : channelProvider(provider)
{
}

NameServerChannelFind::~NameServerChannelFind()
{
}

void NameServerChannelFind::destroy()
{
}

ChannelProvider::shared_pointer NameServerChannelFind::getChannelProvider()
{
    return channelProvider.lock();
};

void NameServerChannelFind::cancel()
{
    throw std::runtime_error("Not supported");
}

/*
 * Name server channel provider class
 */
const std::string NameServerChannelProvider::PROVIDER_NAME("remote");

NameServerChannelProvider::NameServerChannelProvider()
    : nsChannelFind()
    , channelEntryExpirationTime(0)
{
}

NameServerChannelProvider::~NameServerChannelProvider()
{
}

void NameServerChannelProvider::initialize()
{
    ChannelProvider::shared_pointer thisChannelProvider = shared_from_this();
    nsChannelFind.reset(new NameServerChannelFind(thisChannelProvider));
}

void NameServerChannelProvider::setChannelEntryExpirationTime(double expirationTime)
{
    this->channelEntryExpirationTime = expirationTime;
}

void NameServerChannelProvider::setStaticChannelEntries(const ChannelMap& channelMap)
{
    Lock lock(channelMapMutex);
    for (ChannelMap::const_iterator it = channelMap.begin(); it != channelMap.end(); ++it) {
        std::string channelName = it->first;
        ChannelEntry channelEntry = it->second;
        this->channelMap[channelName] = channelEntry;
    }
    LOG(logLevelDebug, "Updated %d static channel entries", int(channelMap.size()));
}

std::string NameServerChannelProvider::NameServerChannelProvider::getProviderName()
{
    return PROVIDER_NAME;
}

void NameServerChannelProvider::destroy()
{
}

ChannelFind::shared_pointer NameServerChannelProvider::channelFind(const std::string& channelName, const ChannelFindRequester::shared_pointer& channelFindRequester)
{
    bool exists = false;
    epicsTimeStamp now;
    epicsTimeGetCurrent(&now);
    {
        Lock lock(channelMapMutex);
        ChannelMap::iterator it = channelMap.find(channelName);
        if (it != channelMap.end()) {
            exists = true;
            // Check expiration time if it is configured
            if (channelEntryExpirationTime > 0) {
                ChannelEntry channelEntry = it->second;
                epicsTimeStamp channelUpdateTime = channelEntry.updateTime;
                double timeSinceUpdate = epicsTimeDiffInSeconds(&now, &channelUpdateTime);
                if (timeSinceUpdate > channelEntryExpirationTime) {
                    LOG(logLevelDebug, "Channel %s was last updated %.2f seconds ago, channel entry has expired", channelName.c_str(), timeSinceUpdate);
                    channelMap.erase(it);
                    exists = false;
                }
            }
        }
    }
    channelFindRequester->channelFindResult(epics::pvData::Status::Ok, nsChannelFind, exists);
    return nsChannelFind;
}

ChannelFind::shared_pointer NameServerChannelProvider::channelList(const ChannelListRequester::shared_pointer& channelListRequester)
{
    if (!channelListRequester.get()) {
        throw std::runtime_error("Null requester");
    }

    epics::pvData::PVStringArray::svector channelNames;
    {
        Lock lock(channelMapMutex);
        for (ChannelMap::const_iterator it = channelMap.begin(); it != channelMap.end(); ++it) {
            std::string channelName = it->first;
            channelNames.push_back(channelName);
        }
    }
    channelListRequester->channelListResult(epics::pvData::Status::Ok, nsChannelFind, freeze(channelNames), true);
    return nsChannelFind;
}

Channel::shared_pointer NameServerChannelProvider::createChannel(const std::string& channelName, const ChannelRequester::shared_pointer& channelRequester, short priority)
{
    return createChannel(channelName, channelRequester, priority, "local");
}

Channel::shared_pointer NameServerChannelProvider::createChannel(const std::string& channelName, const ChannelRequester::shared_pointer& channelRequester, short /*priority*/, const std::string& address)
{
    Channel::shared_pointer nullPtr;
    epics::pvData::Status errorStatus(epics::pvData::Status::STATUSTYPE_ERROR, "Not supported");
    channelRequester->channelCreated(errorStatus, nullPtr);
    return nullPtr;
}

void NameServerChannelProvider::updateChannelMap(const ChannelMap& updatedChannelMap)
{
    Lock lock(channelMapMutex);
    for (ChannelMap::const_iterator it = updatedChannelMap.begin(); it != updatedChannelMap.end(); ++it) {
        std::string channelName = it->first;
        ChannelEntry channelEntry = it->second;
        channelMap[channelName] = channelEntry;
    }
    LOG(logLevelDebug, "Name server channel provider updated %ld channels", updatedChannelMap.size());
}

std::string NameServerChannelProvider::getChannelServerAddress(const std::string& channelName)
{
    std::string serverAddress;
    Lock lock(channelMapMutex);
    ChannelMap::const_iterator it = channelMap.find(channelName);
    if (it != channelMap.end()) {
        serverAddress = it->second.serverAddress;
    }
    return serverAddress;
}

/*
 * Name server class
 */
NameServer::NameServer(const epics::pvAccess::Configuration::shared_pointer& conf)
    : channelProvider(new NameServerChannelProvider)
{
    channelProvider->initialize();
    ServerContext::Config serverConfig = ServerContext::Config().config(conf).provider(channelProvider);
    ServerContextImpl::shared_pointer contextImpl = ServerContextImpl::create(serverConfig);
    ResponseHandler::shared_pointer searchResponseHandler(new NameServerSearchResponseHandler(contextImpl));
    contextImpl->initialize(searchResponseHandler, searchResponseHandler);
    nameServerGuid = contextImpl->getGUID();
    context = contextImpl;
}

NameServer::~NameServer()
{
    shutdown();
    context.reset();
}

void NameServer::setPollPeriod(double pollPeriod)
{
    LOG(logLevelDebug, "Setting server poll period to %.2f seconds", pollPeriod);
    this->pollPeriod = pollPeriod;
}
void NameServer::setPvaTimeout(double timeout)
{
    LOG(logLevelDebug, "Setting PVA timeout to %.2f seconds", timeout);
    this->timeout = timeout;
}
void NameServer::setAutoDiscovery(bool autoDiscovery)
{
    this->autoDiscovery = autoDiscovery;
}
void NameServer::setStaticServerAddresses(const std::string& serverAddresses)
{
    this->serverAddresses = serverAddresses;
}
void NameServer::setStaticChannelEntries(const ChannelMap& channelMap)
{
    this->channelProvider->setStaticChannelEntries(channelMap);
}
void NameServer::setChannelEntryExpirationTime(double expirationTime)
{
    LOG(logLevelDebug, "Setting channel entry expiration time to %.2f seconds", expirationTime);
    this->channelProvider->setChannelEntryExpirationTime(expirationTime);
}

void NameServer::run(double runtime)
{
    epicsTimeStamp startTime;
    epicsTimeGetCurrent(&startTime);
    while(true) {
        ServerAddressList serverAddressList;
        addServersFromAddresses(serverAddressList);
        discoverServers(serverAddressList);
        ChannelMap channelMap;
        discoverChannels(serverAddressList, channelMap);
        channelProvider->updateChannelMap(channelMap);
        epicsTimeStamp now;
        epicsTimeGetCurrent(&now);
        double deltaT = epicsTimeDiffInSeconds(&now, &startTime);
        if (runtime > 0 && deltaT > runtime) {
            break;
        }
        double remainingTime = runtime-deltaT;
        double waitTime = pollPeriod;
        if (waitTime > remainingTime) {
            waitTime = remainingTime;
        }
        epicsThreadSleep(waitTime);
    }
}

bool NameServer::addUniqueServerToList(const std::string& serverAddress, ServerAddressList& serverAddressList)
{
    std::list<std::string>::const_iterator it = std::find(serverAddressList.begin(), serverAddressList.end(), serverAddress);
    if (it == serverAddressList.end()) {
        LOG(logLevelDebug, "Adding server address %s", serverAddress.c_str());
        serverAddressList.push_back(serverAddress);
        return true;
    }
    LOG(logLevelDebug, "Ignoring duplicate server address %s", serverAddress.c_str());
    return false;
}

void NameServer::addServersFromAddresses(ServerAddressList& serverAddressList)
{
    LOG(logLevelDebug, "Adding pre-configured server addresses");
    std::string addresses = StringUtility::replace(serverAddresses, ',', " ");
    InetAddrVector inetAddrVector;
    getSocketAddressList(inetAddrVector, addresses, context->getServerPort());
    int nAddedServers = 0;
    for (unsigned int i = 0; i < inetAddrVector.size(); i++) {
        std::string serverAddress = inetAddressToString(inetAddrVector[i]);
        if (addUniqueServerToList(serverAddress, serverAddressList)) {
            nAddedServers++;
        }
    }
    LOG(logLevelDebug, "Added %d pre-configured server addresses", nAddedServers);
}

void NameServer::discoverChannels(ServerAddressList& serverAddressList, ChannelMap& channelMap)
{
    LOG(logLevelDebug, "Discovering channels for %d servers", int(serverAddressList.size()));
    for (ServerAddressList::const_iterator it = serverAddressList.begin(); it != serverAddressList.end(); ++it) {
        std::string serverAddress = *it;
        discoverServerChannels(serverAddress, channelMap);
    }
}

void NameServer::shutdown()
{
    context->shutdown();
}

std::string NameServer::inetAddrToString(const osiSockAddr& addr)
{
    return inetAddressToString(addr);
}

}}
