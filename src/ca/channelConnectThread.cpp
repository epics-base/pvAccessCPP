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
#include "channelConnectThread.h"

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {
namespace ca {

ChannelConnectThreadPtr ChannelConnectThread::get()
{
    static  ChannelConnectThreadPtr master;
    static Mutex mutex;
    Lock xx(mutex);
    if(!master) {
        master = ChannelConnectThreadPtr(new ChannelConnectThread());
        master->start();
    }
    return master;
}

ChannelConnectThread::ChannelConnectThread()
: isStop(false)
{
}

ChannelConnectThread::~ChannelConnectThread()
{
//std::cout << "ChannelConnectThread::~ChannelConnectThread()\n";
}


void ChannelConnectThread::start()
{
    thread =  std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "channelConnectThread",
        epicsThreadGetStackSize(epicsThreadStackSmall),
        epicsThreadPriorityLow));
    thread->start(); 
}


void ChannelConnectThread::stop()
{
    {
        Lock xx(mutex);
        isStop = true;
    }
    waitForCommand.signal();
    waitForStop.wait();
}

void ChannelConnectThread::channelConnected(
    NotifyChannelRequesterPtr const &notifyChannelRequester)
{
    {   
        Lock lock(mutex);
        if(notifyChannelRequester->isOnQueue) return;
        notifyChannelRequester->isOnQueue = true;
        notifyChannelQueue.push(notifyChannelRequester);
    }
    waitForCommand.signal();
}

void ChannelConnectThread::run()
{
    while(true)
    {
         waitForCommand.wait();
         while(true) {
             bool more = false;
             NotifyChannelRequester* notifyChannelRequester(NULL);
             {
                 Lock lock(mutex);
                 if(!notifyChannelQueue.empty())
                 {
                      more = true;
                      NotifyChannelRequesterWPtr req(notifyChannelQueue.front());
                      notifyChannelQueue.pop();
                      NotifyChannelRequesterPtr reqPtr(req.lock());
                      if(reqPtr) {
                         notifyChannelRequester = reqPtr.get();
                         reqPtr->isOnQueue = false;
                      }
                 }
             }
             if(!more) break;
             if(notifyChannelRequester!=NULL)
             {
                 CAChannelPtr channel(notifyChannelRequester->channel.lock());
                 if(channel) channel->notifyClient();
             }
         }
         if(isStop) {
             waitForStop.signal();
             break;
         }
    }
}

}}}
