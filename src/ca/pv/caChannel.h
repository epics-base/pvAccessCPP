/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CACHANNEL_H
#define CACHANNEL_H

#include <pv/pvAccess.h>

/* for CA */
#include <cadef.h>

#include <pv/caProvider.h>

namespace epics {
namespace pvAccess {
namespace ca {

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

    /* --------------- epics::pvAccess::Channel --------------- */

    virtual std::tr1::shared_ptr<ChannelProvider> getProvider();
    virtual std::string getRemoteAddress();
    virtual ConnectionState getConnectionState();
    virtual std::string getChannelName();
    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester();

    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField);

    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField);

    virtual ChannelProcess::shared_pointer createChannelProcess(
        ChannelProcessRequester::shared_pointer const & channelProcessRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ChannelGet::shared_pointer createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ChannelPut::shared_pointer createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
        ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ChannelRPC::shared_pointer createChannelRPC(
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual epics::pvData::Monitor::shared_pointer createMonitor(
        epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ChannelArray::shared_pointer createChannelArray(
        ChannelArrayRequester::shared_pointer const & channelArrayRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual void printInfo(std::ostream& out);

    /* --------------- epics::pvData::Destroyable --------------- */

    virtual void destroy();

    /* ---------------------------------------------------------------- */

    void threadAttach();

    void registerRequest(ChannelRequest::shared_pointer const & request);
    void unregisterRequest(ChannelRequest::shared_pointer const & request);

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
    // TODO std::unordered_map
    // void* is not the nicest thing, but there is no fast weak_ptr==
    typedef std::map<void*, ChannelRequest::weak_pointer> RequestsList;
    RequestsList requests;
};



class CAChannelGet :
    public ChannelGet,
    public std::tr1::enable_shared_from_this<CAChannelGet>
{

public:
    POINTER_DEFINITIONS(CAChannelGet);

    static ChannelGet::shared_pointer create(CAChannel::shared_pointer const & channel,
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

    /* --------------- epics::pvData::Destroyable --------------- */

    virtual void destroy();

    /* --------------- epics::pvData::Lockable --------------- */

    virtual void lock();
    virtual void unlock();

private:

    CAChannelGet(CAChannel::shared_pointer const & _channel,
                 ChannelGetRequester::shared_pointer const & _channelGetRequester,
                 epics::pvData::PVStructure::shared_pointer const & pvRequest);
    void activate();

    CAChannel::shared_pointer channel;
    ChannelGetRequester::shared_pointer channelGetRequester;
    chtype getType;

    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;

    // TODO AtomicBoolean !!!
    bool lastRequestFlag;
};



class CAChannelPut :
    public ChannelPut,
    public std::tr1::enable_shared_from_this<CAChannelPut>
{

public:
    POINTER_DEFINITIONS(CAChannelPut);

    static ChannelPut::shared_pointer create(CAChannel::shared_pointer const & channel,
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

    /* --------------- epics::pvData::Destroyable --------------- */

    virtual void destroy();

    /* --------------- epics::pvData::Lockable --------------- */

    virtual void lock();
    virtual void unlock();

private:

    CAChannelPut(CAChannel::shared_pointer const & _channel,
                 ChannelPutRequester::shared_pointer const & _channelPutRequester,
                 epics::pvData::PVStructure::shared_pointer const & pvRequest);
    void activate();

    CAChannel::shared_pointer channel;
    ChannelPutRequester::shared_pointer channelPutRequester;
    chtype getType;

    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer bitSet;

    // TODO AtomicBoolean !!!
    bool lastRequestFlag;
};


class CAChannelMonitor :
    public epics::pvData::Monitor,
    public std::tr1::enable_shared_from_this<CAChannelMonitor>
{

public:
    POINTER_DEFINITIONS(CAChannelMonitor);

    static epics::pvData::Monitor::shared_pointer create(CAChannel::shared_pointer const & channel,
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ~CAChannelMonitor();

    void subscriptionEvent(struct event_handler_args &args);

    /* --------------- epics::pvData::Monitor --------------- */

    virtual epics::pvData::Status start();
    virtual epics::pvData::Status stop();
    virtual epics::pvData::MonitorElementPtr poll();
    virtual void release(epics::pvData::MonitorElementPtr const & monitorElement);

    /* --------------- epics::pvData::ChannelRequest --------------- */

    virtual void cancel();

    /* --------------- epics::pvData::Destroyable --------------- */

    virtual void destroy();

private:

    CAChannelMonitor(CAChannel::shared_pointer const & _channel,
                     epics::pvData::MonitorRequester::shared_pointer const & _monitorRequester,
                     epics::pvData::PVStructure::shared_pointer const & pvRequest);
    void activate();

    CAChannel::shared_pointer channel;
    epics::pvData::MonitorRequester::shared_pointer monitorRequester;
    chtype getType;

    epics::pvData::PVStructure::shared_pointer pvStructure;
    epics::pvData::BitSet::shared_pointer changedBitSet;
    epics::pvData::BitSet::shared_pointer overrunBitSet;
    evid eventID;

    epics::pvData::Mutex mutex;
    int count;

    epics::pvData::MonitorElement::shared_pointer element;
    epics::pvData::MonitorElement::shared_pointer nullElement;

    // TODO remove
    Monitor::shared_pointer thisPointer;
};

}
}
}

#endif  /* CACHANNEL_H */
