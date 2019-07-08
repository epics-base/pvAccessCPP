/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.06
 */

#include "caChannel.h"
#include <epicsExit.h>
#define epicsExportSharedSymbols
#include "monitorEventThread.h"

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {
namespace ca {

MonitorEventThreadPtr MonitorEventThread::get()
{
    static  MonitorEventThreadPtr master;
    static Mutex mutex;
    Lock xx(mutex);
    if(!master) {
        master = MonitorEventThreadPtr(new MonitorEventThread());
        master->start();
    }
    return master;
}

MonitorEventThread::MonitorEventThread()
: isStop(false)
{
}

MonitorEventThread::~MonitorEventThread()
{
//std::cout << "MonitorEventThread::~MonitorEventThread()\n";
}

void MonitorEventThread::start()
{
    thread =  std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "monitorEventThread",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow));
    thread->start();  
}

void MonitorEventThread::stop()
{
    {
        Lock xx(mutex);
        isStop = true;
    }
    waitForCommand.signal();
    waitForStop.wait();
}


void MonitorEventThread::event(NotifyMonitorRequesterPtr const &notifyMonitorRequester)
{
    {   
        Lock lock(mutex);
        if(notifyMonitorRequester->isOnQueue) return;
        notifyMonitorRequester->isOnQueue = true;
        notifyMonitorQueue.push(notifyMonitorRequester);
    }
    waitForCommand.signal();
}

void MonitorEventThread::run()
{
    while(true)
    {
         waitForCommand.wait();
         while(true) {
             bool more = false;
             NotifyMonitorRequester* notifyMonitorRequester(NULL);
             {
                 Lock lock(mutex);
                 if(!notifyMonitorQueue.empty())
                 {
                      more = true;
                      NotifyMonitorRequesterWPtr req(notifyMonitorQueue.front());
                      notifyMonitorQueue.pop();
                      NotifyMonitorRequesterPtr reqPtr(req.lock());
                      if(reqPtr) {
                         notifyMonitorRequester = reqPtr.get();
                         reqPtr->isOnQueue = false;
                      }
                 }
             }
             if(!more) break;
             if(notifyMonitorRequester!=NULL)
             {
                 CAChannelMonitorPtr channelMonitor(notifyMonitorRequester->channelMonitor.lock());
                 if(channelMonitor) channelMonitor->notifyClient();
             }
         }
         if(isStop) {
             waitForStop.signal();
             break;
         }
    }
}

}}}
