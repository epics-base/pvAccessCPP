/* pvaPut.cpp */
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

class ChannelPutRequesterImpl : public ChannelPutRequester
{
    PvaPut * pvaPut;
public:
    ChannelPutRequesterImpl(PvaPut * pvaPut)
    : pvaPut(pvaPut) {}
    string getRequesterName()
    {return pvaPut->getRequesterName();}
    void message(string const & message,MessageType messageType)
    {pvaPut->message(message,messageType);}
    void channelPutConnect(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut,
        StructureConstPtr const & structure)
    {pvaPut->channelPutConnect(status,channelPut,structure);}
    void getDone(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut,
        PVStructurePtr const & pvStructure,
        BitSetPtr const & bitSet)
    {pvaPut->getDone(status,channelPut,pvStructure,bitSet);}
    void putDone(
        const Status& status,
        ChannelPut::shared_pointer const & channelPut)
    {pvaPut->putDone(status,channelPut);}
};

PvaPut::PvaPut(
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
  putState(putIdle)
{
}

PvaPut::~PvaPut()
{
    destroy();
}

void PvaPut::checkPutState()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(connectState==connectIdle){
          connect();
          get();
    }
}

// from ChannelPutRequester
string PvaPut::getRequesterName()
{
     PvaPtr yyy = pva.lock();
     if(!yyy) throw std::runtime_error("pva was destroyed");
     return yyy->getRequesterName();
}

void PvaPut::message(string const & message,MessageType messageType)
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("pva was destroyed");
    yyy->message(message, messageType);
}

void PvaPut::channelPutConnect(
    const Status& status,
    ChannelPut::shared_pointer const & channelPut,
    StructureConstPtr const & structure)
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    channelPutConnectStatus = status;
    this->channelPut = channelPut;
    if(status.isOK()) {
        pvaData = PvaPutData::create(structure);
        pvaData->setMessagePrefix(pvaChannel.lock()->getChannelName());
    }
    waitForConnect.signal();
    
}

void PvaPut::getDone(
    const Status& status,
    ChannelPut::shared_pointer const & channelPut,
    PVStructurePtr const & pvStructure,
    BitSetPtr const & bitSet)
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    channelGetPutStatus = status;
    if(status.isOK()) {
        PVStructurePtr pvs = pvaData->getPVStructure();
        pvs->copyUnchecked(*pvStructure,*bitSet);
        BitSetPtr bs = pvaData->getBitSet();
        bs->clear();
        *bs |= *bitSet;
    }
    waitForGetPut.signal();
}

void PvaPut::putDone(
    const Status& status,
    ChannelPut::shared_pointer const & channelPut)
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    channelGetPutStatus = status;
    waitForGetPut.signal();
}


// from PvaPut
void PvaPut::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    if(channelPut) channelPut->destroy();
    channelPut.reset();
}

void PvaPut::connect()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    issueConnect();
    Status status = waitConnect();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPut::connect " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPut::issueConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(connectState!=connectIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaPut already connected ";
        throw std::runtime_error(ss.str());
    }
    putRequester = ChannelPutRequester::shared_pointer(new ChannelPutRequesterImpl(this));
    connectState = connectActive;
    channelPut = channel->createChannelPut(putRequester,pvRequest);
}

Status PvaPut::waitConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(connectState!=connectActive) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaPut illegal connect state ";
        throw std::runtime_error(ss.str());
    }
    waitForConnect.wait();
    if(channelPutConnectStatus.isOK()) {
        connectState = connected;
        return Status::Ok;
    }
    connectState = connectIdle;
    return Status(Status::STATUSTYPE_ERROR,channelPutConnectStatus.getMessage());
}

void PvaPut::get()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    issueGet();
    Status status = waitGet();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPut::get " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPut::issueGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(connectState==connectIdle) connect();
    if(putState!=putIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPut::issueGet get or put aleady active ";
        throw std::runtime_error(ss.str());
    }
    putState = getActive;
    pvaData->getBitSet()->clear();
    channelPut->get();
}

Status PvaPut::waitGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(putState!=getActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPut::waitGet llegal put state";
        throw std::runtime_error(ss.str());
    }
    waitForGetPut.wait();
    putState = putIdle;
    if(channelGetPutStatus.isOK()) {
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelGetPutStatus.getMessage());
}

void PvaPut::put()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    issuePut();
    Status status = waitPut();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPut::put " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPut::issuePut()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(connectState==connectIdle) connect();
    if(putState!=putIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPut::issueGet get or put aleady active ";
        throw std::runtime_error(ss.str());
    }
    putState = putActive;
    channelPut->put(pvaData->getPVStructure(),pvaData->getBitSet());
}

Status PvaPut::waitPut()
{
    if(isDestroyed) throw std::runtime_error("pvaPut was destroyed");
    if(putState!=putActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPut::waitPut llegal put state";
        throw std::runtime_error(ss.str());
    }
    waitForGetPut.wait();
    putState = putIdle;
    if(channelGetPutStatus.isOK()) {
        pvaData->getBitSet()->clear();
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelGetPutStatus.getMessage());
}

PvaPutDataPtr PvaPut::getData()
{
    checkPutState();
    return pvaData;
}

PvaPutPtr PvaPut::create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        Channel::shared_pointer const & channel,
        PVStructurePtr const &pvRequest)
{
    PvaPutPtr epv(new PvaPut(pva,pvaChannel,channel,pvRequest));
    return epv;
}


}}
