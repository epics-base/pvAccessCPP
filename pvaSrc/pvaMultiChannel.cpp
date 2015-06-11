/* pvaMultiChannel.cpp */
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



PvaMultiChannel::PvaMultiChannel(
    PvaPtr const &pva,
    PVStringArrayPtr const & channelName,
    string const & providerName)
: pva(pva),
  channelName(channelName),
  providerName(providerName),
  numChannel(channelName->getLength()),
  isConnected(getPVDataCreate()->createPVScalarArray<PVBooleanArray>()),
  isDestroyed(false)
{
}

PvaMultiChannel::~PvaMultiChannel()
{
    destroy();
}

void PvaMultiChannel::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    pvaChannelArray.reset();
}

PVStringArrayPtr PvaMultiChannel::getChannelNames()
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    return channelName;
}

Status PvaMultiChannel::connect(double timeout,size_t maxNotConnected)
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    if(pvaChannelArray) throw std::runtime_error("pvaMultiChannel already connected");
    PvaPtr pva = this->pva.lock();
    if(!pva) return Status(Status::STATUSTYPE_ERROR,"pva is gone");
    shared_vector<PvaChannelPtr> pvaChannel(numChannel,PvaChannelPtr());
    PVStringArray::const_svector channelNames = channelName->view();
    shared_vector<boolean> isConnected(numChannel,false);
    for(size_t i=0; i< numChannel; ++i) {
        pvaChannel[i] = pva->createChannel(channelNames[i],providerName);
        pvaChannel[i]->issueConnect();
    }
    Status returnStatus = Status::Ok;
    Status status = Status::Ok;
    size_t numBad = 0;
    for(size_t i=0; i< numChannel; ++i) {
	if(numBad==0) {
            status = pvaChannel[i]->waitConnect(timeout);
        } else {
            status = pvaChannel[i]->waitConnect(.001);
        }
        if(status.isOK()) {
            ++numConnected;
            isConnected[i] = true;
            continue;
        }
        if(returnStatus.isOK()) returnStatus = status;
        ++numBad;
        if(numBad>maxNotConnected) break;
    }
    pvaChannelArray = PvaChannelArrayPtr(new PvaChannelArray(freeze(pvaChannel)));
    this->isConnected->replace(freeze(isConnected));
    return numBad>maxNotConnected ? returnStatus : Status::Ok;
}


bool PvaMultiChannel::allConnected()
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    if(!pvaChannelArray) throw std::runtime_error("pvaMultiChannel not connected");
    if(numConnected==numChannel) return true;
    return (numConnected==numChannel) ? true : false;
}

bool PvaMultiChannel::connectionChange()
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    if(!pvaChannelArray) throw std::runtime_error("pvaMultiChannel not connected");
    if(numConnected==numChannel) return true;
    PVBooleanArray::const_svector isConnected = this->isConnected->view();
    shared_vector<const PvaChannelPtr> channels = *pvaChannelArray.get();
    for(size_t i=0; i<numChannel; ++i) {
         const PvaChannelPtr pvaChannel = channels[i];
         Channel::shared_pointer channel = pvaChannel->getChannel();
         Channel::ConnectionState stateNow = channel->getConnectionState();
         bool connectedNow = stateNow==Channel::CONNECTED ? true : false;
         if(connectedNow!=isConnected[i]) return true;
    }
    return false;
}

PVBooleanArrayPtr PvaMultiChannel::getIsConnected()
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    if(!pvaChannelArray) throw std::runtime_error("pvaMultiChannel not connected");
    if(!connectionChange()) return isConnected;
    shared_vector<boolean> isConnected(numChannel,false);
    shared_vector<const PvaChannelPtr> channels = *pvaChannelArray.get();
    for(size_t i=0; i<numChannel; ++i) {
         const PvaChannelPtr pvaChannel = channels[i];
         Channel::shared_pointer channel = pvaChannel->getChannel();
         Channel::ConnectionState stateNow = channel->getConnectionState();
         if(stateNow==Channel::CONNECTED) isConnected[i] = true;
    }
    this->isConnected->replace(freeze(isConnected));
    return this->isConnected;
}

PvaChannelArrayWPtr PvaMultiChannel::getPvaChannelArray()
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    if(!pvaChannelArray) throw std::runtime_error("pvaMultiChannel not connected");
    return pvaChannelArray;
}

Pva::weak_pointer PvaMultiChannel::getPva()
{
    if(isDestroyed) throw std::runtime_error("pvaMultiChannel was destroyed");
    return pva;
}

PvaMultiChannelPtr PvaMultiChannel::create(
   PvaPtr const &pva,
   PVStringArrayPtr const & channelNames,
   string const & providerName)
{
    PvaMultiChannelPtr channel(new PvaMultiChannel(pva,channelNames,providerName));
    return channel;
}

}}
