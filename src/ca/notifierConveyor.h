/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef INC_notifierConveyor_H
#define INC_notifierConveyor_H

#include <queue>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {
namespace ca {

class Notification;
typedef std::tr1::shared_ptr<Notification> NotificationPtr;
typedef std::tr1::weak_ptr<Notification> NotificationWPtr;

class NotifierClient;
typedef std::tr1::shared_ptr<NotifierClient> NotifierClientPtr;
typedef std::tr1::weak_ptr<NotifierClient> NotifierClientWPtr;

class NotifierClient
{
public:
    virtual void notifyClient() = 0;
};

class Notification
{
public:
    Notification() : queued(false) {}
    void setClient(NotifierClientPtr const &client) {
        this->client = client;
    }
private:
    NotifierClientWPtr client;
    bool queued;
    friend class NotifierConveyor;
};

class NotifierConveyor :
    public epicsThreadRunable
{
public:
    NotifierConveyor() : halt(false) {}
    ~NotifierConveyor();
    virtual void run();
    void start();
    void notifyClient(NotificationPtr const &notificationPtr);

private:
    std::tr1::shared_ptr<epicsThread> thread;
    epics::pvData::Mutex mutex;
    epics::pvData::Event workToDo;
    std::queue<NotificationWPtr> workQueue;
    bool halt;
};

}}}

#endif  /* INC_notifierConveyor_H */
