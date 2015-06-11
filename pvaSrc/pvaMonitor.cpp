/* pvaMonitor.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.03
 */
#define epicsExportSharedSymbols

#include <sstream>
#include <pv/event.h>
#include <pv/pva.h>
#include <pv/bitSetUtil.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

namespace epics { namespace pva {


class ChannelMonitorRequester : public MonitorRequester
{
    PvaMonitor * pvaMonitor;
public:
    ChannelMonitorRequester(PvaMonitor * pvaMonitor)
    : pvaMonitor(pvaMonitor) {}
    string getRequesterName()
    {return pvaMonitor->getRequesterName();}
    void message(string const & message,MessageType messageType)
    {pvaMonitor->message(message,messageType);}
    void monitorConnect(
        const Status& status,
        Monitor::shared_pointer const & monitor,
        StructureConstPtr const & structure)
    {pvaMonitor->monitorConnect(status,monitor,structure);}
    void monitorEvent(MonitorPtr const & monitor)
    {
         pvaMonitor->monitorEvent(monitor);
    }
    void unlisten(MonitorPtr const & monitor)
    {pvaMonitor->unlisten();}
};

PvaMonitor::PvaMonitor(
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
  userPoll(false),
  userWait(false)
{
}

PvaMonitor::~PvaMonitor()
{
    destroy();
}

void PvaMonitor::checkMonitorState()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState==connectIdle) connect();
    if(connectState==connected) start();
}

// from MonitorRequester
string PvaMonitor::getRequesterName()
{
     PvaPtr yyy = pva.lock();
     if(!yyy) throw std::runtime_error("pva was destroyed");
     return yyy->getRequesterName();
}

void PvaMonitor::message(string const & message,MessageType messageType)
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    PvaPtr yyy = pva.lock();
    if(!yyy) throw std::runtime_error("pva was destroyed");
    yyy->message(message, messageType);
}

void PvaMonitor::monitorConnect(
    const Status& status,
    Monitor::shared_pointer const & monitor,
    StructureConstPtr const & structure)
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    connectStatus = status;
    this->monitor = monitor;
    if(status.isOK()) {
        pvaData = PvaMonitorData::create(structure);
        pvaData->setMessagePrefix(channel->getChannelName());
    }
    waitForConnect.signal();
    
}

void PvaMonitor::monitorEvent(MonitorPtr const & monitor)
{
    PvaMonitorRequesterPtr req = pvaMonitorRequester.lock();
    if(req) req->event(getPtrSelf());
    if(userWait) waitForEvent.signal();
}

void PvaMonitor::unlisten()
{
    destroy();
}

// from PvaMonitor
void PvaMonitor::destroy()
{
    {
        Lock xx(mutex);
        if(isDestroyed) return;
        isDestroyed = true;
    }
    if(monitor) monitor->destroy();
    monitor.reset();
}

void PvaMonitor::connect()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    issueConnect();
    Status status = waitConnect();
    if(status.isOK()) return;
    stringstream ss;
    ss << "channel " << channel->getChannelName() << " PvaMonitor::connect " << status.getMessage();
    throw std::runtime_error(ss.str());
}

void PvaMonitor::issueConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState!=connectIdle) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaMonitor already connected ";
        throw std::runtime_error(ss.str());
    }
    monitorRequester = ChannelMonitorRequester::shared_pointer(new ChannelMonitorRequester(this));
    connectState = connectActive;
    monitor = channel->createMonitor(monitorRequester,pvRequest);
}

Status PvaMonitor::waitConnect()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState!=connectActive) {
        stringstream ss;
        ss << "channel " << channel->getChannelName() << " pvaMonitor illegal connect state ";
        throw std::runtime_error(ss.str());
    }
    waitForConnect.wait();
    if(connectStatus.isOK()){
        connectState = connected;
        return Status::Ok;
    }
    connectState = connectIdle;
    return Status(Status::STATUSTYPE_ERROR,connectStatus.getMessage());
}

void PvaMonitor::setRequester(PvaMonitorRequesterPtr const & pvaMonitorrRequester)
{
    this->pvaMonitorRequester = pvaMonitorrRequester;
}

void PvaMonitor::start()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState==monitorStarted) return;
    if(connectState==connectIdle) connect();
    if(connectState!=connected) throw std::runtime_error("PvaMonitor::start illegal state");
    connectState = monitorStarted;
    monitor->start();
}


void PvaMonitor::stop()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState!=monitorStarted) return;
    connectState = connected;
    monitor->stop();
}

bool PvaMonitor::poll()
{
    checkMonitorState();
    if(connectState!=monitorStarted) throw std::runtime_error("PvaMonitor::poll illegal state");
    if(userPoll) throw std::runtime_error("PvaMonitor::poll did not release last");
    monitorElement = monitor->poll();
    if(!monitorElement) return false;
    userPoll = true;
    pvaData->setData(monitorElement);
   return true;
}

bool PvaMonitor::waitEvent(double secondsToWait)
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState!=monitorStarted) throw std::runtime_error("PvaMonitor::poll illegal state");
    if(poll()) return true;
    userWait = true;
    if(secondsToWait==0.0) {
        waitForEvent.wait();
    } else {
        waitForEvent.wait(secondsToWait);
    }
    userWait = false;
    return poll();
}

void PvaMonitor::releaseEvent()
{
    if(isDestroyed) throw std::runtime_error("pvaMonitor was destroyed");
    if(connectState!=monitorStarted) throw std::runtime_error("PvaMonitor::poll illegal state");
    if(!userPoll) throw std::runtime_error("PvaMonitor::releaseEvent did not call poll");
    userPoll = false;
    monitor->release(monitorElement);
}

PvaMonitorDataPtr PvaMonitor::getData()
{
    checkMonitorState();
    return pvaData;
}

PvaMonitorPtr PvaMonitor::create(
        PvaPtr const &pva,
        PvaChannelPtr const & pvaChannel,
        Channel::shared_pointer const & channel,
        PVStructurePtr const &pvRequest)
{
    PvaMonitorPtr epv(new PvaMonitor(pva,pvaChannel,channel,pvRequest));
    return epv;
}

}}
