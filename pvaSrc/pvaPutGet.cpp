/* pvaPutGet.cpp */
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

class ChannelPutGetRequesterImpl : public ChannelPutGetRequester
{
    PvaPutGet * pvaPutGet;
public:
    ChannelPutGetRequesterImpl(PvaPutGet * pvaPutGet)
    : pvaPutGet(pvaPutGet) {}
    string getRequesterName()
    {return pvaPutGet->getRequesterName();}
    void message(string const & message,MessageType messageType)
    {pvaPutGet->message(message,messageType);}
    void channelPutGetConnect(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::StructureConstPtr const & putStructure,
        epics::pvData::StructureConstPtr const & getStructure)
    {
         pvaPutGet->channelPutGetConnect(status,channelPutGet,putStructure,getStructure);
    }
    void putGetDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructurePtr const & getPVStructure,
        epics::pvData::BitSetPtr const & getBitSet)
    {
        pvaPutGet->putGetDone(status,channelPutGet,getPVStructure,getBitSet);
    }
    void getPutDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructurePtr const & putPVStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet)
    {
        pvaPutGet->getPutDone(status,channelPutGet,putPVStructure,putBitSet);
    }
    void getGetDone(
        const epics::pvData::Status& status,
        epics::pvAccess::ChannelPutGet::shared_pointer const & channelPutGet,
        epics::pvData::PVStructurePtr const & getPVStructure,
        epics::pvData::BitSet::shared_pointer const & getBitSet)
    {
        pvaPutGet->getGetDone(status,channelPutGet,getPVStructure,getBitSet);
    }
};

PvaPutGet::PvaPutGet(
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
  putGetState(putGetIdle)
{
}

PvaPutGet::~PvaPutGet()
{
    destroy();
}

void PvaPutGet::checkPutGetState()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(connectState==connectIdle){
        connect();
        getPut();
    }
}

// from ChannelPutGetRequester
string PvaPutGet::getRequesterName()
{
     PvaPtr yyy = pva.lock();
     if(!yyy) throw std::runtime_error("pva was destroyed");
     return yyy->getRequesterName();
}

void PvaPutGet::message(string const & message,MessageType messageType)
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("pva was destroyed");
    yyy->message(message, messageType);
}

void PvaPutGet::channelPutGetConnect(
    const Status& status,
    ChannelPutGet::shared_pointer const & channelPutGet,
    StructureConstPtr const & putStructure,
    StructureConstPtr const & getStructure)
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    channelPutGetConnectStatus = status;
    this->channelPutGet = channelPutGet;
    if(status.isOK()) {
        pvaPutData = PvaPutData::create(putStructure);
        pvaPutData->setMessagePrefix(pvaChannel.lock()->getChannelName());
        pvaGetData = PvaGetData::create(getStructure);
        pvaGetData->setMessagePrefix(pvaChannel.lock()->getChannelName());
    }
    waitForConnect.signal();
    
}

void PvaPutGet::putGetDone(
        const Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        PVStructurePtr const & getPVStructure,
        BitSetPtr const & getBitSet)
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    channelPutGetStatus = status;
    if(status.isOK()) {
        pvaGetData->setData(getPVStructure,getBitSet);
    }
    waitForPutGet.signal();
}

void PvaPutGet::getPutDone(
    const Status& status,
    ChannelPutGet::shared_pointer const & channelPutGet,
    PVStructurePtr const & putPVStructure,
    BitSetPtr const & putBitSet)
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    channelGetPutGetStatus = status;
    if(status.isOK()) {
        PVStructurePtr pvs = pvaPutData->getPVStructure();
        pvs->copyUnchecked(*putPVStructure,*putBitSet);
        BitSetPtr bs = pvaPutData->getBitSet();
        bs->clear();
        *bs |= *putBitSet;
    }
    waitForPutGet.signal();
}

void PvaPutGet::getGetDone(
        const Status& status,
        ChannelPutGet::shared_pointer const & channelPutGet,
        PVStructurePtr const & getPVStructure,
        BitSetPtr const & getBitSet)
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    channelPutGetStatus = status;
    if(status.isOK()) {
        pvaGetData->setData(getPVStructure,getBitSet);
    }
    waitForPutGet.signal();
}



