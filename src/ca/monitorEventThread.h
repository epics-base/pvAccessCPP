/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.06
 */
#ifndef MonitorEventThread_H
#define MonitorEventThread_H
#include <queue>
#include <cadef.h>
#include <shareLib.h>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {
namespace ca {

class NotifyMonitorRequester;
typedef std::tr1::shared_ptr<NotifyMonitorRequester> NotifyMonitorRequesterPtr;
typedef std::tr1::weak_ptr<NotifyMonitorRequester> NotifyMonitorRequesterWPtr;


class MonitorEventThread;
typedef std::tr1::shared_ptr<MonitorEventThread> MonitorEventThreadPtr;

class CAChannelMonitor;
typedef std::tr1::shared_ptr<CAChannelMonitor> CAChannelMonitorPtr;
typedef std::tr1::weak_ptr<CAChannelMonitor> CAChannelMonitorWPtr;

class NotifyMonitorRequester
{
public:
    MonitorRequester::weak_pointer monitorRequester;
    CAChannelMonitorWPtr channelMonitor;
    bool isOnQueue;
    NotifyMonitorRequester() : isOnQueue(false) {}
    void setChannelMonitor(CAChannelMonitorPtr const &channelMonitor)
    { this->channelMonitor = channelMonitor;}
};


class MonitorEventThread :
     public epicsThreadRunable
{
public:
    static MonitorEventThreadPtr get();
    ~MonitorEventThread();
    virtual void run();
    void start();
    void stop();
    void event(NotifyMonitorRequesterPtr const &notifyMonitorRequester);
private:
    MonitorEventThread();

    bool isStop;
    std::tr1::shared_ptr<epicsThread> thread;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForCommand;
    epics::pvData::Event waitForStop;
    std::queue<NotifyMonitorRequesterWPtr> notifyMonitorQueue;
};


}}}

#endif  /* MonitorEventThread_H */
