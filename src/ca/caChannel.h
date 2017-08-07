/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CACHANNEL_H
#define CACHANNEL_H

#include <queue>
#include <vector>

#include <pv/pvAccess.h>


/* for CA */
#include <cadef.h>

#include "caProviderPvt.h"

namespace epics {
namespace pvAccess {
namespace ca {

class CAChannel;
typedef std::tr1::shared_ptr<CAChannel> CAChannelPtr;
typedef std::tr1::weak_ptr<CAChannel> CAChannelWPtr;
class CAChannelPut;
typedef std::tr1::shared_ptr<CAChannelPut> CAChannelPutPtr;
typedef std::tr1::weak_ptr<CAChannelPut> CAChannelPutWPtr;
class CAChannelGet;
typedef std::tr1::shared_ptr<CAChannelGet> CAChannelGetPtr;
typedef std::tr1::weak_ptr<CAChannelGet> CAChannelGetWPtr;
class CAChannelMonitor;
typedef std::tr1::shared_ptr<CAChannelMonitor> CAChannelMonitorPtr;
typedef std::tr1::weak_ptr<CAChannelMonitor> CAChannelMonitorWPtr;

typedef std::tr1::shared_ptr<ChannelBaseRequester> ChannelBaseRequesterPtr;
typedef std::tr1::weak_ptr<ChannelBaseRequester> ChannelBaseRequesterWPtr;

class CAChannel :
    public Channel,
    public std::tr1::enable_shared_from_this<CAChannel>
{

public:
    POINTER_DEFINITIONS(CAChannel);

    static shared_pointer create(CAChannelProvider::shared_pointer const & channelProvider,
                                 std::string const & channelName,
                                 short priority,
                                 ChannelRequester::shared_pointer const & channelRequester);

    virtual ~CAChannel();

    void connected();
    void disconnected();

    chid getChannelID();
    chtype getNativeType();
    unsigned getElementCount();
    epics::pvData::Structure::const_shared_pointer getStructure();

    /* --------------- epics::pvAccess::Channel --------------- */

    virtual std::tr1::shared_ptr<ChannelProvider> getProvider();
    virtual std::string getRemoteAddress();
    virtual ConnectionState getConnectionState();
    virtual std::string getChannelName();
    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester();

    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField);

    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField);

