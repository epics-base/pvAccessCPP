/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.07
 */
#ifndef ChannelConnectThread_H
#define ChannelConnectThread_H
#include <queue>
#include <cadef.h>
#include <shareLib.h>
#include <epicsThread.h>
#include <pv/event.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {
namespace ca {

class NotifyChannelRequester;
typedef std::tr1::shared_ptr<NotifyChannelRequester> NotifyChannelRequesterPtr;
typedef std::tr1::weak_ptr<NotifyChannelRequester> NotifyChannelRequesterWPtr;


class ChannelConnectThread;
typedef std::tr1::shared_ptr<ChannelConnectThread> ChannelConnectThreadPtr;

class CAChannel;
typedef std::tr1::shared_ptr<CAChannel> CAChannelPtr;
typedef std::tr1::weak_ptr<CAChannel> CAChannelWPtr;

class NotifyChannelRequester
{
public:
    ChannelRequester::weak_pointer channelRequester;
    CAChannelWPtr channel;
    bool isOnQueue;
    NotifyChannelRequester() : isOnQueue(false) {}
    void setChannel(CAChannelPtr const &channel)
    { this->channel = channel;}
};


class ChannelConnectThread :
     public epicsThreadRunable
{
public:
    static ChannelConnectThreadPtr get();
    ~ChannelConnectThread();
    virtual void run();
    void start();
    void stop();
    void channelConnected(NotifyChannelRequesterPtr const &notifyChannelRequester);
private:
    ChannelConnectThread();

    bool isStop;
    std::tr1::shared_ptr<epicsThread> thread;
    epics::pvData::Mutex mutex;
    epics::pvData::Event waitForCommand;
    epics::pvData::Event waitForStop;
    std::queue<NotifyChannelRequesterWPtr> notifyChannelQueue;
};


}}}

#endif  /* ChannelConnectThread_H */