// from PvaPutGet
void PvaPutGet::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    if(channelPutGet) channelPutGet->destroy();
    channelPutGet.reset();
}

void PvaPutGet::connect()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    issueConnect();
    Status status = waitConnect();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPutGet::connect " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPutGet::issueConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(connectState!=connectIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaPutGet already connected ";
        throw std::runtime_error(ss.str());
    }
    putGetRequester = ChannelPutGetRequester::shared_pointer(new ChannelPutGetRequesterImpl(this));
    connectState = connectActive;
    channelPutGet = channel->createChannelPutGet(putGetRequester,pvRequest);
}

Status PvaPutGet::waitConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(connectState!=connectActive) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaPutGet illegal connect state ";
        throw std::runtime_error(ss.str());
    }
    waitForConnect.wait();
    if(channelPutGetConnectStatus.isOK()) {
        connectState = connected;
        return Status::Ok;
    }
    connectState = connectIdle;
    return Status(Status::STATUSTYPE_ERROR,channelPutGetConnectStatus.getMessage());
}


void PvaPutGet::putGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    issuePutGet();
    Status status = waitPutGet();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPutGet::putGet " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPutGet::issuePutGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(connectState==connectIdle) connect();
    if(putGetState!=putGetIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPutGet::issueGet get or put aleady active ";
        throw std::runtime_error(ss.str());
    }
    putGetState = putGetActive;
    channelPutGet->putGet(pvaPutData->getPVStructure(),pvaPutData->getBitSet());
}


Status PvaPutGet::waitPutGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(putGetState!=putGetActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPutGet::waitPutGet llegal put state";
        throw std::runtime_error(ss.str());
    }
    waitForPutGet.wait();
    putGetState = putGetIdle;
    if(channelGetPutGetStatus.isOK()) {
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelGetPutGetStatus.getMessage());
}

void PvaPutGet::getGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    issueGetGet();
    Status status = waitGetGet();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPutGet::getGet " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPutGet::issueGetGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(connectState==connectIdle) connect();
    if(putGetState!=putGetIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPutGet::issueGetGet aleady active ";
        throw std::runtime_error(ss.str());
    }
    putGetState = putGetActive;
    channelPutGet->getGet();
}

Status PvaPutGet::waitGetGet()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(putGetState!=putGetActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPutGet::waitGetGet illegal state";
        throw std::runtime_error(ss.str());
    }
    waitForPutGet.wait();
    putGetState = putGetIdle;
    if(channelGetPutGetStatus.isOK()) {
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelGetPutGetStatus.getMessage());
}

void PvaPutGet::getPut()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    issueGetPut();
    Status status = waitGetPut();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaPutGet::getPut " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaPutGet::issueGetPut()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(connectState==connectIdle) connect();
    if(putGetState!=putGetIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPutGet::issueGetPut aleady active ";
        throw std::runtime_error(ss.str());
    }
    putGetState = putGetActive;
    channelPutGet->getPut();
}

Status PvaPutGet::waitGetPut()
{
    if(isDestroyed) throw std::runtime_error("pvaPutGet was destroyed");
    if(putGetState!=putGetActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaPutGet::waitGetPut illegal state";
        throw std::runtime_error(ss.str());
    }
    waitForPutGet.wait();
    putGetState = putGetIdle;
    if(channelGetPutGetStatus.isOK()) {
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelGetPutGetStatus.getMessage());
}

PvaGetDataPtr PvaPutGet::getGetData()
{
    checkPutGetState();
    return pvaGetData;
}

PvaPutDataPtr PvaPutGet::getPutData()
{
    checkPutGetState();
    return pvaPutData;
}

PvaPutGetPtr PvaPutGet::create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        Channel::shared_pointer const & channel,
        PVStructurePtr const &pvRequest)
{
    PvaPutGetPtr epv(new PvaPutGet(pva,pvaChannel,channel,pvRequest));
    return epv;
}


}}