    virtual ChannelGet::shared_pointer createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ChannelPut::shared_pointer createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual Monitor::shared_pointer createMonitor(
        MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual void printInfo(std::ostream& out);

    /* --------------- Destroyable --------------- */

    virtual void destroy();

    /* ---------------------------------------------------------------- */

    void threadAttach();

    void addChannelGet(const CAChannelGetPtr & get);
    void addChannelPut(const CAChannelPutPtr & get);
    void addChannelMonitor(const CAChannelMonitorPtr & get);


private:

    CAChannel(std::string const & channelName,
              CAChannelProvider::shared_pointer const & channelProvider,
              ChannelRequester::shared_pointer const & channelRequester);
    void activate(short priority);

    std::string channelName;

    CAChannelProvider::shared_pointer channelProvider;
    ChannelRequester::shared_pointer channelRequester;

    chid channelID;
    chtype channelType;
    unsigned elementCount;
    epics::pvData::Structure::const_shared_pointer structure;

    epics::pvData::Mutex requestsMutex;

    bool destroyed;
    std::queue<CAChannelPutPtr> putQueue;
    std::queue<CAChannelGetPtr> getQueue;
    std::queue<CAChannelMonitorPtr> monitorQueue;
    std::vector<CAChannelGetWPtr> getList;
    std::vector<CAChannelPutWPtr> putList;
    std::vector<CAChannelMonitorWPtr> monitorList;
};


class CAChannelGet :
    public ChannelGet,
    public ChannelRequester,
    public ChannelBaseRequester,
    public std::tr1::enable_shared_from_this<CAChannelGet>
{

public:
    POINTER_DEFINITIONS(CAChannelGet);

    static CAChannelGet::shared_pointer create(CAChannel::shared_pointer const & channel,
            ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ~CAChannelGet();

    void getDone(struct event_handler_args &args);

    /* --------------- epics::pvAccess::ChannelGet --------------- */
    virtual void get();

    /* --------------- epics::pvData::ChannelRequest --------------- */
    virtual Channel::shared_pointer getChannel();
    virtual void cancel();
    virtual void lastRequest();

    /* --------------- ChannelRequester --------------- */
    virtual void channelCreated(
         const epics::pvData::Status& status,
         Channel::shared_pointer const & channel);
    virtual void channelStateChange(
         Channel::shared_pointer const & channel,
         Channel::ConnectionState connectionState);
    virtual std::string getRequesterName() { return "CAChannelGet";}
    /* --------------- ChannelBaseRequester --------------- */
    virtual void channelDisconnect(bool destroy);
   /* --------------- Destroyable --------------- */
    virtual void destroy();

    void activate();

private:

    CAChannelGet(CAChannel::shared_pointer const & _channel,
                 ChannelGetRequester::shared_pointer const & _channelGetRequester,
                 epics::pvData::PVStructure::shared_pointer const & pvRequest);
    
    CAChannel::shared_pointer channel;
    ChannelGetRequester::shared_pointer channelGetRequester;
    epics::pvData::PVStructure::shared_pointer pvRequest;
    bool lastRequestFlag;

    chtype getType;
    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;
};



class CAChannelPut :
    public ChannelPut,
    public ChannelRequester,
    public ChannelBaseRequester,
    public std::tr1::enable_shared_from_this<CAChannelPut>
{

public:
    POINTER_DEFINITIONS(CAChannelPut);

    static CAChannelPut::shared_pointer create(CAChannel::shared_pointer const & channel,
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ~CAChannelPut();

    void putDone(struct event_handler_args &args);
    void getDone(struct event_handler_args &args);

    /* --------------- epics::pvAccess::ChannelPut --------------- */

    virtual void put(
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet
    );
    virtual void get();

    /* --------------- epics::pvData::ChannelRequest --------------- */

    virtual Channel::shared_pointer getChannel();
    virtual void cancel();
    virtual void lastRequest();

    /* --------------- ChannelRequester --------------- */
    virtual void channelCreated(
         const epics::pvData::Status& status,
         Channel::shared_pointer const & channel);
    virtual void channelStateChange(
         Channel::shared_pointer const & channel,
         Channel::ConnectionState connectionState);
    virtual std::string getRequesterName() { return "CAChannelPut";}
    /* --------------- ChannelBaseRequester --------------- */
    virtual void channelDisconnect(bool destroy);
    /* --------------- Destroyable --------------- */

    virtual void destroy();

     void activate();

private:

    CAChannelPut(CAChannel::shared_pointer const & _channel,
                 ChannelPutRequester::shared_pointer const & _channelPutRequester,
                 epics::pvData::PVStructure::shared_pointer const & pvRequest);
   
    CAChannel::shared_pointer channel;
    ChannelPutRequester::shared_pointer channelPutRequester;
    epics::pvData::PVStructure::shared_pointer pvRequest;
    bool block;
    bool lastRequestFlag;

    chtype getType;
    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;
};

class CACMonitorQueue;
typedef std::tr1::shared_ptr<CACMonitorQueue> CACMonitorQueuePtr;

class CAChannelMonitor :
    public Monitor,
    public ChannelRequester,
    public ChannelBaseRequester,
    public std::tr1::enable_shared_from_this<CAChannelMonitor>
{

public:
    POINTER_DEFINITIONS(CAChannelMonitor);

    static CAChannelMonitor::shared_pointer create(CAChannel::shared_pointer const & channel,
            MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ~CAChannelMonitor();

    void subscriptionEvent(struct event_handler_args &args);

    /* --------------- Monitor --------------- */

    virtual epics::pvData::Status start();
    virtual epics::pvData::Status stop();
    virtual MonitorElementPtr poll();
    virtual void release(MonitorElementPtr const & monitorElement);

    /* --------------- epics::pvData::ChannelRequest --------------- */
    virtual void cancel();
    /* --------------- ChannelRequester --------------- */
    virtual void channelCreated(
         const epics::pvData::Status& status,
         Channel::shared_pointer const & channel);
    virtual void channelStateChange(
         Channel::shared_pointer const & channel,
         Channel::ConnectionState connectionState);
    virtual std::string getRequesterName() { return "CAChannelMonitor";}
    /* --------------- ChannelBaseRequester --------------- */
    virtual void channelDisconnect(bool destroy);
    /* --------------- Destroyable --------------- */
    virtual void destroy();
    void activate();
private:

    CAChannelMonitor(CAChannel::shared_pointer const & _channel,
                     MonitorRequester::shared_pointer const & _monitorRequester,
                     epics::pvData::PVStructure::shared_pointer const & pvRequest);
    

    CAChannelPtr channel;
    MonitorRequester::shared_pointer monitorRequester;
    epics::pvData::PVStructure::shared_pointer pvRequest;
    bool isStarted;
    chtype getType;

    epics::pvData::PVStructure::shared_pointer pvStructure;
    evid eventID;
    CACMonitorQueuePtr monitorQueue;
};

}
}
}

#endif  /* CACHANNEL_H */
