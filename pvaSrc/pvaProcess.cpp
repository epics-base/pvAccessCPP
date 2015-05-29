/* pvaProcess.cpp */
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


class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    PvaProcess * pvaProcess;
public:
    ChannelProcessRequesterImpl(PvaProcess * pvaProcess)
    : pvaProcess(pvaProcess) {}
    string getRequesterName()
    {return pvaProcess->getRequesterName();}
    void message(string const & message,MessageType messageType)
    {pvaProcess->message(message,messageType);}
    void channelProcessConnect(
        const Status& status,
        ChannelProcess::shared_pointer const & channelProcess)
    {pvaProcess->channelProcessConnect(status,channelProcess);}
    void processDone(
        const Status& status,
        ChannelProcess::shared_pointer const & channelProcess)
    {pvaProcess->processDone(status,channelProcess);}
};

PvaProcess::PvaProcess(
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
  processState(processIdle)
{
}

PvaProcess::~PvaProcess()
{
    destroy();
}

void PvaProcess::checkProcessState()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    if(connectState==connectIdle) connect();
    if(processState==processIdle) process();
}

// from ChannelProcessRequester
string PvaProcess::getRequesterName()
{
     PvaPtr yyy = pva.lock();
     if(!yyy) throw std::runtime_error("pva was destroyed");
     return yyy->getRequesterName();
}

void PvaProcess::message(string const & message,MessageType messageType)
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("pva was destroyed");
    yyy->message(message, messageType);
}

void PvaProcess::channelProcessConnect(
    const Status& status,
    ChannelProcess::shared_pointer const & channelProcess)
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    channelProcessConnectStatus = status;
    this->channelProcess = channelProcess;
    waitForConnect.signal();
    
}

void PvaProcess::processDone(
    const Status& status,
    ChannelProcess::shared_pointer const & channelProcess)
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    channelProcessStatus = status;
    waitForProcess.signal();
}


// from PvaProcess
void PvaProcess::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    if(channelProcess) channelProcess->destroy();
    channelProcess.reset();
}

void PvaProcess::connect()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    issueConnect();
    Status status = waitConnect();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaProcess::connect " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaProcess::issueConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    if(connectState!=connectIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaProcess already connected ";
        throw std::runtime_error(ss.str());
    }
    processRequester = ChannelProcessRequester::shared_pointer(new ChannelProcessRequesterImpl(this));
    connectState = connectActive;
    channelProcess = channel->createChannelProcess(processRequester,pvRequest);
}

Status PvaProcess::waitConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    if(connectState!=connectActive) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaProcess illegal connect state ";
        throw std::runtime_error(ss.str());
    }
    waitForConnect.wait();
    if(channelProcessConnectStatus.isOK()){
        connectState = connected;
        return Status::Ok;
    }
    connectState = connectIdle;
    return Status(Status::STATUSTYPE_ERROR,channelProcessConnectStatus.getMessage());
}

void PvaProcess::process()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    issueProcess();
    Status status = waitProcess();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaProcess::process " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaProcess::issueProcess()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    if(connectState==connectIdle) connect();
    if(processState!=processIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaProcess::issueProcess process aleady active ";
        throw std::runtime_error(ss.str());
    }
    processState = processActive;
    channelProcess->process();
}

Status PvaProcess::waitProcess()
{
    if(isDestroyed) throw std::runtime_error("pvaProcess was destroyed");
    if(processState!=processActive){
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " PvaProcess::waitProcess llegal process state";
        throw std::runtime_error(ss.str());
    }
    waitForProcess.wait();
    processState = processIdle;
    if(channelProcessStatus.isOK()) {
        return Status::Ok;
    }
    return Status(Status::STATUSTYPE_ERROR,channelProcessStatus.getMessage());
}

PvaProcessPtr PvaProcess::create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        Channel::shared_pointer const & channel,
        PVStructurePtr const &pvRequest)
{
    PvaProcessPtr epv(new PvaProcess(pva,pvaChannel,channel,pvRequest));
    return epv;
}

}}
