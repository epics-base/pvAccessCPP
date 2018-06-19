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
std::cout << "MonitorEventThread::~MonitorEventThread()\n";
}

void MonitorEventThread::start()
{
    thread =  std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "monitorEventThread",
        epicsThreadGetStackSize(epicsThreadStackSmall),
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


void MonitorEventThread::event(NotifyRequesterPtr const &stopMonitor)
{
    {   
        Lock lock(mutex);
        if(stopMonitor->isOnQueue) return;
        stopMonitor->isOnQueue = true;
        notifyMonitorQueue.push(stopMonitor);
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
             NotifyRequester* notifyRequester(NULL);
             {
                 Lock lock(mutex);
                 if(!notifyMonitorQueue.empty())
                 {
                      more = true;
                      NotifyRequesterWPtr req(notifyMonitorQueue.front());
                      notifyMonitorQueue.pop();
                      NotifyRequesterPtr reqPtr(req.lock());
                      if(reqPtr) {
                         notifyRequester = reqPtr.get();
                         reqPtr->isOnQueue = false;
                      }
                 }
             }
             if(!more) break;
             if(notifyRequester!=NULL)
             {
                 CAChannelMonitorPtr channelMonitor(notifyRequester->channelMonitor.lock());
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
