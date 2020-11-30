/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <queue>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

#define epicsExportSharedSymbols
#include "notifierConveyor.h"

using epics::pvData::Lock;

namespace epics {
namespace pvAccess {
namespace ca {

NotifierConveyor::~NotifierConveyor()
{
    if (thread) {
        {
            Lock the(mutex);
            halt = true;
        }
        workToDo.signal();
        thread->exitWait();
    }
}

void NotifierConveyor::start()
{
    if (thread) return;
    thread = std::tr1::shared_ptr<epicsThread>(new epicsThread(*this,
        "caProvider::clientNotifier",
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow));
    thread->start();
}

void NotifierConveyor::notifyClient(
    NotificationPtr const &notificationPtr)
{
    {
        Lock the(mutex);
        if (halt || notificationPtr->queued) return;
        notificationPtr->queued = true;
        workQueue.push(notificationPtr);
    }
    workToDo.signal();
}

void NotifierConveyor::run()
{
    bool stopping;
    do {
        workToDo.wait();
        Lock the(mutex);
        stopping = halt;
        bool work = !workQueue.empty();
        while (work)
        {
            NotificationWPtr notificationWPtr(workQueue.front());
            workQueue.pop();
            work = !workQueue.empty();
            NotificationPtr notification(notificationWPtr.lock());
            if (notification) {
                notification->queued = false;
                NotifierClientPtr client(notification->client.lock());
                if (client) {
                    the.unlock();
                    try { client->notifyClient(); }
                    catch (std::exception &e) {
                        std::cerr << "Exception from notifyClient(): "
                            << e.what() << std::endl;
                    }
                    catch (...) {
                        std::cerr << "Unknown exception from notifyClient()"
                            << std::endl;
                    }
                    if (work) {
                        the.lock();
                        stopping = halt;
                    }
                }
            }
        }
    } while (!stopping);
}

}}}
