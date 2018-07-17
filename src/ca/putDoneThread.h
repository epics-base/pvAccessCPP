/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.07
 */
#ifndef PutDoneThread_H
#define PutDoneThread_H
#include <queue>
#include <cadef.h>
#include <shareLib.h>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {
namespace ca {

class NotifyPutRequester;
typedef std::tr1::shared_ptr<NotifyPutRequester> NotifyPutRequesterPtr;
typedef std::tr1::weak_ptr<NotifyPutRequester> NotifyPutRequesterWPtr;


class PutDoneThread;
typedef std::tr1::shared_ptr<PutDoneThread> PutDoneThreadPtr;

class CAChannelPut;
typedef std::tr1::shared_ptr<CAChannelPut> CAChannelPutPtr;
typedef std::tr1::weak_ptr<CAChannelPut> CAChannelPutWPtr;

class NotifyPutRequester
{
public:
    ChannelPutRequester::weak_pointer channelPutRequester;
    CAChannelPutWPtr channelPut;
    bool isOnQueue;
    NotifyPutRequester() : isOnQueue(false) {}
    void setChannelPut(CAChannelPutPtr const &channelPut)
    { this->channelPut = channelPut;}
};


class PutDoneThread :
     public epicsThreadRunable
{
public:
    static PutDoneThreadPtr get();
    ~PutDoneThread();
    virtual void run();
    void start();
    void stop();
    void putDone(NotifyPutRequesterPtr const &notifyPutRequester);
private:
    PutDoneThread();
    bool isStop;
    std::tr1::shared_ptr<epicsThread> thread;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForCommand;
    epics::pvData::Event waitForStop;
    std::queue<NotifyPutRequesterWPtr> notifyPutQueue;
};


}}}

#endif  /* PutDoneThread_H */
