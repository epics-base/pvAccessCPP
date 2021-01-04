/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <queue>
#include <cstdio>
#include <cantProceed.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <epicsGuard.h>     // Needed for 3.15 builds
#include <epicsEvent.h>
#include <pv/sharedPtr.h>

#define epicsExportSharedSymbols
#include "notifierConveyor.h"

namespace epics {
namespace pvAccess {
namespace ca {

NotifierConveyor::~NotifierConveyor()
{
    if (thread) {
        if (thread->isCurrentThread()) {
            cantProceed("NotifierConveyor: Can't delete me in notify()!\n");
        }
        {
            epicsGuard<epicsMutex> G(mutex);
            halt = true;
        }
        workToDo.trigger();
        thread->exitWait();
    }
}

void NotifierConveyor::start()
{
    if (thread) return;
    char name[40];
    std::sprintf(name, "pva::ca::conveyor %p", this);
    thread = std::tr1::shared_ptr<epicsThread>(new epicsThread(*this, name,
        epicsThreadGetStackSize(epicsThreadStackBig),
        epicsThreadPriorityLow));
    thread->start();
}

void NotifierConveyor::notifyClient(
    NotificationPtr const &notificationPtr)
{
    {
        epicsGuard<epicsMutex> G(mutex);
        if (halt || notificationPtr->queued) return;
        notificationPtr->queued = true;
        workQueue.push(notificationPtr);
    }
    workToDo.trigger();
}

void NotifierConveyor::run()
{
    bool stopping;
    do {
        workToDo.wait();
        epicsGuard<epicsMutex> G(mutex);
        stopping = halt;
        while (!stopping && !workQueue.empty()) {
            NotificationWPtr notificationWPtr(workQueue.front());
            workQueue.pop();
            NotificationPtr notification(notificationWPtr.lock());
            if (notification) {
                notification->queued = false;
                epicsGuardRelease<epicsMutex> U(G);
                NotifierClientPtr client(notification->client.lock());
                if (client) {
                    try { client->notifyClient(); }
                    catch (std::exception &e) {
                        std::cerr << "Exception from notifyClient(): "
                            << e.what() << std::endl;
                    }
                    catch (...) {
                        std::cerr << "Unknown exception from notifyClient()"
                            << std::endl;
                    }
                }
            }
            stopping = halt;
        }
    } while (!stopping);
}

}}}
