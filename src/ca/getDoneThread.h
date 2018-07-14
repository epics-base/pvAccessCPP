/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.07
 */
#ifndef GetDoneThread_H
#define GetDoneThread_H
#include <queue>
#include <cadef.h>
#include <shareLib.h>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {
namespace ca {

class NotifyGetRequester;
typedef std::tr1::shared_ptr<NotifyGetRequester> NotifyGetRequesterPtr;
typedef std::tr1::weak_ptr<NotifyGetRequester> NotifyGetRequesterWPtr;


class GetDoneThread;
typedef std::tr1::shared_ptr<GetDoneThread> GetDoneThreadPtr;

class CAChannelGet;
typedef std::tr1::shared_ptr<CAChannelGet> CAChannelGetPtr;
typedef std::tr1::weak_ptr<CAChannelGet> CAChannelGetWPtr;

class NotifyGetRequester
{
public:
    ChannelGetRequester::weak_pointer channelGetRequester;
    CAChannelGetWPtr channelGet;
    bool isOnQueue;
    NotifyGetRequester() : isOnQueue(false) {}
    void setChannelGet(CAChannelGetPtr const &channelGet)
    { this->channelGet = channelGet;}
};


class GetDoneThread :
     public epicsThreadRunable
{
public:
    static GetDoneThreadPtr get();
    ~GetDoneThread();
    virtual void run();
    void start();
    void stop();
    void getDone(NotifyGetRequesterPtr const &notifyGetRequester);
private:
    GetDoneThread();

    bool isStop;
    std::tr1::shared_ptr<epicsThread> thread;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForCommand;
    epics::pvData::Event waitForStop;
    std::queue<NotifyGetRequesterWPtr> notifyGetQueue;
};


}}}

#endif  /* GetDoneThread_H */
