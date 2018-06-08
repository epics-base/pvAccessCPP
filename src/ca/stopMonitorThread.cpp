/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.04
 */

#include "caChannel.h"
#include <epicsExit.h>
#define epicsExportSharedSymbols
#include "stopMonitorThread.h"

using namespace epics::pvData;

namespace epics {
namespace pvAccess {
namespace ca {

StopMonitorThreadPtr StopMonitorThread::get()
{
    static  StopMonitorThreadPtr master;
    static Mutex mutex;
    Lock xx(mutex);
    if(!master) {
        master = StopMonitorThreadPtr(new StopMonitorThread());
        master->start();
    }
    return master;
}

StopMonitorThread::StopMonitorThread()
: isStop(false),
  isAttachContext(false),
  isWaitForNoEvents(false),
  current_context(NULL)
{
}

StopMonitorThread::~StopMonitorThread()
{
std::cout << "StopMonitorThread::~StopMonitorThread()\n";
}

void StopMonitorThread::attachContext(ca_client_context* current_context)
{
    Lock xx(mutex);
    isAttachContext = true;
    this->current_context = current_context;
    waitForCommand.signal();
}

void StopMonitorThread::start()
{
    thread =  std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "stopMonitorThread",
        epicsThreadGetStackSize(epicsThreadStackSmall),
        epicsThreadPriorityLow));
    thread->start();  
}

void StopMonitorThread::stop()
{
    {
        Lock xx(mutex);
        isStop = true;
    }
    waitForCommand.signal();
    waitForStop.wait();
}

void StopMonitorThread::callStop(evid pevid)
{
    {
        Lock xx(mutex);
        evidQueue.push(&(*pevid));
    }
    waitForCommand.signal();
}

void StopMonitorThread::waitForNoEvents()
{
    while(true)
    {
        {
            Lock xx(mutex);
            if(evidQueue.size()==0) return;
            isWaitForNoEvents = true;
        }
        waitForCommand.signal();
        noMoreEvents.wait();
    }
}

void StopMonitorThread::run()
{
    while(true)
    {
         waitForCommand.wait();
         Lock lock(mutex);
         if(isAttachContext)
         {
             int result = ca_attach_context(current_context);
             if(result != ECA_NORMAL) {
                 std::string mess("StopMonitorThread::run() while calling ca_attach_context ");
                 mess += ca_message(result);
                 throw  std::runtime_error(mess);
             }
             isAttachContext = false;
         }
         if(evidQueue.size()>0)
         {   
             while(!evidQueue.empty())
             {
                 evid pvid = evidQueue.front();
                 evidQueue.pop();
                 int result = ca_clear_subscription(pvid);
                 if(result!=ECA_NORMAL)
                 {
                     std::cout << "StopMonitorThread::run() ca_clear_subscription error "
                        << ca_message(result) << "\n";
                 }
             }
         }
         if(isWaitForNoEvents)
         {
              isWaitForNoEvents = false;
              noMoreEvents.signal();
         }
         if(isStop) {
             waitForStop.signal();
             break;
         }
    }
}

}}}
