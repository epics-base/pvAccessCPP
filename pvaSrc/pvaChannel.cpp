/* pvaChannel.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.02
 */
#define epicsExportSharedSymbols

#include <map>
#include <sstream>
#include <pv/event.h>
#include <pv/lock.h>
#include <pv/pva.h>
#include <pv/createRequest.h>


using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

namespace epics { namespace pva {


class PvaGetCache
{
public:
    PvaGetCache(){}
    ~PvaGetCache();
    void destroy() {
       pvaGetMap.clear();
    }
    PvaGetPtr getGet(string const & request);
    void addGet(string const & request,PvaGetPtr const & pvaGet);
private:
    map<string,PvaGetPtr> pvaGetMap;
};

PvaGetCache::~PvaGetCache()
{
    destroy();
}

PvaGetPtr PvaGetCache::getGet(string const & request)
{
    map<string,PvaGetPtr>::iterator iter = pvaGetMap.find(request);
    if(iter!=pvaGetMap.end()) return iter->second;
    return PvaGetPtr();
}

void PvaGetCache::addGet(string const & request,PvaGetPtr const & pvaGet)
{
     pvaGetMap.insert(std::pair<string,PvaGetPtr>(
         request,pvaGet));
}


class PvaPutCache
{
public:
    PvaPutCache(){}
    ~PvaPutCache();
    void destroy() {
       pvaPutMap.clear();
    }
    PvaPutPtr getPut(string const & request);
    void addPut(string const & request,PvaPutPtr const & pvaPut);
private:
    map<string,PvaPutPtr> pvaPutMap;
};

PvaPutCache::~PvaPutCache()
{
    destroy();
}

PvaPutPtr PvaPutCache::getPut(string const & request)
{
    map<string,PvaPutPtr>::iterator iter = pvaPutMap.find(request);
    if(iter!=pvaPutMap.end()) return iter->second;
    return PvaPutPtr();
}

void PvaPutCache::addPut(string const & request,PvaPutPtr const & pvaPut)
{
     pvaPutMap.insert(std::pair<string,PvaPutPtr>(
         request,pvaPut));
}

class ChannelRequesterImpl : public ChannelRequester
{
     PvaChannel *pvaChannel;
public:
     ChannelRequesterImpl(PvaChannel *pvaChannel)
     : pvaChannel(pvaChannel) {}
    void channelCreated(
        const Status& status,
        Channel::shared_pointer const & channel)
    { pvaChannel->channelCreated(status,channel); }
    void channelStateChange(
        Channel::shared_pointer const & channel,
        Channel::ConnectionState connectionState)
    {pvaChannel->channelStateChange(channel,connectionState);}
    tr1::shared_ptr<Channel> getChannel() {return pvaChannel->getChannel();}
    string getRequesterName()
    {return pvaChannel->getRequesterName();}
    void message(
        string const & message,
        MessageType messageType)
    { pvaChannel->message(message,messageType); }
    void destroy() {pvaChannel->destroy();}
};


PvaChannel::PvaChannel(
    PvaPtr const &pva,
    string const & channelName,
    string const & providerName)
: pva(pva),
  channelName(channelName),
  providerName(providerName),
  connectState(connectIdle),
  isDestroyed(false),
  createRequest(CreateRequest::create()),
  pvaGetCache(new PvaGetCache()),
  pvaPutCache(new PvaPutCache())
{}

PvaChannel::~PvaChannel()
{
    destroy();
}

void PvaChannel::channelCreated(const Status& status, Channel::shared_pointer const & channel)
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    if(status.isOK()) {
        this->channel = channel;
        return;
    }
    cout << "PvaChannel::channelCreated status " << status.getMessage() << " why??\n";
}

void PvaChannel::channelStateChange(
    Channel::shared_pointer const & channel,
    Channel::ConnectionState connectionState)
{
    if(isDestroyed) return;
    bool waitingForConnect = false;
    if(connectState==connectActive) waitingForConnect = true;
    if(connectionState!=Channel::CONNECTED) {
         string mess(channelName +
             " connection state " + Channel::ConnectionStateNames[connectionState]);
         message(mess,errorMessage);
         channelConnectStatus = Status(Status::STATUSTYPE_ERROR,mess);
         connectState = notConnected;
    } else {
         connectState = connected;
    }
    if(waitingForConnect) waitForConnect.signal();
}

string PvaChannel::getRequesterName()
{
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    return yyy->getRequesterName();
}

void PvaChannel::message(
    string const & message,
    MessageType messageType)
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    yyy->message(message, messageType);
}

