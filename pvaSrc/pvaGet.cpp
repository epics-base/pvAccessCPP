/* pvaGet.cpp */
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

#include <sstream>
#include <pv/event.h>
#include <pv/pva.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

namespace epics { namespace pva {


class ChannelGetRequesterImpl : public ChannelGetRequester
{
    PvaGet * pvaGet;
public:
    ChannelGetRequesterImpl(PvaGet * pvaGet)
    : pvaGet(pvaGet) {}
    string getRequesterName()
    {return pvaGet->getRequesterName();}
    void message(string const & message,MessageType messageType)
    {pvaGet->message(message,messageType);}
    void channelGetConnect(
        const Status& status,
        ChannelGet::shared_pointer const & channelGet,
        StructureConstPtr const & structure)
    {pvaGet->channelGetConnect(status,channelGet,structure);}
    void getDone(
        const Status& status,
        ChannelGet::shared_pointer const & channelGet,
        PVStructurePtr const & pvStructure,
        BitSetPtr const & bitSet)
    {pvaGet->getDone(status,channelGet,pvStructure,bitSet);}
};

PvaGet::PvaGet(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        Channel::shared_pointer const & channel,
        PVStructurePtr const &pvRequest)
: pva(pva),
  pvaChannel(pvaChannel),
  channel(channel),
  pvRequest(pvRequest),
  isDestroyed(false),
  connectState(connectIdle),
  getState(getIdle)
{
}

PvaGet::~PvaGet()
{
    destroy();
}

void PvaGet::checkGetState()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    if(connectState==connectIdle) connect();
    if(getState==getIdle) get();
}

// from ChannelGetRequester
string PvaGet::getRequesterName()
{
     PvaPtr yyy = pva.lock();
     if(!yyy) throw std::runtime_error("pva was destroyed");
     return yyy->getRequesterName();
}

void PvaGet::message(string const & message,MessageType messageType)
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("pva was destroyed");
    yyy->message(message, messageType);
}

void PvaGet::channelGetConnect(
    const Status& status,
    ChannelGet::shared_pointer const & channelGet,
    StructureConstPtr const & structure)
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    channelGetConnectStatus = status;
    this->channelGet = channelGet;
    if(status.isOK()) {
        pvaData = PvaGetData::create(structure);
        pvaData->setMessagePrefix(channel->getChannelName());
    }
    waitForConnect.signal();
    
}

void PvaGet::getDone(
    const Status& status,
    ChannelGet::shared_pointer const & channelGet,
    PVStructurePtr const & pvStructure,
    BitSetPtr const & bitSet)
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    channelGetStatus = status;
    if(status.isOK()) {
        pvaData->setData(pvStructure,bitSet);
    }
    waitForGet.signal();
}


// from PvaGet
void PvaGet::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    if(channelGet) channelGet->destroy();
    channelGet.reset();
}

void PvaGet::connect()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    issueConnect();
    Status status = waitConnect();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaGet::connect " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaGet::issueConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    if(connectState!=connectIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaGet already connected ";
        throw std::runtime_error(ss.str());
    }
    getRequester = ChannelGetRequester::shared_pointer(new ChannelGetRequesterImpl(this));
    connectState = connectActive;
    channelGet = channel->createChannelGet(getRequester,pvRequest);
}

Status PvaGet::waitConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    if(connectState!=connectActive) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaGet illegal connect state ";
        throw std::runtime_error(ss.str());
    }
    waitForConnect.wait();
    if(channelGetConnectStatus.isOK()){
        connectState = connected;
        return Status::Ok;
    }
    connectState = connectIdle;
    return Status(Status::STATUSTYPE_ERROR,channelGetConnectStatus.getMessage());
}

void PvaGet::get()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    issueGet();
    Status status = waitGet();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaGet::get " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaGet::issueGet()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    if(connectState==connectIdle) connect();
    if(getState!=getIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaGet::issueGet get aleady active ";
        throw std::runtime_error(ss.str());
    }
    getState = getActive;
    channelGet->get();
}

Status PvaGet::waitGet()
{
    if(isDestroyed) throw std::runtime_error("pvaGet was destroyed");
    if(getState!=getActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaGet::waitGet llegal get state";
        throw std::runtime_error(ss.str());
    }
    waitForGet.wait();
    getState = getIdle;
    if(channelGetStatus.isOK()) {
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelGetStatus.getMessage());
}
PvaGetDataPtr PvaGet::getData()
{
    checkGetState();
    return pvaData;
}

PvaGetPtr PvaGet::create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        Channel::shared_pointer const & channel,
        PVStructurePtr const &pvRequest)
{
    PvaGetPtr epv(new PvaGet(pva,pvaChannel,channel,pvRequest));
    return epv;
}

}}
