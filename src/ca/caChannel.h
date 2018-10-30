/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

/**
 * @author msekoranja, mrk
 * @date 2018.07
 */

#ifndef CACHANNEL_H
#define CACHANNEL_H

#include <queue>
#include <vector>

#include <pv/pvAccess.h>
#include <pv/event.h>

/* for CA */
#include <cadef.h>

#include "caProviderPvt.h"
#include "dbdToPv.h"

namespace epics {
namespace pvAccess {
namespace ca {

class CAChannel;
typedef std::tr1::shared_ptr<CAChannel> CAChannelPtr;
typedef std::tr1::weak_ptr<CAChannel> CAChannelWPtr;
class ChannelConnectThread;
typedef std::tr1::shared_ptr<ChannelConnectThread> ChannelConnectThreadPtr;

class NotifyChannelRequester;
typedef std::tr1::shared_ptr<NotifyChannelRequester> NotifyChannelRequesterPtr;

class NotifyMonitorRequester;
typedef std::tr1::shared_ptr<NotifyMonitorRequester> NotifyMonitorRequesterPtr;
class MonitorEventThread;
typedef std::tr1::shared_ptr<MonitorEventThread> MonitorEventThreadPtr;

class NotifyGetRequester;
typedef std::tr1::shared_ptr<NotifyGetRequester> NotifyGetRequesterPtr;
typedef std::tr1::weak_ptr<NotifyGetRequester> NotifyGetRequesterWPtr;
class GetDoneThread;
typedef std::tr1::shared_ptr<GetDoneThread> GetDoneThreadPtr;

class NotifyPutRequester;
typedef std::tr1::shared_ptr<NotifyPutRequester> NotifyPutRequesterPtr;
typedef std::tr1::weak_ptr<NotifyPutRequester> NotifyPutRequesterWPtr;
class PutDoneThread;
typedef std::tr1::shared_ptr<PutDoneThread> PutDoneThreadPtr;

class CAChannelGetField;
typedef std::tr1::shared_ptr<CAChannelGetField> CAChannelGetFieldPtr;
typedef std::tr1::weak_ptr<CAChannelGetField> CAChannelGetFieldWPtr;
class CAChannelPut;
typedef std::tr1::shared_ptr<CAChannelPut> CAChannelPutPtr;
typedef std::tr1::weak_ptr<CAChannelPut> CAChannelPutWPtr;
class CAChannelGet;
typedef std::tr1::shared_ptr<CAChannelGet> CAChannelGetPtr;
typedef std::tr1::weak_ptr<CAChannelGet> CAChannelGetWPtr;
class CAChannelMonitor;
typedef std::tr1::shared_ptr<CAChannelMonitor> CAChannelMonitorPtr;
typedef std::tr1::weak_ptr<CAChannelMonitor> CAChannelMonitorWPtr;

class CAChannelGetField :
    public std::tr1::enable_shared_from_this<CAChannelGetField>
{
public:
    POINTER_DEFINITIONS(CAChannelGetField);
    CAChannelGetField(
        CAChannelPtr const &channel,
        GetFieldRequester::shared_pointer const & requester,
        std::string const & subField);
    ~CAChannelGetField();
    void callRequester(CAChannelPtr const & caChannel);
    void activate();
private:
    CAChannelWPtr channel;
    GetFieldRequester::weak_pointer getFieldRequester;
    std::string subField;
};

class CAChannel :
    public Channel,
    public std::tr1::enable_shared_from_this<CAChannel>
{
public:
    POINTER_DEFINITIONS(CAChannel);
    static CAChannelPtr create(
        CAChannelProvider::shared_pointer const & channelProvider,
        std::string const & channelName,
        short priority,
        ChannelRequester::shared_pointer const & channelRequester);
    virtual ~CAChannel();
    chid getChannelID();

    virtual std::tr1::shared_ptr<ChannelProvider> getProvider();
    virtual std::string getRemoteAddress();
    virtual ConnectionState getConnectionState();
    virtual std::string getChannelName();
    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester();
    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField);
    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField);
    virtual ChannelGet::shared_pointer createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructurePtr const & pvRequest);
    virtual ChannelPut::shared_pointer createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructurePtr const & pvRequest);
    virtual Monitor::shared_pointer createMonitor(
        MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructurePtr const & pvRequest);
    virtual void printInfo(std::ostream& out);

    void attachContext();
    void disconnectChannel();
    void connect(bool isConnected);
    void notifyClient();
private:
    virtual void destroy() {}
    CAChannel(std::string const & channelName,
              CAChannelProvider::shared_pointer const & channelProvider,
              ChannelRequester::shared_pointer const & channelRequester);
    void activate(short priority);
    void addMonitor(CAChannelMonitorPtr const & monitor);

    std::string channelName;
    CAChannelProviderWPtr channelProvider;
    ChannelRequester::weak_pointer channelRequester;
    chid channelID;
    bool channelCreated;
    bool channelConnected;
    ChannelConnectThreadPtr channelConnectThread;
    NotifyChannelRequesterPtr notifyChannelRequester;

    epics::pvData::Mutex requestsMutex;
    std::queue<CAChannelGetFieldPtr> getFieldQueue;
    std::queue<CAChannelPutPtr> putQueue;
    std::queue<CAChannelGetPtr> getQueue;
    std::queue<CAChannelMonitorPtr> monitorQueue;
    std::vector<CAChannelMonitorWPtr> monitorlist;
};