void PvaChannel::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    if(channel) channel->destroy();
    channel.reset();
    pvaGetCache.reset();
    pvaPutCache.reset();
}

string PvaChannel::getChannelName()
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    return channelName;
}

Channel::shared_pointer PvaChannel::getChannel()
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    return channel;
}

void PvaChannel::connect(double timeout)
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    issueConnect();
    Status status = waitConnect(timeout);
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << getChannelName() << " PvaChannel::connect " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaChannel::issueConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    if(connectState!=connectIdle) {
       throw std::runtime_error("pvaChannel already connected");
    }
    channelRequester = ChannelRequester::shared_pointer(new ChannelRequesterImpl(this));

    channelConnectStatus = Status(
           Status::STATUSTYPE_ERROR,
           getChannelName() + " createChannel failed");
    connectState = connectActive;
    ChannelProviderRegistry::shared_pointer reg = getChannelProviderRegistry();
    ChannelProvider::shared_pointer provider = reg->getProvider(providerName);
    if(!provider) {
        throw std::runtime_error(getChannelName() + " provider " + providerName + " not registered");
    }
    channel = provider->createChannel(channelName,channelRequester,ChannelProvider::PRIORITY_DEFAULT);
    if(!channel) {
         throw std::runtime_error(channelConnectStatus.getMessage());
    }
}

Status PvaChannel::waitConnect(double timeout)
{
    if(isDestroyed) throw std::runtime_error("pvaChannel was destroyed");
    waitForConnect.wait(timeout);
    if(connectState==connected) return Status::Ok;
    return Status(Status::STATUSTYPE_ERROR,channelConnectStatus.getMessage());
}

PvaFieldPtr PvaChannel::createField()
{
    return createField("");
}

PvaFieldPtr PvaChannel::createField(string const & subField)
{
    throw std::runtime_error("PvaChannel::createField not implemented");
}

PvaProcessPtr PvaChannel::createProcess()
{
    return createProcess("");
}

PvaProcessPtr PvaChannel::createProcess(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createProcess invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createProcess(pvRequest);
}

PvaProcessPtr PvaChannel::createProcess(PVStructurePtr const &  pvRequest)
{
    if(connectState!=connected) connect(5.0);
    if(connectState!=connected) throw std::runtime_error("PvaChannel::creatProcess not connected");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    return PvaProcess::create(yyy,getPtrSelf(),channel,pvRequest);
}

PvaGetPtr PvaChannel::get() {return get("value,alarm,timeStamp");}

PvaGetPtr PvaChannel::get(string const & request)
{
    PvaGetPtr pvaGet = pvaGetCache->getGet(request);
    if(pvaGet) return pvaGet;
    pvaGet = createGet(request);
    pvaGet->connect();
    pvaGetCache->addGet(request,pvaGet);
    return pvaGet;
}

PvaGetPtr PvaChannel::createGet()
{
    return PvaChannel::createGet("value,alarm,timeStamp");
}

PvaGetPtr PvaChannel::createGet(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createGet invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createGet(pvRequest);
}

PvaGetPtr PvaChannel::createGet(PVStructurePtr const &  pvRequest)
{
    if(connectState!=connected) connect(5.0);
    if(connectState!=connected) throw std::runtime_error("PvaChannel::creatGet not connected");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    return PvaGet::create(yyy,getPtrSelf(),channel,pvRequest);
}

PvaPutPtr PvaChannel::put() {return put("value");}

PvaPutPtr PvaChannel::put(string const & request)
{
    PvaPutPtr pvaPut = pvaPutCache->getPut(request);
    if(pvaPut) return pvaPut;
    pvaPut = createPut(request);
    pvaPut->connect();
    pvaPut->get();
    pvaPutCache->addPut(request,pvaPut);
    return pvaPut;
}

