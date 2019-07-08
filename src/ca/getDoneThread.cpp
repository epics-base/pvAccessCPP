/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.07
 */

#include "caChannel.h"
#include <epicsExit.h>
#define epicsExportSharedSymbols
#include "getDoneThread.h"

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {
namespace ca {

GetDoneThreadPtr GetDoneThread::get()
{
    static  GetDoneThreadPtr master;
    static Mutex mutex;
    Lock xx(mutex);
    if(!master) {
        master = GetDoneThreadPtr(new GetDoneThread());
        master->start();
    }
    return master;
}

GetDoneThread::GetDoneThread()
: isStop(false)
{
}

GetDoneThread::~GetDoneThread()
{
//std::cout << "GetDoneThread::~GetDoneThread()\n";
}


void GetDoneThread::start()
{
    thread =  std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "getDoneThread",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow));
    thread->start(); 
}


void GetDoneThread::stop()
{
    {
        Lock xx(mutex);
        isStop = true;
    }
    waitForCommand.signal();
    waitForStop.wait();
}

void GetDoneThread::getDone(NotifyGetRequesterPtr const &notifyGetRequester)
{
    {   
        Lock lock(mutex);
        if(notifyGetRequester->isOnQueue) return;
        notifyGetRequester->isOnQueue = true;
        notifyGetQueue.push(notifyGetRequester);
    }
    waitForCommand.signal();
}

void GetDoneThread::run()
{
    while(true)
    {
         waitForCommand.wait();
         while(true) {
             bool more = false;
             NotifyGetRequester* notifyGetRequester(NULL);
             {
                 Lock lock(mutex);
                 if(!notifyGetQueue.empty())
                 {
                      more = true;
                      NotifyGetRequesterWPtr req(notifyGetQueue.front());
                      notifyGetQueue.pop();
                      NotifyGetRequesterPtr reqPtr(req.lock());
                      if(reqPtr) {
                         notifyGetRequester = reqPtr.get();
                         reqPtr->isOnQueue = false;
                      }
                 }
             }
             if(!more) break;
             if(notifyGetRequester!=NULL)
             {
                 CAChannelGetPtr channelGet(notifyGetRequester->channelGet.lock());
                 if(channelGet) channelGet->notifyClient();
             }
         }
         if(isStop) {
             waitForStop.signal();
             break;
         }
    }
}

}}}
