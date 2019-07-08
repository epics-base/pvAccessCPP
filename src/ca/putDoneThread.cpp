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
#include "putDoneThread.h"

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {
namespace ca {

PutDoneThreadPtr PutDoneThread::get()
{
    static  PutDoneThreadPtr master;
    static Mutex mutex;
    Lock xx(mutex);
    if(!master) {
        master = PutDoneThreadPtr(new PutDoneThread());
        master->start();
    }
    return master;
}

PutDoneThread::PutDoneThread()
: isStop(false)
{
}

PutDoneThread::~PutDoneThread()
{
//std::cout << "PutDoneThread::~PutDoneThread()\n";
}


void PutDoneThread::start()
{
    thread =  std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "putDoneThread",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow));
    thread->start(); 
}


void PutDoneThread::stop()
{
    {
        Lock xx(mutex);
        isStop = true;
    }
    waitForCommand.signal();
    waitForStop.wait();
}

void PutDoneThread::putDone(NotifyPutRequesterPtr const &notifyPutRequester)
{
    {   
        Lock lock(mutex);
        if(notifyPutRequester->isOnQueue) return;
        notifyPutRequester->isOnQueue = true;
        notifyPutQueue.push(notifyPutRequester);
    }
    waitForCommand.signal();
}

void PutDoneThread::run()
{
    while(true)
    {
         waitForCommand.wait();
         while(true) {
             bool more = false;
             NotifyPutRequester* notifyPutRequester(NULL);
             {
                 Lock lock(mutex);
                 if(!notifyPutQueue.empty())
                 {
                      more = true;
                      NotifyPutRequesterWPtr req(notifyPutQueue.front());
                      notifyPutQueue.pop();
                      NotifyPutRequesterPtr reqPtr(req.lock());
                      if(reqPtr) {
                         notifyPutRequester = reqPtr.get();
                         reqPtr->isOnQueue = false;
                      }
                 }
             }
             if(!more) break;
             if(notifyPutRequester!=NULL)
             {
                 CAChannelPutPtr channelPut(notifyPutRequester->channelPut.lock());
                 if(channelPut) channelPut->notifyClient();
             }
         }
         if(isStop) {
             waitForStop.signal();
             break;
         }
    }
}

}}}