PvaPutPtr PvaChannel::createPut()
{
    return createPut("value");
}

PvaPutPtr PvaChannel::createPut(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createPut invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createPut(pvRequest);
}

PvaPutPtr PvaChannel::createPut(PVStructurePtr const & pvRequest)
{
    if(connectState!=connected) connect(5.0);
    if(connectState!=connected) throw std::runtime_error("PvaChannel::creatPut not connected");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    return PvaPut::create(yyy,getPtrSelf(),channel,pvRequest);
}

PvaPutGetPtr PvaChannel::createPutGet()
{
    return createPutGet("putField(argument)getField(result)");
}

PvaPutGetPtr PvaChannel::createPutGet(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createPutGet invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createPutGet(pvRequest);
}

PvaPutGetPtr PvaChannel::createPutGet(PVStructurePtr const & pvRequest)
{
    if(connectState!=connected) connect(5.0);
    if(connectState!=connected) throw std::runtime_error("PvaChannel::creatPutGet not connected");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    return PvaPutGet::create(yyy,getPtrSelf(),channel,pvRequest);
}

PvaRPCPtr PvaChannel::createRPC()
{
    return createRPC("");
}

PvaRPCPtr PvaChannel::createRPC(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createRPC invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createRPC(pvRequest);
}

PvaRPCPtr PvaChannel::createRPC(PVStructurePtr const & pvRequest)
{
    throw std::runtime_error("PvaChannel::createRPC not implemented");
}

PvaArrayPtr PvaChannel::createArray()
{
    return createArray("value");
}

PvaArrayPtr PvaChannel::createArray(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createArray invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createArray(pvRequest);
}

PvaArrayPtr PvaChannel::createArray(PVStructurePtr const &  pvRequest)
{
    throw std::runtime_error("PvaChannel::createArray not implemented");
}


PvaMonitorPtr PvaChannel::monitor() {return monitor("value,alarm,timeStamp");}

PvaMonitorPtr PvaChannel::monitor(string const & request)
{
    PvaMonitorPtr pvaMonitor = createMonitor(request);
    pvaMonitor->connect();
    pvaMonitor->start();
    return pvaMonitor;
}

PvaMonitorPtr PvaChannel::monitor(PvaMonitorRequesterPtr const & pvaMonitorRequester)
{    return monitor("value,alarm,timeStamp",pvaMonitorRequester);
}

PvaMonitorPtr PvaChannel::monitor(string const & request,
    PvaMonitorRequesterPtr const & pvaMonitorRequester)
{
    PvaMonitorPtr pvaMonitor = createMonitor(request);
    pvaMonitor->connect();
    pvaMonitor->setRequester(pvaMonitorRequester);
    pvaMonitor->start();
    return pvaMonitor;
}

PvaMonitorPtr PvaChannel::createMonitor()
{
    return createMonitor("value,alarm,timeStamp");
}

PvaMonitorPtr PvaChannel::createMonitor(string const & request)
{
    PVStructurePtr pvRequest = createRequest->createRequest(request);
    if(!pvRequest) {
        stringstream ss;
        ss << "channel " << getChannelName();
        ss << " PvaChannel::createMonitor invalid pvRequest: " + createRequest->getMessage();
        throw std::runtime_error(ss.str());
    }
    return createMonitor(pvRequest);
}

PvaMonitorPtr  PvaChannel::createMonitor(PVStructurePtr const &  pvRequest)
{
    if(connectState!=connected) connect(5.0);
    if(connectState!=connected) throw std::runtime_error("PvaChannel::createMonitor not connected");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("Pva was destroyed");
    return PvaMonitor::create(yyy,getPtrSelf(),channel,pvRequest);
}


PvaChannelPtr PvaChannel::create(
   PvaPtr const &pva,
   string const & channelName,
   string const & providerName)
{
    PvaChannelPtr channel(new PvaChannel(pva,channelName,providerName));
    return channel;
}

}}