class CAChannelGet :
    public ChannelGet,
    public std::tr1::enable_shared_from_this<CAChannelGet>
{
public:
    POINTER_DEFINITIONS(CAChannelGet);
    static CAChannelGet::shared_pointer create(CAChannel::shared_pointer const & channel,
            ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructurePtr const & pvRequest);
    virtual ~CAChannelGet();
    void getDone(struct event_handler_args &args);
    virtual void get();
    virtual Channel::shared_pointer getChannel();
    virtual void cancel();
    virtual void lastRequest();
    virtual std::string getRequesterName();

    void activate();
    void notifyClient();
private:
    virtual void destroy() {}
    CAChannelGet(CAChannel::shared_pointer const & _channel,
                 ChannelGetRequester::shared_pointer const & _channelGetRequester,
                 epics::pvData::PVStructurePtr const & pvRequest);
    
    CAChannelPtr channel;
    ChannelGetRequester::weak_pointer channelGetRequester;
    const epics::pvData::PVStructure::shared_pointer pvRequest;
    epics::pvData::Status getStatus;
    GetDoneThreadPtr getDoneThread;
    NotifyGetRequesterPtr notifyGetRequester;
    DbdToPvPtr dbdToPv;
    epics::pvData::Mutex mutex;
    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;
};

class CAChannelPut :
    public ChannelPut,
    public std::tr1::enable_shared_from_this<CAChannelPut>
{

public:
    POINTER_DEFINITIONS(CAChannelPut);
    static CAChannelPut::shared_pointer create(CAChannel::shared_pointer const & channel,
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructurePtr const & pvRequest);
    virtual ~CAChannelPut();
    void putDone(struct event_handler_args &args);
    void getDone(struct event_handler_args &args);
    virtual void put(
        epics::pvData::PVStructure::shared_pointer const & pvPutStructure,
        epics::pvData::BitSet::shared_pointer const & putBitSet
    );
    virtual void get();
    virtual Channel::shared_pointer getChannel();
    virtual void cancel();
    virtual void lastRequest();

    virtual std::string getRequesterName();
    void activate();
    void notifyClient();
private:
    virtual void destroy() {}
    CAChannelPut(CAChannel::shared_pointer const & _channel,
                 ChannelPutRequester::shared_pointer const & _channelPutRequester,
                 epics::pvData::PVStructurePtr const & pvRequest);
    CAChannelPtr channel;
    ChannelPutRequester::weak_pointer channelPutRequester;
    const epics::pvData::PVStructure::shared_pointer pvRequest;
    bool block;
    bool isPut;
    epics::pvData::Status getStatus;
    epics::pvData::Status putStatus;
    PutDoneThreadPtr putDoneThread;
    NotifyPutRequesterPtr notifyPutRequester;
    DbdToPvPtr dbdToPv;
    epics::pvData::Mutex mutex;
    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;
};

class CACMonitorQueue;
typedef std::tr1::shared_ptr<CACMonitorQueue> CACMonitorQueuePtr;

class CAChannelMonitor :
    public Monitor,
    public std::tr1::enable_shared_from_this<CAChannelMonitor>
{

public:
    POINTER_DEFINITIONS(CAChannelMonitor);
    static CAChannelMonitor::shared_pointer create(CAChannel::shared_pointer const & channel,
            MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructurePtr const & pvRequest);
    virtual ~CAChannelMonitor();
    void subscriptionEvent(struct event_handler_args &args);

    virtual epics::pvData::Status start();
    virtual epics::pvData::Status stop();
    virtual MonitorElementPtr poll();
    virtual void release(MonitorElementPtr const & monitorElement);
    virtual void cancel();
    virtual std::string getRequesterName();
    void activate();
    void notifyClient();
private:
    virtual void destroy() {}
    CAChannelMonitor(CAChannel::shared_pointer const & _channel,
                     MonitorRequester::shared_pointer const & _monitorRequester,
                     epics::pvData::PVStructurePtr const & pvRequest);
    CAChannelPtr channel;
    MonitorRequester::weak_pointer monitorRequester;
    const epics::pvData::PVStructure::shared_pointer pvRequest;
    bool isStarted;
    MonitorEventThreadPtr monitorEventThread;
    evid pevid;
    unsigned long eventMask;
    NotifyMonitorRequesterPtr notifyMonitorRequester;

    DbdToPvPtr dbdToPv;
    epics::pvData::Mutex mutex;
    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::MonitorElementPtr activeElement;
    
    CACMonitorQueuePtr monitorQueue;
};

}}}

#endif  /* CACHANNEL_H */
