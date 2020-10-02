/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.07
 */

#include <epicsExit.h>
#define epicsExportSharedSymbols
#include "channelConnectThread.h"
#include "caChannel.h"

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {
namespace ca {

ChannelConnectThread::ChannelConnectThread()
: isStop(false)
{
}

ChannelConnectThread::~ChannelConnectThread()
{
    {
        Lock the(mutex);
        isStop = true;
    }
    workToDo.signal();
    thread->exitWait();
}


void ChannelConnectThread::start()
{
    if (thread) return;
    thread = std::tr1::shared_ptr<epicsThread>(new epicsThread(
        *this,
        "channelConnectThread",
        epicsThreadGetStackSize(epicsThreadStackSmall),
        epicsThreadPriorityLow));
    thread->start();
}

void ChannelConnectThread::channelConnected(
    NotifyChannelRequesterPtr const &notifyChannelRequester)
{
    {
        Lock the(mutex);
        if (notifyChannelRequester->isOnQueue) return;
        notifyChannelRequester->isOnQueue = true;
        notifyChannelQueue.push(notifyChannelRequester);
    }
    workToDo.signal();
}

void ChannelConnectThread::run()
{
    do {
        workToDo.wait();
        while (true) {
            bool more = false;
            NotifyChannelRequester* notifyChannelRequester(NULL);
            {
                Lock the(mutex);
                if (!notifyChannelQueue.empty())
                {
                    more = true;
                    NotifyChannelRequesterWPtr req(notifyChannelQueue.front());
                    notifyChannelQueue.pop();
                    NotifyChannelRequesterPtr reqPtr(req.lock());
                    if (reqPtr) {
                        notifyChannelRequester = reqPtr.get();
                        reqPtr->isOnQueue = false;
                    }
                }
            }
            if (!more) break;
            if (notifyChannelRequester)
            {
                CAChannelPtr channel(notifyChannelRequester->channel.lock());
                if (channel) channel->notifyClient();
            }
        }
    } while (!stopping());
}

}}}
