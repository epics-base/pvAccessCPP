/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <iostream>
#include <sstream>
#include <memory>
#include <queue>
#include <stdexcept>

#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/bitSetUtil.h>
#include <pv/queue.h>
#include <pv/standardPVField.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/pvaConstants.h>
#include <pv/blockingUDP.h>
#include <pv/blockingTCP.h>
#include <pv/namedLockPattern.h>
#include <pv/inetAddressUtil.h>
#include <pv/hexDump.h>
#include <pv/remote.h>
#include <pv/channelSearchManager.h>
#include <pv/serializationHelper.h>
#include <pv/simpleChannelSearchManagerImpl.h>
#include <pv/clientContextImpl.h>
#include <pv/configuration.h>
#include <pv/beaconHandler.h>
#include <pv/logger.h>
#include <pv/security.h>

#include <pv/pvAccessMB.h>

//#include <tr1/unordered_map>

using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;

using namespace std;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

string ClientContextImpl::PROVIDER_NAME = "pva";
Status ChannelImpl::channelDestroyed(
    Status::STATUSTYPE_WARNING, "channel destroyed");
Status ChannelImpl::channelDisconnected(
    Status::STATUSTYPE_WARNING, "channel disconnected");
string emptyString;

// TODO consider std::unordered_map
//typedef std::tr1::unordered_map<pvAccessID, ResponseRequest::weak_pointer> IOIDResponseRequestMap;
typedef std::map<pvAccessID, ResponseRequest::weak_pointer> IOIDResponseRequestMap;


#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

struct delayed_destroyable_deleter
{
    template<class T> void operator()(T * p)
    {
        try
        {
            // new owner, this also allows to use shared_from_this() in destroy() method
            std::tr1::shared_ptr<T> ptr(p);
            ptr->destroy();
        }
        catch(std::exception &ex)
        {
            printf("delayed_destroyable_deleter: unhandled exception: %s", ex.what());
        }
        catch(...)
        {
            printf("delayed_destroyable_deleter: unhandled exception");
        }
    }
};

/**
 * Base channel request.
 * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
 */
class BaseRequestImpl :
    public DataResponse,
    public SubscriptionRequest,
    public TransportSender,
    public Destroyable,
    public std::tr1::enable_shared_from_this<BaseRequestImpl>
{
public:
    typedef std::tr1::shared_ptr<BaseRequestImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const BaseRequestImpl> const_shared_pointer;

    static PVDataCreatePtr pvDataCreate;

    static Status notInitializedStatus;
    static Status destroyedStatus;
    static Status channelNotConnected;
    static Status channelDestroyed;
    static Status otherRequestPendingStatus;
    static Status invalidPutStructureStatus;
    static Status invalidPutArrayStatus;
    static Status invalidBitSetLengthStatus;
    static Status pvRequestNull;

    static PVStructure::shared_pointer nullPVStructure;
    static Structure::const_shared_pointer nullStructure;
    static BitSet::shared_pointer nullBitSet;

    static BitSet::shared_pointer createBitSetFor(
        PVStructure::shared_pointer const & pvStructure,
        BitSet::shared_pointer const & existingBitSet)
    {
        int pvStructureSize = pvStructure->getNumberFields();
        if (existingBitSet.get() && static_cast<int32>(existingBitSet->size()) >= pvStructureSize)
        {
            // clear existing BitSet
            // also necessary if larger BitSet is reused
            existingBitSet->clear();
            return existingBitSet;
        }
        else
            return BitSet::shared_pointer(new BitSet(pvStructureSize));
    }

    static PVField::shared_pointer reuseOrCreatePVField(
        Field::const_shared_pointer const & field,
        PVField::shared_pointer const & existingPVField)
    {
        if (existingPVField.get() && *field == *existingPVField->getField())
            return existingPVField;
        else
            return pvDataCreate->createPVField(field);
    }

protected:

    ChannelImpl::shared_pointer m_channel;

    Requester::shared_pointer m_requester;

    /* negative... */
    static const int NULL_REQUEST = -1;
    static const int PURE_DESTROY_REQUEST = -2;
    static const int PURE_CANCEL_REQUEST = -3;

    pvAccessID m_ioid;

    int32 m_pendingRequest;

    Mutex m_mutex;

    // used to hold ownership until create is called (to support complete async usage)
    ResponseRequest::shared_pointer m_thisPointer;

    bool m_destroyed;
    bool m_initialized;

    AtomicBoolean m_lastRequest;

    AtomicBoolean m_subscribed;

    virtual ~BaseRequestImpl() {};

    BaseRequestImpl(ChannelImpl::shared_pointer const & channel, Requester::shared_pointer const & requester) :
        m_channel(channel),
        m_requester(requester),
        m_ioid(INVALID_IOID),
        m_pendingRequest(NULL_REQUEST),
        m_destroyed(false),
        m_initialized(false),
        m_subscribed()
    {
    }

    void activate() {
        // register response request
        // ResponseRequest::shared_pointer to this instance must already exist
        m_thisPointer = shared_from_this();
        m_ioid = m_channel->getContext()->registerResponseRequest(m_thisPointer);
        m_channel->registerResponseRequest(m_thisPointer);
    }

    bool startRequest(int32 qos) {
        Lock guard(m_mutex);

        // we allow pure destroy...
        if (m_pendingRequest != NULL_REQUEST && qos != PURE_DESTROY_REQUEST && qos != PURE_CANCEL_REQUEST)
            return false;

        m_pendingRequest = qos;
        return true;
    }

    void stopRequest() {
        Lock guard(m_mutex);
        m_pendingRequest = NULL_REQUEST;
    }

    int32 getPendingRequest() {
        Lock guard(m_mutex);
        return m_pendingRequest;
    }

public:

    // used to send message to this request
    Requester::shared_pointer getRequester() {
        return m_requester;
    }

    pvAccessID getIOID() const {
        return m_ioid;
    }

    virtual void initResponse(Transport::shared_pointer const & transport, int8 version, ByteBuffer* payloadBuffer, int8 qos, const Status& status) = 0;
    virtual void normalResponse(Transport::shared_pointer const & transport, int8 version, ByteBuffer* payloadBuffer, int8 qos, const Status& status) = 0;

    virtual void response(Transport::shared_pointer const & transport, int8 version, ByteBuffer* payloadBuffer) {
        transport->ensureData(1);
        int8 qos = payloadBuffer->getByte();

        Status status;
        status.deserialize(payloadBuffer, transport.get());

        try
        {
            if (qos & QOS_INIT)
            {
                if (status.isSuccess())
                {
                    // once created set destroy flag
                    m_mutex.lock();
                    m_initialized = true;
                    m_mutex.unlock();
                }

                // we are initialized now, release pointer
                // this is safe since at least caller owns it
                m_thisPointer.reset();

                initResponse(transport, version, payloadBuffer, qos, status);
            }
            else
            {
                bool destroyReq = false;

                if (qos & QOS_DESTROY)
                {
                    m_mutex.lock();
                    m_initialized = false;
                    destroyReq = true;
                    m_mutex.unlock();
                }

                normalResponse(transport, version, payloadBuffer, qos, status);

                if (destroyReq)
                    destroy();
            }
        }
        catch (std::exception &e) {
            LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what());
        }
        catch (...) {
            LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__);
        }
    }

    virtual void cancel() {

        {
            Lock guard(m_mutex);
            if (m_destroyed)
                return;
        }

        try
        {
            startRequest(PURE_CANCEL_REQUEST);
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (...) {
            // noop (do not complain if fails)
        }

    }

    virtual Channel::shared_pointer getChannel() {
        return m_channel;
    }

    virtual void destroy() {
        destroy(false);
    }

    virtual void lastRequest() {
        m_lastRequest.set();
    }

    virtual void destroy(bool createRequestFailed) {

        {
            Lock guard(m_mutex);
            if (m_destroyed)
                return;
            m_destroyed = true;
        }

        // unregister response request
        m_channel->getContext()->unregisterResponseRequest(m_ioid);
        m_channel->unregisterResponseRequest(m_ioid);

        // destroy remote instance
        if (!createRequestFailed && m_initialized)
        {
            try
            {
                startRequest(PURE_DESTROY_REQUEST);
                m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
            } catch (...) {
                // noop (do not complain if fails)
            }

        }

        // in case this instance is destroyed uninitialized
        m_thisPointer.reset();
    }

    virtual void timeout() {
        cancel();
        // TODO notify?
    }

    void reportStatus(const Status& status) {
        // destroy, since channel (parent) was destroyed
        // NOTE: by-ref compare, not nice
        if (&status == &ChannelImpl::channelDestroyed)
            destroy();
        else if (&status == &ChannelImpl::channelDisconnected)
        {
            m_subscribed.clear();
            stopRequest();
        }
        // TODO notify?
    }

    virtual void resubscribeSubscription(Transport::shared_pointer const & transport) {
        if (transport.get() != 0 && !m_subscribed.get() && startRequest(QOS_INIT))
        {
            m_subscribed.set();
            transport->enqueueSendRequest(shared_from_this());
        }
    }

    virtual void updateSubscription() {
        // default is noop
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int8 qos = getPendingRequest();
        if (qos == -1)
            return;
        else if (qos == PURE_DESTROY_REQUEST)
        {
            control->startMessage((int8)CMD_DESTROY_REQUEST, 8);
            buffer->putInt(m_channel->getServerChannelID());
            buffer->putInt(m_ioid);
        }
        else if (qos == PURE_CANCEL_REQUEST)
        {
            control->startMessage((int8)CMD_CANCEL_REQUEST, 8);
            buffer->putInt(m_channel->getServerChannelID());
            buffer->putInt(m_ioid);
        }
        stopRequest();
    }

};



PVDataCreatePtr BaseRequestImpl::pvDataCreate = getPVDataCreate();

Status BaseRequestImpl::notInitializedStatus(Status::STATUSTYPE_ERROR, "request not initialized");
Status BaseRequestImpl::destroyedStatus(Status::STATUSTYPE_ERROR, "request destroyed");
Status BaseRequestImpl::channelNotConnected(Status::STATUSTYPE_ERROR, "channel not connected");
Status BaseRequestImpl::channelDestroyed(Status::STATUSTYPE_ERROR, "channel destroyed");
Status BaseRequestImpl::otherRequestPendingStatus(Status::STATUSTYPE_ERROR, "other request pending");
Status BaseRequestImpl::invalidPutStructureStatus(Status::STATUSTYPE_ERROR, "incompatible put structure");
Status BaseRequestImpl::invalidPutArrayStatus(Status::STATUSTYPE_ERROR, "incompatible put array");
Status BaseRequestImpl::invalidBitSetLengthStatus(Status::STATUSTYPE_ERROR, "invalid bit-set length");
Status BaseRequestImpl::pvRequestNull(Status::STATUSTYPE_ERROR, "pvRequest == 0");

PVStructure::shared_pointer BaseRequestImpl::nullPVStructure;
Structure::const_shared_pointer BaseRequestImpl::nullStructure;
BitSet::shared_pointer BaseRequestImpl::nullBitSet;

PVACCESS_REFCOUNT_MONITOR_DEFINE(channelProcess);

class ChannelProcessRequestImpl :
    public BaseRequestImpl,
    public ChannelProcess
{
private:
    ChannelProcessRequester::shared_pointer m_callback;
    PVStructure::shared_pointer m_pvRequest;

    ChannelProcessRequestImpl(ChannelImpl::shared_pointer const & channel, ChannelProcessRequester::shared_pointer const & callback, PVStructure::shared_pointer const & pvRequest) :
        BaseRequestImpl(channel, callback),
        m_callback(callback),
        m_pvRequest(pvRequest)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelProcess);
    }

    void activate()
    {
        BaseRequestImpl::activate();

        // pvRequest can be null

        // TODO best-effort support

        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            ChannelProcess::shared_pointer thisPointer = dynamic_pointer_cast<ChannelProcess>(shared_from_this());
            EXCEPTION_GUARD(m_callback->channelProcessConnect(channelDestroyed, thisPointer));
            BaseRequestImpl::destroy(true);
        }
    }

public:
    static ChannelProcess::shared_pointer create(ChannelImpl::shared_pointer const & channel, ChannelProcessRequester::shared_pointer const & callback, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelProcessRequestImpl> tp(
            new ChannelProcessRequestImpl(channel, callback, pvRequest),
            delayed_destroyable_deleter()
        );
        ChannelProcess::shared_pointer thisPointer = tp;
        static_cast<ChannelProcessRequestImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelProcessRequestImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelProcess);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_PROCESS, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        buffer->putByte((int8)m_pendingRequest);

        if (pendingRequest & QOS_INIT)
        {
            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);
        }

        stopRequest();
    }

    virtual void initResponse(Transport::shared_pointer const & /*transport*/, int8 /*version*/, ByteBuffer* /*payloadBuffer*/, int8 /*qos*/, const Status& status) {
        ChannelProcess::shared_pointer thisPtr = dynamic_pointer_cast<ChannelProcess>(shared_from_this());
        EXCEPTION_GUARD(m_callback->channelProcessConnect(status, thisPtr));
    }

    virtual void normalResponse(Transport::shared_pointer const & /*transport*/, int8 /*version*/, ByteBuffer* /*payloadBuffer*/, int8 /*qos*/, const Status& status) {
        ChannelProcess::shared_pointer thisPtr = dynamic_pointer_cast<ChannelProcess>(shared_from_this());
        EXCEPTION_GUARD(m_callback->processDone(status, thisPtr));
    }

    virtual void process()
    {
        ChannelProcess::shared_pointer thisPtr = dynamic_pointer_cast<ChannelProcess>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_callback->processDone(destroyedStatus, thisPtr));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_callback->processDone(notInitializedStatus, thisPtr));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY : QOS_DEFAULT)) {
            EXCEPTION_GUARD(m_callback->processDone(otherRequestPendingStatus, thisPtr));
            return;
        }

        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_callback->processDone(channelNotConnected, thisPtr));
        }
    }

    virtual Channel::shared_pointer getChannel()
    {
        return BaseRequestImpl::getChannel();
    }

    virtual void cancel()
    {
        BaseRequestImpl::cancel();
    }

    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual void lastRequest()
    {
        BaseRequestImpl::lastRequest();
    }

    virtual void lock() {
        // noop
    }

    virtual void unlock() {
        // noop
    }
};







PVACCESS_REFCOUNT_MONITOR_DEFINE(channelGet);

class ChannelGetImpl :
    public BaseRequestImpl,
    public ChannelGet
{
private:
    ChannelGetRequester::shared_pointer m_channelGetRequester;

    PVStructure::shared_pointer m_pvRequest;

    PVStructure::shared_pointer m_structure;
    BitSet::shared_pointer m_bitSet;

    Mutex m_structureMutex;

    ChannelGetImpl(ChannelImpl::shared_pointer const & channel, ChannelGetRequester::shared_pointer const & channelGetRequester, PVStructure::shared_pointer const & pvRequest) :
        BaseRequestImpl(channel, channelGetRequester),
        m_channelGetRequester(channelGetRequester),
        m_pvRequest(pvRequest)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelGet);
    }

    void activate()
    {
        if (!m_pvRequest)
        {
            ChannelGet::shared_pointer thisPointer = dynamic_pointer_cast<ChannelGet>(shared_from_this());
            EXCEPTION_GUARD(m_channelGetRequester->channelGetConnect(pvRequestNull, thisPointer, nullStructure));
            return;
        }

        BaseRequestImpl::activate();

        // TODO immediate get, i.e. get data with init message
        // TODO one-time get, i.e. immediate get + lastRequest

        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            ChannelGet::shared_pointer thisPointer = dynamic_pointer_cast<ChannelGet>(shared_from_this());
            EXCEPTION_GUARD(m_channelGetRequester->channelGetConnect(channelDestroyed, thisPointer, nullStructure));
            BaseRequestImpl::destroy(true);
        }
    }

public:
    static ChannelGet::shared_pointer create(ChannelImpl::shared_pointer const & channel, ChannelGetRequester::shared_pointer const & channelGetRequester, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelGetImpl> tp(
            new ChannelGetImpl(channel, channelGetRequester, pvRequest),
            delayed_destroyable_deleter());
        ChannelGet::shared_pointer thisPointer = tp;
        static_cast<ChannelGetImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelGetImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelGet);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        bool initStage = ((pendingRequest & QOS_INIT) != 0);

        MB_POINT_CONDITIONAL(channelGet, 1, "client channelGet->serialize (start)", !initStage);

        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_GET, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        buffer->putByte((int8)m_pendingRequest);

        if (initStage)
        {
            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);
        }

        MB_POINT_CONDITIONAL(channelGet, 2, "client channelGet->serialize (end)", !initStage);

        stopRequest();
    }

    virtual void initResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 /*qos*/, const Status& status) {
        if (!status.isSuccess())
        {
            ChannelGet::shared_pointer thisPointer = dynamic_pointer_cast<ChannelGet>(shared_from_this());
            EXCEPTION_GUARD(m_channelGetRequester->channelGetConnect(status, thisPointer, nullStructure));
            return;
        }

        // create data and its bitSet
        {
            Lock lock(m_structureMutex);
            m_structure = SerializationHelper::deserializeStructureAndCreatePVStructure(payloadBuffer, transport.get(), m_structure);
            m_bitSet = createBitSetFor(m_structure, m_bitSet);
        }

        // notify
        ChannelGet::shared_pointer thisChannelGet = dynamic_pointer_cast<ChannelGet>(shared_from_this());
        EXCEPTION_GUARD(m_channelGetRequester->channelGetConnect(status, thisChannelGet, m_structure->getStructure()));
    }

    virtual void normalResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 /*qos*/, const Status& status) {

        MB_POINT(channelGet, 8, "client channelGet->deserialize (start)");

        ChannelGet::shared_pointer thisPtr = dynamic_pointer_cast<ChannelGet>(shared_from_this());

        if (!status.isSuccess())
        {
            EXCEPTION_GUARD(m_channelGetRequester->getDone(status, thisPtr, nullPVStructure, nullBitSet));
            return;
        }

        // deserialize bitSet and data
        {
            Lock lock(m_structureMutex);
            m_bitSet->deserialize(payloadBuffer, transport.get());
            m_structure->deserialize(payloadBuffer, transport.get(), m_bitSet.get());
        }

        MB_POINT(channelGet, 9, "client channelGet->deserialize (end), just before channelGet->getDone() is called");

        EXCEPTION_GUARD(m_channelGetRequester->getDone(status, thisPtr, m_structure, m_bitSet));
    }

    virtual void get() {

        MB_INC_AUTO_ID(channelGet);
        MB_POINT(channelGet, 0, "client channelGet->get()");

        ChannelGet::shared_pointer thisPtr = dynamic_pointer_cast<ChannelGet>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelGetRequester->getDone(destroyedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelGetRequester->getDone(notInitializedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
        }
        /*
        // TODO bulk hack
                    if (lastRequest)
                    {
                        try {
                            m_channel->checkAndGetTransport()->flushSendQueue();
                        } catch (std::runtime_error &rte) {
                            stopRequest();
                            EXCEPTION_GUARD(m_channelGetRequester->getDone(channelNotConnected, thisPtr));
                        }
                        return;
                    }
          */
        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_GET : QOS_DEFAULT)) {
            EXCEPTION_GUARD(m_channelGetRequester->getDone(otherRequestPendingStatus, thisPtr, nullPVStructure, nullBitSet));
            return;
        }

        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
            //TODO bulk hack m_channel->checkAndGetTransport()->enqueueOnlySendRequest(thisSender);
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelGetRequester->getDone(channelNotConnected, thisPtr, nullPVStructure, nullBitSet));
        }
    }

    virtual Channel::shared_pointer getChannel()
    {
        return BaseRequestImpl::getChannel();
    }

    virtual void cancel()
    {
        BaseRequestImpl::cancel();
    }

    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual void lastRequest()
    {
        BaseRequestImpl::lastRequest();
    }

    virtual void lock()
    {
        m_structureMutex.lock();
    }

    virtual void unlock()
    {
        m_structureMutex.unlock();
    }
};











PVACCESS_REFCOUNT_MONITOR_DEFINE(channelPut);

class ChannelPutImpl :
    public BaseRequestImpl,
    public ChannelPut
{
private:
    ChannelPutRequester::shared_pointer m_channelPutRequester;

    PVStructure::shared_pointer m_pvRequest;

    PVStructure::shared_pointer m_structure;
    BitSet::shared_pointer m_bitSet;

    Mutex m_structureMutex;

    ChannelPutImpl(ChannelImpl::shared_pointer const & channel, ChannelPutRequester::shared_pointer const & channelPutRequester, PVStructure::shared_pointer const & pvRequest) :
        BaseRequestImpl(channel, channelPutRequester),
        m_channelPutRequester(channelPutRequester),
        m_pvRequest(pvRequest)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelPut);
    }

    void activate()
    {
        if (!m_pvRequest)
        {
            ChannelPut::shared_pointer thisPointer = dynamic_pointer_cast<ChannelPut>(shared_from_this());
            EXCEPTION_GUARD(m_channelPutRequester->channelPutConnect(pvRequestNull, thisPointer, nullStructure));
            return;
        }

        BaseRequestImpl::activate();

        // TODO low-overhead put
        // TODO best-effort put

        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            ChannelPut::shared_pointer thisPointer = dynamic_pointer_cast<ChannelPut>(shared_from_this());
            EXCEPTION_GUARD(m_channelPutRequester->channelPutConnect(channelDestroyed, thisPointer, nullStructure));
            BaseRequestImpl::destroy(true);
        }
    }

public:
    static ChannelPut::shared_pointer create(ChannelImpl::shared_pointer const & channel, ChannelPutRequester::shared_pointer const & channelPutRequester, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelPutImpl> tp(
            new ChannelPutImpl(channel, channelPutRequester, pvRequest),
            delayed_destroyable_deleter());
        ChannelPut::shared_pointer thisPointer = tp;
        static_cast<ChannelPutImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelPutImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelPut);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_PUT, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        buffer->putByte((int8)m_pendingRequest);

        if (pendingRequest & QOS_INIT)
        {
            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);
        }
        else if (!(pendingRequest & QOS_GET))
        {
            // put
            // serialize only what has been changed
            {
                // no need to lock here, since it is already locked via TransportSender IF
                //Lock lock(m_structureMutex);
                m_bitSet->serialize(buffer, control);
                m_structure->serialize(buffer, control, m_bitSet.get());
            }
        }

        stopRequest();
    }

    virtual void initResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 /*qos*/, const Status& status) {
        if (!status.isSuccess())
        {
            ChannelPut::shared_pointer thisChannelPut = dynamic_pointer_cast<ChannelPut>(shared_from_this());
            EXCEPTION_GUARD(m_channelPutRequester->channelPutConnect(status, thisChannelPut, nullStructure));
            return;
        }

        // create data and its bitSet
        {
            Lock lock(m_structureMutex);
            m_structure = SerializationHelper::deserializeStructureAndCreatePVStructure(payloadBuffer, transport.get(), m_structure);
            m_bitSet = createBitSetFor(m_structure, m_bitSet);
        }

        // notify
        ChannelPut::shared_pointer thisChannelPut = dynamic_pointer_cast<ChannelPut>(shared_from_this());
        EXCEPTION_GUARD(m_channelPutRequester->channelPutConnect(status, thisChannelPut, m_structure->getStructure()));
    }

    virtual void normalResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 qos, const Status& status) {

        ChannelPut::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPut>(shared_from_this());

        if (qos & QOS_GET)
        {
            if (!status.isSuccess())
            {
                EXCEPTION_GUARD(m_channelPutRequester->getDone(status, thisPtr, nullPVStructure, nullBitSet));
                return;
            }

            {
                Lock lock(m_structureMutex);
                m_bitSet->deserialize(payloadBuffer, transport.get());
                m_structure->deserialize(payloadBuffer, transport.get(), m_bitSet.get());
            }

            EXCEPTION_GUARD(m_channelPutRequester->getDone(status, thisPtr, m_structure, m_bitSet));
        }
        else
        {
            EXCEPTION_GUARD(m_channelPutRequester->putDone(status, thisPtr));
        }
    }

    virtual void get() {

        ChannelPut::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPut>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelPutRequester->getDone(destroyedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelPutRequester->getDone(notInitializedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_GET | QOS_DESTROY : QOS_GET)) {
            EXCEPTION_GUARD(m_channelPutRequester->getDone(otherRequestPendingStatus, thisPtr, nullPVStructure, nullBitSet));
            return;
        }


        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelPutRequester->getDone(channelNotConnected, thisPtr, nullPVStructure, nullBitSet));
        }
    }

    virtual void put(PVStructure::shared_pointer const & pvPutStructure, BitSet::shared_pointer const & pvPutBitSet) {

        ChannelPut::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPut>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                m_channelPutRequester->putDone(destroyedStatus, thisPtr);
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelPutRequester->putDone(notInitializedStatus, thisPtr));
                return;
            }
        }

        if (!(*m_structure->getStructure() == *pvPutStructure->getStructure()))
        {
            EXCEPTION_GUARD(m_channelPutRequester->putDone(invalidPutStructureStatus, thisPtr));
            return;
        }

        if (pvPutBitSet->size() < m_bitSet->size())
        {
            EXCEPTION_GUARD(m_channelPutRequester->putDone(invalidBitSetLengthStatus, thisPtr));
            return;
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY : QOS_DEFAULT)) {
            m_channelPutRequester->putDone(otherRequestPendingStatus, thisPtr);
            return;
        }

        try {
            lock();
            *m_bitSet = *pvPutBitSet;
            m_structure->copyUnchecked(*pvPutStructure, *m_bitSet);
            unlock();
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelPutRequester->putDone(channelNotConnected, thisPtr));
        }
    }

    virtual Channel::shared_pointer getChannel()
    {
        return BaseRequestImpl::getChannel();
    }

    virtual void cancel()
    {
        BaseRequestImpl::cancel();
    }

    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual void lastRequest()
    {
        BaseRequestImpl::lastRequest();
    }

    virtual void lock()
    {
        m_structureMutex.lock();
    }

    virtual void unlock()
    {
        m_structureMutex.unlock();
    }
};








PVACCESS_REFCOUNT_MONITOR_DEFINE(channelPutGet);

class ChannelPutGetImpl :
    public BaseRequestImpl,
    public ChannelPutGet
{
private:
    ChannelPutGetRequester::shared_pointer m_channelPutGetRequester;

    PVStructure::shared_pointer m_pvRequest;

    // put data container
    PVStructure::shared_pointer m_putData;
    BitSet::shared_pointer m_putDataBitSet;

    // get data container
    PVStructure::shared_pointer m_getData;
    BitSet::shared_pointer m_getDataBitSet;

    Mutex m_structureMutex;

    ChannelPutGetImpl(ChannelImpl::shared_pointer const & channel, ChannelPutGetRequester::shared_pointer const & channelPutGetRequester, PVStructure::shared_pointer const & pvRequest) :
        BaseRequestImpl(channel, channelPutGetRequester),
        m_channelPutGetRequester(channelPutGetRequester),
        m_pvRequest(pvRequest)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelPutGet);
    }

    void activate()
    {
        if (!m_pvRequest)
        {
            ChannelPutGet::shared_pointer thisPointer = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());
            EXCEPTION_GUARD(m_channelPutGetRequester->channelPutGetConnect(pvRequestNull, thisPointer, nullStructure, nullStructure));
            return;
        }

        BaseRequestImpl::activate();

        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            ChannelPutGet::shared_pointer thisPointer = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());
            EXCEPTION_GUARD(m_channelPutGetRequester->channelPutGetConnect(channelDestroyed, thisPointer, nullStructure, nullStructure));
            BaseRequestImpl::destroy(true);
        }
    }

public:
    static ChannelPutGet::shared_pointer create(ChannelImpl::shared_pointer const & channel, ChannelPutGetRequester::shared_pointer const & channelPutGetRequester, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelPutGetImpl> tp(
            new ChannelPutGetImpl(channel, channelPutGetRequester, pvRequest),
            delayed_destroyable_deleter());
        ChannelPutGet::shared_pointer thisPointer = tp;
        static_cast<ChannelPutGetImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelPutGetImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelPutGet);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_PUT_GET, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        if ((pendingRequest & QOS_INIT) == 0)
            buffer->putByte((int8)pendingRequest);

        if (pendingRequest & QOS_INIT)
        {
            buffer->putByte((int8)QOS_INIT);

            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);
        }
        else if (pendingRequest & (QOS_GET | QOS_GET_PUT)) {
            // noop
        }
        else
        {
            {
                // no need to lock here, since it is already locked via TransportSender IF
                //Lock lock(m_structureMutex);
                m_putDataBitSet->serialize(buffer, control);
                m_putData->serialize(buffer, control, m_putDataBitSet.get());
            }
        }

        stopRequest();
    }

    virtual void initResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 /*qos*/, const Status& status) {
        if (!status.isSuccess())
        {
            ChannelPutGet::shared_pointer thisChannelPutGet = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());
            EXCEPTION_GUARD(m_channelPutGetRequester->channelPutGetConnect(status, thisChannelPutGet, nullStructure, nullStructure));
            return;
        }

        {
            Lock lock(m_structureMutex);
            m_putData = SerializationHelper::deserializeStructureAndCreatePVStructure(payloadBuffer, transport.get());
            m_putDataBitSet = createBitSetFor(m_putData, m_putDataBitSet);
            m_getData = SerializationHelper::deserializeStructureAndCreatePVStructure(payloadBuffer, transport.get());
            m_getDataBitSet = createBitSetFor(m_getData, m_getDataBitSet);
        }

        // notify
        ChannelPutGet::shared_pointer thisChannelPutGet = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());
        EXCEPTION_GUARD(m_channelPutGetRequester->channelPutGetConnect(status, thisChannelPutGet, m_putData->getStructure(), m_getData->getStructure()));
    }


    virtual void normalResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 qos, const Status& status) {

        ChannelPutGet::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());

        if (qos & QOS_GET)
        {
            if (!status.isSuccess())
            {
                EXCEPTION_GUARD(m_channelPutGetRequester->getGetDone(status, thisPtr, nullPVStructure, nullBitSet));
                return;
            }

            {
                Lock lock(m_structureMutex);
                // deserialize get data
                m_getDataBitSet->deserialize(payloadBuffer, transport.get());
                m_getData->deserialize(payloadBuffer, transport.get(), m_getDataBitSet.get());
            }

            EXCEPTION_GUARD(m_channelPutGetRequester->getGetDone(status, thisPtr, m_getData, m_getDataBitSet));
        }
        else if (qos & QOS_GET_PUT)
        {
            if (!status.isSuccess())
            {
                EXCEPTION_GUARD(m_channelPutGetRequester->getPutDone(status, thisPtr, nullPVStructure, nullBitSet));
                return;
            }

            {
                Lock lock(m_structureMutex);
                // deserialize put data
                m_putDataBitSet->deserialize(payloadBuffer, transport.get());
                m_putData->deserialize(payloadBuffer, transport.get(), m_putDataBitSet.get());
            }

            EXCEPTION_GUARD(m_channelPutGetRequester->getPutDone(status, thisPtr, m_putData, m_putDataBitSet));
        }
        else
        {
            if (!status.isSuccess())
            {
                EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(status, thisPtr, nullPVStructure, nullBitSet));
                return;
            }

            {
                Lock lock(m_structureMutex);
                // deserialize data
                m_getDataBitSet->deserialize(payloadBuffer, transport.get());
                m_getData->deserialize(payloadBuffer, transport.get(), m_getDataBitSet.get());
            }

            EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(status, thisPtr, m_getData, m_getDataBitSet));
        }
    }


    virtual void putGet(PVStructure::shared_pointer const & pvPutStructure, BitSet::shared_pointer const & bitSet) {

        ChannelPutGet::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(destroyedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(notInitializedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
        }

        if (!(*m_putData->getStructure() == *pvPutStructure->getStructure()))
        {
            EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(invalidPutStructureStatus, thisPtr, nullPVStructure, nullBitSet));
            return;
        }

        if (bitSet->size() < m_putDataBitSet->size())
        {
            EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(invalidBitSetLengthStatus, thisPtr, nullPVStructure, nullBitSet));
            return;
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY : QOS_DEFAULT)) {
            EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(otherRequestPendingStatus, thisPtr, nullPVStructure, nullBitSet));
            return;
        }

        try {
            lock();
            *m_putDataBitSet = *bitSet;
            m_putData->copyUnchecked(*pvPutStructure, *m_putDataBitSet);
            unlock();
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelPutGetRequester->putGetDone(channelNotConnected, thisPtr, nullPVStructure, nullBitSet));
        }
    }

    virtual void getGet() {

        ChannelPutGet::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelPutGetRequester->getGetDone(destroyedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelPutGetRequester->getGetDone(notInitializedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_GET : QOS_GET)) {
            EXCEPTION_GUARD(m_channelPutGetRequester->getGetDone(otherRequestPendingStatus, thisPtr, nullPVStructure, nullBitSet));
            return;
        }

        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelPutGetRequester->getGetDone(channelNotConnected, thisPtr, nullPVStructure, nullBitSet));
        }
    }

    virtual void getPut() {

        ChannelPutGet::shared_pointer thisPtr = dynamic_pointer_cast<ChannelPutGet>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                m_channelPutGetRequester->getPutDone(destroyedStatus, thisPtr, nullPVStructure, nullBitSet);
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelPutGetRequester->getPutDone(notInitializedStatus, thisPtr, nullPVStructure, nullBitSet));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_GET_PUT : QOS_GET_PUT)) {
            m_channelPutGetRequester->getPutDone(otherRequestPendingStatus, thisPtr, nullPVStructure, nullBitSet);
            return;
        }

        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelPutGetRequester->getPutDone(channelNotConnected, thisPtr, nullPVStructure, nullBitSet));
        }
    }

    virtual Channel::shared_pointer getChannel()
    {
        return BaseRequestImpl::getChannel();
    }

    virtual void cancel()
    {
        BaseRequestImpl::cancel();
    }

    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual void lastRequest()
    {
        BaseRequestImpl::lastRequest();
    }

    virtual void lock()
    {
        m_structureMutex.lock();
    }

    virtual void unlock()
    {
        m_structureMutex.unlock();
    }

};










PVACCESS_REFCOUNT_MONITOR_DEFINE(channelRPC);

class ChannelRPCImpl :
    public BaseRequestImpl,
    public ChannelRPC
{
private:
    ChannelRPCRequester::shared_pointer m_channelRPCRequester;

    PVStructure::shared_pointer m_pvRequest;

    PVStructure::shared_pointer m_structure;

    Mutex m_structureMutex;

    ChannelRPCImpl(ChannelImpl::shared_pointer const & channel, ChannelRPCRequester::shared_pointer const & channelRPCRequester, PVStructure::shared_pointer const & pvRequest) :
        BaseRequestImpl(channel, channelRPCRequester),
        m_channelRPCRequester(channelRPCRequester),
        m_pvRequest(pvRequest)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelRPC);
    }

    void activate()
    {
        if (!m_pvRequest)
        {
            ChannelRPC::shared_pointer thisPointer = dynamic_pointer_cast<ChannelRPC>(shared_from_this());
            EXCEPTION_GUARD(m_channelRPCRequester->channelRPCConnect(pvRequestNull, thisPointer));
            return;
        }

        BaseRequestImpl::activate();

        // subscribe
        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            ChannelRPC::shared_pointer thisPointer = dynamic_pointer_cast<ChannelRPC>(shared_from_this());
            EXCEPTION_GUARD(m_channelRPCRequester->channelRPCConnect(channelDestroyed, thisPointer));
            BaseRequestImpl::destroy(true);
        }
    }

public:
    static ChannelRPC::shared_pointer create(ChannelImpl::shared_pointer const & channel, ChannelRPCRequester::shared_pointer const & channelRPCRequester, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelRPCImpl> tp(
            new ChannelRPCImpl(channel, channelRPCRequester, pvRequest),
            delayed_destroyable_deleter());
        ChannelRPC::shared_pointer thisPointer = tp;
        static_cast<ChannelRPCImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelRPCImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelRPC);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_RPC, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        if ((m_pendingRequest & QOS_INIT) == 0)
            buffer->putByte((int8)m_pendingRequest);

        if (pendingRequest & QOS_INIT)
        {
            buffer->putByte((int8)QOS_INIT);

            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);
        }
        else
        {
            {
                // no need to lock here, since it is already locked via TransportSender IF
                //Lock lock(m_structureMutex);
                SerializationHelper::serializeStructureFull(buffer, control, m_structure);
                // release arguments structure
                m_structure.reset();
            }
        }

        stopRequest();
    }

    virtual void initResponse(Transport::shared_pointer const & /*transport*/, int8 /*version*/, ByteBuffer* /*payloadBuffer*/, int8 /*qos*/, const Status& status) {
        if (!status.isSuccess())
        {
            ChannelRPC::shared_pointer thisChannelRPC = dynamic_pointer_cast<ChannelRPC>(shared_from_this());
            EXCEPTION_GUARD(m_channelRPCRequester->channelRPCConnect(status, thisChannelRPC));
            return;
        }

        // notify
        ChannelRPC::shared_pointer thisChannelRPC = dynamic_pointer_cast<ChannelRPC>(shared_from_this());
        EXCEPTION_GUARD(m_channelRPCRequester->channelRPCConnect(status, thisChannelRPC));
    }

    virtual void normalResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 /*qos*/, const Status& status) {

        ChannelRPC::shared_pointer thisPtr = dynamic_pointer_cast<ChannelRPC>(shared_from_this());

        if (!status.isSuccess())
        {
            EXCEPTION_GUARD(m_channelRPCRequester->requestDone(status, thisPtr, nullPVStructure));
            return;
        }


        PVStructure::shared_pointer response(SerializationHelper::deserializeStructureFull(payloadBuffer, transport.get()));
        EXCEPTION_GUARD(m_channelRPCRequester->requestDone(status, thisPtr, response));
    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument) {

        ChannelRPC::shared_pointer thisPtr = dynamic_pointer_cast<ChannelRPC>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelRPCRequester->requestDone(destroyedStatus, thisPtr, nullPVStructure));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelRPCRequester->requestDone(notInitializedStatus, thisPtr, nullPVStructure));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY : QOS_DEFAULT)) {
            EXCEPTION_GUARD(m_channelRPCRequester->requestDone(otherRequestPendingStatus, thisPtr, nullPVStructure));
            return;
        }

        try {
            m_structureMutex.lock();
            m_structure = pvArgument;
            m_structureMutex.unlock();

            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelRPCRequester->requestDone(channelNotConnected, thisPtr, nullPVStructure));
        }
    }

    virtual Channel::shared_pointer getChannel()
    {
        return BaseRequestImpl::getChannel();
    }

    virtual void cancel()
    {
        BaseRequestImpl::cancel();
    }

    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual void lastRequest()
    {
        BaseRequestImpl::lastRequest();
    }

    virtual void lock()
    {
        m_structureMutex.lock();
    }

    virtual void unlock()
    {
        m_structureMutex.unlock();
    }
};









PVACCESS_REFCOUNT_MONITOR_DEFINE(channelArray);

class ChannelArrayImpl :
    public BaseRequestImpl,
    public ChannelArray
{
private:
    ChannelArrayRequester::shared_pointer m_channelArrayRequester;

    PVStructure::shared_pointer m_pvRequest;

    // data container
    PVArray::shared_pointer m_arrayData;

    size_t m_offset;
    size_t m_count;
    size_t m_stride;

    size_t m_length;

    Mutex m_structureMutex;

    ChannelArrayImpl(ChannelImpl::shared_pointer const & channel, ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvRequest) :
        BaseRequestImpl(channel, channelArrayRequester),
        m_channelArrayRequester(channelArrayRequester),
        m_pvRequest(pvRequest),
        m_offset(0), m_count(0),
        m_length(0)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelArray);
    }

    void activate()
    {
        if (!m_pvRequest)
        {
            ChannelArray::shared_pointer thisPointer = dynamic_pointer_cast<ChannelArray>(shared_from_this());
            EXCEPTION_GUARD(m_channelArrayRequester->channelArrayConnect(pvRequestNull, thisPointer, Array::shared_pointer()));
            return;
        }

        BaseRequestImpl::activate();

        // subscribe
        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            ChannelArray::shared_pointer thisPointer = dynamic_pointer_cast<ChannelArray>(shared_from_this());
            EXCEPTION_GUARD(m_channelArrayRequester->channelArrayConnect(channelDestroyed, thisPointer, Array::shared_pointer()));
            BaseRequestImpl::destroy(true);
        }
    }

public:
    static ChannelArray::shared_pointer create(ChannelImpl::shared_pointer const & channel, ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelArrayImpl> tp(
            new ChannelArrayImpl(channel, channelArrayRequester, pvRequest),
            delayed_destroyable_deleter());
        ChannelArray::shared_pointer thisPointer = tp;
        static_cast<ChannelArrayImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelArrayImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelArray);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_ARRAY, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        buffer->putByte((int8)m_pendingRequest);

        if (pendingRequest & QOS_INIT)
        {
            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);
        }
        else if (pendingRequest & QOS_GET)
        {
            // lock... see comment below
            SerializeHelper::writeSize(m_offset, buffer, control);
            SerializeHelper::writeSize(m_count, buffer, control);
            SerializeHelper::writeSize(m_stride, buffer, control);
        }
        else if (pendingRequest & QOS_GET_PUT) // i.e. setLength
        {
            // lock... see comment below
            SerializeHelper::writeSize(m_length, buffer, control);
        }
        else if (pendingRequest & QOS_PROCESS) // i.e. getLength
        {
            // noop
        }
        // put
        else
        {
            {
                // no need to lock here, since it is already locked via TransportSender IF
                //Lock lock(m_structureMutex);
                SerializeHelper::writeSize(m_offset, buffer, control);
                SerializeHelper::writeSize(m_stride, buffer, control);
                // TODO what about count sanity check?
                m_arrayData->serialize(buffer, control, 0, m_count ? m_count : m_arrayData->getLength()); // put from 0 offset (see API doc), m_count == 0 means entire array
            }
        }

        stopRequest();
    }

    virtual void initResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 /*qos*/, const Status& status) {
        if (!status.isSuccess())
        {
            ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());
            EXCEPTION_GUARD(m_channelArrayRequester->channelArrayConnect(status, thisChannelArray, Array::shared_pointer()));
            return;
        }

        // create data and its bitSet
        FieldConstPtr field = transport->cachedDeserialize(payloadBuffer);
        {
            Lock lock(m_structureMutex);
            m_arrayData = dynamic_pointer_cast<PVArray>(getPVDataCreate()->createPVField(field));
        }

        // notify
        ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());
        EXCEPTION_GUARD(m_channelArrayRequester->channelArrayConnect(status, thisChannelArray, m_arrayData->getArray()));
    }

    virtual void normalResponse(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer, int8 qos, const Status& status) {

        ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());

        if (qos & QOS_GET)
        {
            if (!status.isSuccess())
            {
                m_channelArrayRequester->getArrayDone(status, thisChannelArray, PVArray::shared_pointer());
                return;
            }

            {
                Lock lock(m_structureMutex);
                m_arrayData->deserialize(payloadBuffer, transport.get());
            }

            EXCEPTION_GUARD(m_channelArrayRequester->getArrayDone(status, thisChannelArray, m_arrayData));
        }
        else if (qos & QOS_GET_PUT)
        {
            EXCEPTION_GUARD(m_channelArrayRequester->setLengthDone(status, thisChannelArray));
        }
        else if (qos & QOS_PROCESS)
        {
            size_t length = SerializeHelper::readSize(payloadBuffer, transport.get());

            EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(status, thisChannelArray, length));
        }
        else
        {
            EXCEPTION_GUARD(m_channelArrayRequester->putArrayDone(status, thisChannelArray));
        }
    }


    virtual void getArray(size_t offset, size_t count, size_t stride) {

        // TODO stride == 0 check

        ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelArrayRequester->getArrayDone(destroyedStatus, thisChannelArray, PVArray::shared_pointer()));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelArrayRequester->getArrayDone(notInitializedStatus, thisChannelArray, PVArray::shared_pointer()));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_GET : QOS_GET)) {
            EXCEPTION_GUARD(m_channelArrayRequester->getArrayDone(otherRequestPendingStatus, thisChannelArray, PVArray::shared_pointer()));
            return;
        }

        try {
            {
                Lock lock(m_structureMutex);
                m_offset = offset;
                m_count = count;
                m_stride = stride;
            }
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelArrayRequester->getArrayDone(channelNotConnected, thisChannelArray, PVArray::shared_pointer()));
        }
    }

    virtual void putArray(PVArray::shared_pointer const & putArray, size_t offset, size_t count, size_t stride) {

        // TODO stride == 0 check

        ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelArrayRequester->putArrayDone(destroyedStatus, thisChannelArray));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelArrayRequester->putArrayDone(notInitializedStatus, thisChannelArray));
                return;
            }
        }

        if (!(*m_arrayData->getArray() == *putArray->getArray()))
        {
            EXCEPTION_GUARD(m_channelArrayRequester->putArrayDone(invalidPutArrayStatus, thisChannelArray));
            return;
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY : QOS_DEFAULT)) {
            EXCEPTION_GUARD(m_channelArrayRequester->putArrayDone(otherRequestPendingStatus, thisChannelArray));
            return;
        }

        try {
            {
                Lock lock(m_structureMutex);
                m_arrayData->copyUnchecked(*putArray);
                m_offset = offset;
                m_count = count;
                m_stride = stride;
            }
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelArrayRequester->putArrayDone(channelNotConnected, thisChannelArray));
        }
    }

    virtual void setLength(size_t length) {

        ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelArrayRequester->setLengthDone(destroyedStatus, thisChannelArray));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelArrayRequester->setLengthDone(notInitializedStatus, thisChannelArray));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_GET_PUT : QOS_GET_PUT)) {
            EXCEPTION_GUARD(m_channelArrayRequester->setLengthDone(otherRequestPendingStatus, thisChannelArray));
            return;
        }

        try {
            {
                Lock lock(m_structureMutex);
                m_length = length;
            }
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelArrayRequester->setLengthDone(channelNotConnected, thisChannelArray));
        }
    }


    virtual void getLength() {

        ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());

        {
            Lock guard(m_mutex);
            if (m_destroyed) {
                EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(destroyedStatus, thisChannelArray, 0));
                return;
            }
            if (!m_initialized) {
                EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(notInitializedStatus, thisChannelArray, 0));
                return;
            }
        }

        if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_PROCESS : QOS_PROCESS)) {
            EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(otherRequestPendingStatus, thisChannelArray, 0));
            return;
        }

        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            stopRequest();
            EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(channelNotConnected, thisChannelArray, 0));
        }
    }

    virtual Channel::shared_pointer getChannel()
    {
        return BaseRequestImpl::getChannel();
    }

    virtual void cancel()
    {
        BaseRequestImpl::cancel();
    }

    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual void lastRequest()
    {
        BaseRequestImpl::lastRequest();
    }

    virtual void lock()
    {
        m_structureMutex.lock();
    }

    virtual void unlock()
    {
        m_structureMutex.unlock();
    }
};









PVACCESS_REFCOUNT_MONITOR_DEFINE(channelGetField);

// NOTE: this instance is not returned as Request, so it must self-destruct
class ChannelGetFieldRequestImpl :
    public DataResponse,
    public TransportSender,
    public std::tr1::enable_shared_from_this<ChannelGetFieldRequestImpl>
{
public:
    typedef std::tr1::shared_ptr<ChannelGetFieldRequestImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ChannelGetFieldRequestImpl> const_shared_pointer;

private:
    ChannelImpl::shared_pointer m_channel;

    GetFieldRequester::shared_pointer m_callback;
    string m_subField;

    pvAccessID m_ioid;

    Mutex m_mutex;
    bool m_destroyed;

    ResponseRequest::shared_pointer m_thisPointer;

    ChannelGetFieldRequestImpl(ChannelImpl::shared_pointer const & channel, GetFieldRequester::shared_pointer const & callback, string const & subField) :
        m_channel(channel),
        m_callback(callback),
        m_subField(subField),
        m_ioid(INVALID_IOID),
        m_destroyed(false)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelGetField);
    }

    void activate()
    {
        // register response request
        m_thisPointer = shared_from_this();
        m_ioid = m_channel->getContext()->registerResponseRequest(m_thisPointer);
        m_channel->registerResponseRequest(m_thisPointer);

        // enqueue send request
        try {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
        } catch (std::runtime_error &rte) {
            EXCEPTION_GUARD(m_callback->getDone(BaseRequestImpl::channelNotConnected, FieldConstPtr()));
        }
    }

public:
    static shared_pointer create(ChannelImpl::shared_pointer const & channel, GetFieldRequester::shared_pointer const & callback, string const & subField)
    {
        shared_pointer thisPointer(new ChannelGetFieldRequestImpl(channel, callback, subField), delayed_destroyable_deleter());
        thisPointer->activate();
        return thisPointer;
    }

    ~ChannelGetFieldRequestImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelGetField);
    }

    Requester::shared_pointer getRequester() {
        return m_callback;
    }

    pvAccessID getIOID() const {
        return m_ioid;
    }

    virtual void lock() {
        // noop
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        control->startMessage((int8)17, 8);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        SerializeHelper::serializeString(m_subField, buffer, control);
    }


    virtual Channel::shared_pointer getChannel()
    {
        return m_channel;
    }

    virtual void cancel() {
        // TODO
        // noop
    }

    virtual void timeout() {
        cancel();
    }

    void reportStatus(const Status& status) {
        // destroy, since channel (parent) was destroyed
        if (&status == &ChannelImpl::channelDestroyed)
            destroy();
        // TODO notify?
    }

    virtual void unlock() {
        // noop
    }

    virtual void destroy()
    {
        {
            Lock guard(m_mutex);
            if (m_destroyed)
                return;
            m_destroyed = true;
        }

        // unregister response request
        m_channel->getContext()->unregisterResponseRequest(m_ioid);
        m_channel->unregisterResponseRequest(m_ioid);

        m_thisPointer.reset();
    }

    virtual void response(Transport::shared_pointer const & transport, int8 /*version*/, ByteBuffer* payloadBuffer) {

        Status status;
        status.deserialize(payloadBuffer, transport.get());
        if (status.isSuccess())
        {
            // deserialize Field...
            FieldConstPtr field = transport->cachedDeserialize(payloadBuffer);
            EXCEPTION_GUARD(m_callback->getDone(status, field));
        }
        else
        {
            EXCEPTION_GUARD(m_callback->getDone(status, FieldConstPtr()));
        }

        destroy();
    }


};



class MonitorStrategy : public Monitor {
public:
    virtual ~MonitorStrategy() {};
    virtual void init(StructureConstPtr const & structure) = 0;
    virtual void response(Transport::shared_pointer const & transport, ByteBuffer* payloadBuffer) = 0;
    virtual void unlisten() = 0;
};

typedef vector<MonitorElement::shared_pointer> FreeElementQueue;
typedef queue<MonitorElement::shared_pointer> MonitorElementQueue;


class MonitorStrategyQueue :
    public MonitorStrategy,
    public TransportSender,
    public std::tr1::enable_shared_from_this<MonitorStrategyQueue>
{
private:

    int32 m_queueSize;

    StructureConstPtr m_lastStructure;
    FreeElementQueue m_freeQueue;
    MonitorElementQueue m_monitorQueue;


    MonitorRequester::shared_pointer m_callback;

    Mutex m_mutex;

    BitSet::shared_pointer m_bitSet1;
    BitSet::shared_pointer m_bitSet2;
    MonitorElement::shared_pointer m_overrunElement;
    bool m_overrunInProgress;


    MonitorElement::shared_pointer m_nullMonitorElement;

    PVStructure::shared_pointer m_up2datePVStructure;

    int32 m_releasedCount;
    bool m_reportQueueStateInProgress;

    // TODO check for cyclic-ref
    ChannelImpl::shared_pointer m_channel;
    pvAccessID m_ioid;

    bool m_pipeline;
    int32 m_ackAny;

    bool m_unlisten;

public:

    MonitorStrategyQueue(ChannelImpl::shared_pointer channel, pvAccessID ioid,
                         MonitorRequester::shared_pointer const & callback,
                         int32 queueSize,
                         bool pipeline, int32 ackAny) :
        m_queueSize(queueSize), m_lastStructure(),
        m_freeQueue(),
        m_monitorQueue(),
        m_callback(callback), m_mutex(),
        m_bitSet1(), m_bitSet2(), m_overrunInProgress(false),
        m_nullMonitorElement(),
        m_releasedCount(0),
        m_reportQueueStateInProgress(false),
        m_channel(channel), m_ioid(ioid),
        m_pipeline(pipeline), m_ackAny(ackAny),
        m_unlisten(false)
    {
        if (queueSize <= 1)
            throw std::invalid_argument("queueSize <= 1");

        m_freeQueue.reserve(m_queueSize);
        // TODO array based deque
        //m_monitorQueue.reserve(m_queueSize);
    }

    virtual ~MonitorStrategyQueue()
    {
    }

    virtual void init(StructureConstPtr const & structure) {
        Lock guard(m_mutex);

        m_releasedCount = 0;
        m_reportQueueStateInProgress = false;

        // reuse on reconnect
        if (m_lastStructure.get() == 0 ||
                *(m_lastStructure.get()) != *(structure.get()))
        {
            while (!m_monitorQueue.empty())
                m_monitorQueue.pop();
            m_freeQueue.clear();
            for (int32 i = 0; i < m_queueSize; i++)
            {
                PVStructure::shared_pointer pvStructure = getPVDataCreate()->createPVStructure(structure);
                MonitorElement::shared_pointer monitorElement(new MonitorElement(pvStructure));
                m_freeQueue.push_back(monitorElement);
            }
            m_lastStructure = structure;
        }
    }


    virtual void response(Transport::shared_pointer const & transport, ByteBuffer* payloadBuffer) {

        {
            // TODO do not lock deserialization
            Lock guard(m_mutex);

            if (m_overrunInProgress)
            {
                PVStructurePtr pvStructure = m_overrunElement->pvStructurePtr;
                BitSet::shared_pointer changedBitSet = m_overrunElement->changedBitSet;
                BitSet::shared_pointer overrunBitSet = m_overrunElement->overrunBitSet;

                // lazy init
                if (m_bitSet1.get() == 0) m_bitSet1.reset(new BitSet(changedBitSet->size()));
                if (m_bitSet2.get() == 0) m_bitSet2.reset(new BitSet(overrunBitSet->size()));

                m_bitSet1->deserialize(payloadBuffer, transport.get());
                pvStructure->deserialize(payloadBuffer, transport.get(), m_bitSet1.get());
                m_bitSet2->deserialize(payloadBuffer, transport.get());

                // OR local overrun
                // TODO this does not work perfectly if bitSet is compressed !!!
                // uncompressed bitSets should be used !!!
                overrunBitSet->or_and(*(changedBitSet.get()), *(m_bitSet1.get()));

                // OR remove change
                *(changedBitSet.get()) |= *(m_bitSet1.get());

                // OR remote overrun
                *(overrunBitSet.get()) |= *(m_bitSet2.get());

                // m_up2datePVStructure is already set

                return;
            }

            MonitorElementPtr newElement = m_freeQueue.back();
            m_freeQueue.pop_back();

            if (m_freeQueue.empty())
            {
                m_overrunInProgress = true;
                m_overrunElement = newElement;
            }

            // setup current fields
            PVStructurePtr pvStructure = newElement->pvStructurePtr;
            BitSet::shared_pointer changedBitSet = newElement->changedBitSet;
            BitSet::shared_pointer overrunBitSet = newElement->overrunBitSet;

            // deserialize changedBitSet and data, and overrun bit set
            changedBitSet->deserialize(payloadBuffer, transport.get());
            if (m_up2datePVStructure && m_up2datePVStructure.get() != pvStructure.get())
                pvStructure->copyUnchecked(*m_up2datePVStructure, *changedBitSet, true);
            pvStructure->deserialize(payloadBuffer, transport.get(), changedBitSet.get());
            overrunBitSet->deserialize(payloadBuffer, transport.get());

            m_up2datePVStructure = pvStructure;

            if (!m_overrunInProgress)
                m_monitorQueue.push(newElement);
        }

        if (!m_overrunInProgress)
        {
            EXCEPTION_GUARD(m_callback->monitorEvent(shared_from_this()));
        }
    }

    virtual void unlisten()
    {
        bool notifyUnlisten = false;
        {
            Lock guard(m_mutex);
            notifyUnlisten = m_monitorQueue.empty();
            m_unlisten = !notifyUnlisten;
        }

        if (notifyUnlisten)
        {
            EXCEPTION_GUARD(m_callback->unlisten(shared_from_this()));
        }
    }

    virtual MonitorElement::shared_pointer poll() {
        Lock guard(m_mutex);

        if (m_monitorQueue.empty()) {

            if (m_unlisten) {
                m_unlisten = false;
                guard.unlock();
                EXCEPTION_GUARD(m_callback->unlisten(shared_from_this()));
            }
            return m_nullMonitorElement;
        }

        MonitorElement::shared_pointer retVal = m_monitorQueue.front();
        m_monitorQueue.pop();
        return retVal;
    }

    // NOTE: a client must always call poll() after release() to check the presence of any new monitor elements
    virtual void release(MonitorElement::shared_pointer const & monitorElement) {

        // fast sanity check check if monitorElement->pvStructurePtr->getStructure() matches
        // not to accept wrong structure (might happen on monitor reconnect with different type)
        // silent return
        if (monitorElement->pvStructurePtr->getStructure().get() != m_lastStructure.get())
            return;

        bool sendAck = false;
        {
            Lock guard(m_mutex);

            m_freeQueue.push_back(monitorElement);

            if (m_overrunInProgress)
            {
                // compress bit-set
                PVStructurePtr pvStructure = m_overrunElement->pvStructurePtr;
                BitSetUtil::compress(m_overrunElement->changedBitSet, pvStructure);
                BitSetUtil::compress(m_overrunElement->overrunBitSet, pvStructure);

                m_monitorQueue.push(m_overrunElement);

                m_overrunElement.reset();
                m_overrunInProgress = false;
            }

            if (m_pipeline)
            {
                m_releasedCount++;
                if (!m_reportQueueStateInProgress && m_releasedCount >= m_ackAny)
                {
                    sendAck = true;
                    m_reportQueueStateInProgress = true;
                }
            }

            if (sendAck)
            {
                try
                {
                    m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
                } catch (...) {
                    // noop (do not complain if fails)
                    m_reportQueueStateInProgress = false;
                }
            }
        }
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        control->startMessage((int8)CMD_MONITOR, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        buffer->putByte((int8)QOS_GET_PUT);

        {
            Lock guard(m_mutex);
            buffer->putInt(m_releasedCount);
            m_releasedCount = 0;
            m_reportQueueStateInProgress = false;
        }

        // immediate send
        control->flush(true);
    }

    virtual void lock()
    {
        // noop
    }

    virtual void unlock()
    {
        // noop
    }

    Status start() {
        Lock guard(m_mutex);
        while (!m_monitorQueue.empty())
        {
            m_freeQueue.push_back(m_monitorQueue.front());
            m_monitorQueue.pop();
        }
        if (m_overrunElement)
        {
            m_freeQueue.push_back(m_overrunElement);
            m_overrunElement.reset();
        }
        m_overrunInProgress = false;
        return Status::Ok;
    }

    Status stop() {
        return Status::Ok;
    }

    void destroy() {
    }

};



PVACCESS_REFCOUNT_MONITOR_DEFINE(channelMonitor);

class ChannelMonitorImpl :
    public BaseRequestImpl,
    public Monitor
{
private:
    MonitorRequester::shared_pointer m_monitorRequester;
    bool m_started;

    PVStructure::shared_pointer m_pvRequest;

    std::tr1::shared_ptr<MonitorStrategy> m_monitorStrategy;

    int32 m_queueSize;
    bool m_pipeline;
    int32 m_ackAny;

    ChannelMonitorImpl(
        ChannelImpl::shared_pointer const & channel,
        MonitorRequester::shared_pointer const & monitorRequester,
        PVStructure::shared_pointer const & pvRequest)
        :
        BaseRequestImpl(channel, monitorRequester),
        m_monitorRequester(monitorRequester),
        m_started(false),
        m_pvRequest(pvRequest),
        m_queueSize(2),
        m_pipeline(false),
        m_ackAny(0)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelMonitor);
    }

    void activate()
    {
        if (!m_pvRequest)
        {
            Monitor::shared_pointer thisPointer = dynamic_pointer_cast<Monitor>(shared_from_this());
            EXCEPTION_GUARD(m_monitorRequester->monitorConnect(pvRequestNull, thisPointer, StructureConstPtr()));
            return;
        }

        PVStructurePtr pvOptions = m_pvRequest->getSubField<PVStructure>("record._options");
        if (pvOptions) {
            PVStringPtr pvString = pvOptions->getSubField<PVString>("queueSize");
            if (pvString) {
                int32 size;
                std::stringstream ss;
                ss << pvString->get();
                ss >> size;
                if (size > 1)
                    m_queueSize = size;
            }
            pvString = pvOptions->getSubField<PVString>("pipeline");
            if (pvString)
                m_pipeline = (pvString->get() == "true");

            // pipeline options
            if (m_pipeline)
            {
                // defaults to queueSize/2
                m_ackAny = m_queueSize/2;

                pvString = pvOptions->getSubField<PVString>("ackAny");
                if (pvString) {
                    int32 size;
                    string sval = pvString->get();
                    string::size_type slen = sval.length();
                    bool percentage = (slen > 0) && (sval[slen-1] == '%');
                    if (percentage)
                        sval = sval.substr(0, slen-1);
                    std::stringstream ss;
                    ss << sval;
                    ss >> size;
                    if (percentage)
                        size = (m_queueSize * size) / 100;
                    if (size <= 0)
                        m_ackAny = 1;
                    else
                        m_ackAny = (m_ackAny <= m_queueSize) ? size : m_queueSize;
                }
            }
        }

        BaseRequestImpl::activate();

        std::tr1::shared_ptr<MonitorStrategyQueue> tp(
            new MonitorStrategyQueue(m_channel, m_ioid, m_monitorRequester, m_queueSize,
                                     m_pipeline, m_ackAny)
        );
        m_monitorStrategy = tp;

        // subscribe
        try {
            resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
        } catch (std::runtime_error &rte) {
            Monitor::shared_pointer thisPointer = dynamic_pointer_cast<Monitor>(shared_from_this());
            EXCEPTION_GUARD(m_monitorRequester->monitorConnect(channelDestroyed, thisPointer, StructureConstPtr()));
            BaseRequestImpl::destroy(true);
        }
    }

    // override default impl. to provide pipeline QoS flag
    virtual void resubscribeSubscription(Transport::shared_pointer const & transport) {
        if (transport.get() != 0 && !m_subscribed.get() &&
                startRequest(m_pipeline ? (QOS_INIT | QOS_GET_PUT) : QOS_INIT))
        {
            m_subscribed.set();
            transport->enqueueSendRequest(shared_from_this());
        }
    }

public:
    static Monitor::shared_pointer create(
        ChannelImpl::shared_pointer const & channel,
        MonitorRequester::shared_pointer const & monitorRequester,
        PVStructure::shared_pointer const & pvRequest)
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelMonitorImpl> tp(
            new ChannelMonitorImpl(
                channel, monitorRequester,
                pvRequest),
            delayed_destroyable_deleter());
        Monitor::shared_pointer thisPointer = tp;
        static_cast<ChannelMonitorImpl*>(thisPointer.get())->activate();
        return thisPointer;
    }

    ~ChannelMonitorImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channelMonitor);
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        int32 pendingRequest = getPendingRequest();
        if (pendingRequest < 0)
        {
            BaseRequestImpl::send(buffer, control);
            return;
        }

        control->startMessage((int8)CMD_MONITOR, 9);
        buffer->putInt(m_channel->getServerChannelID());
        buffer->putInt(m_ioid);
        buffer->putByte((int8)m_pendingRequest);

        if (pendingRequest & QOS_INIT)
        {
            // pvRequest
            SerializationHelper::serializePVRequest(buffer, control, m_pvRequest);

            // if streaming
            if (pendingRequest & QOS_GET_PUT)
            {
                control->ensureBuffer(4);
                buffer->putInt(m_queueSize);
            }
        }

        stopRequest();
    }

    virtual void initResponse(
        Transport::shared_pointer const & transport,
        int8 /*version*/,
        ByteBuffer* payloadBuffer,
        int8 /*qos*/,
        const Status& status)
    {
        if (!status.isSuccess())
        {
            Monitor::shared_pointer thisChannelMonitor = dynamic_pointer_cast<Monitor>(shared_from_this());
            EXCEPTION_GUARD(m_monitorRequester->monitorConnect(status, thisChannelMonitor, StructureConstPtr()));
            return;
        }

        StructureConstPtr structure =
            dynamic_pointer_cast<const Structure>(
                transport->cachedDeserialize(payloadBuffer)
            );
        m_monitorStrategy->init(structure);

        bool restoreStartedState = m_started;

        // notify
        Monitor::shared_pointer thisChannelMonitor = dynamic_pointer_cast<Monitor>(shared_from_this());
        EXCEPTION_GUARD(m_monitorRequester->monitorConnect(status, thisChannelMonitor, structure));

        if (restoreStartedState)
            start();
    }

    virtual void normalResponse(
        Transport::shared_pointer const & transport,
        int8 /*version*/,
        ByteBuffer* payloadBuffer,
        int8 qos,
        const Status& /*status*/)
    {
        if (qos & QOS_GET)
        {
            // TODO not supported by IF yet...
        }
        else if (qos & QOS_DESTROY)
        {
            // TODO for now status is ignored

            if (payloadBuffer->getRemaining())
                m_monitorStrategy->response(transport, payloadBuffer);

            // unlisten will be called when all the elements in the queue gets processed
            m_monitorStrategy->unlisten();
        }
        else
        {
            m_monitorStrategy->response(transport, payloadBuffer);
        }
    }

    // override, since we optimize status
    virtual void response(
        Transport::shared_pointer const & transport,
        int8 version,
        ByteBuffer* payloadBuffer)
    {
        transport->ensureData(1);
        int8 qos = payloadBuffer->getByte();
        if (qos & QOS_INIT)
        {
            Status status;
            status.deserialize(payloadBuffer, transport.get());
            if (status.isSuccess())
            {
                m_mutex.lock();
                m_initialized = true;
                m_mutex.unlock();
            }
            initResponse(transport, version, payloadBuffer, qos, status);
        }
        else if (qos & QOS_DESTROY)
        {
            Status status;
            status.deserialize(payloadBuffer, transport.get());

            m_mutex.lock();
            m_initialized = false;
            m_mutex.unlock();

            normalResponse(transport, version, payloadBuffer, qos, status);
        }
        else
        {
            normalResponse(transport, version, payloadBuffer, qos, Status::Ok);
        }

    }

    virtual Status start()
    {
        Lock guard(m_mutex);

        if (m_destroyed)
            return BaseRequestImpl::destroyedStatus;
        if (!m_initialized)
            return BaseRequestImpl::notInitializedStatus;

        m_monitorStrategy->start();

        // start == process + get
        if (!startRequest(QOS_PROCESS | QOS_GET))
            return BaseRequestImpl::otherRequestPendingStatus;

        try
        {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
            m_started = true;
            return Status::Ok;
        } catch (std::runtime_error &rte) {
            stopRequest();
            return BaseRequestImpl::channelNotConnected;
        }
    }

    virtual Status stop()
    {
        Lock guard(m_mutex);

        if (m_destroyed)
            return BaseRequestImpl::destroyedStatus;
        if (!m_initialized)
            return BaseRequestImpl::notInitializedStatus;

        m_monitorStrategy->stop();

        // stop == process + no get
        if (!startRequest(QOS_PROCESS))
            return BaseRequestImpl::otherRequestPendingStatus;

        try
        {
            m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
            m_started = false;
            return Status::Ok;
        } catch (std::runtime_error &rte) {
            stopRequest();
            return BaseRequestImpl::channelNotConnected;
        }
    }


    virtual void destroy()
    {
        BaseRequestImpl::destroy();
    }

    virtual MonitorElement::shared_pointer poll()
    {
        return m_monitorStrategy->poll();
    }

    virtual void release(MonitorElement::shared_pointer const & monitorElement)
    {
        m_monitorStrategy->release(monitorElement);
    }

    virtual void lock()
    {
        // noop
    }

    virtual void unlock()
    {
        // noop
    }

};



class AbstractClientResponseHandler : public AbstractResponseHandler {
protected:
    ClientContextImpl::weak_pointer _context;
public:
    AbstractClientResponseHandler(ClientContextImpl::shared_pointer const & context, string const & description) :
        AbstractResponseHandler(context.get(), description), _context(ClientContextImpl::weak_pointer(context)) {
    }

    virtual ~AbstractClientResponseHandler() {
    }
};

class NoopResponse : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    NoopResponse(ClientContextImpl::shared_pointer const & context, string const & description) :
        AbstractClientResponseHandler(context, description)
    {
    }

    virtual ~NoopResponse() {
    }
};


class BadResponse : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    BadResponse(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Bad response")
    {
    }

    virtual ~BadResponse() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & /*transport*/, int8 /*version*/, int8 command,
                                size_t /*payloadSize*/, epics::pvData::ByteBuffer* /*payloadBuffer*/)
    {
        char ipAddrStr[48];
        ipAddrToDottedIP(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

        LOG(logLevelInfo,
            "Undecipherable message (bad response type %d) from %s.",
            command, ipAddrStr);
    }
};


class DataResponseHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    DataResponseHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Data response")
    {
    }

    virtual ~DataResponseHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(4);
        // TODO check and optimize?
        ResponseRequest::shared_pointer rr = _context.lock()->getResponseRequest(payloadBuffer->getInt());
        if (rr.get())
        {
            DataResponse::shared_pointer nrr = dynamic_pointer_cast<DataResponse>(rr);
            if (nrr.get())
                nrr->response(transport, version, payloadBuffer);
        }
    }
};


class MultipleDataResponseHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    MultipleDataResponseHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Multiple data response")
    {
    }

    virtual ~MultipleDataResponseHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        // TODO add submessage payload size, so that non-existant IOID can be skipped
        // and others not lost

        ClientContextImpl::shared_pointer context = _context.lock();
        while (true)
        {
            transport->ensureData(4);
            pvAccessID ioid = payloadBuffer->getInt();
            if (ioid == INVALID_IOID)
                return;

            ResponseRequest::shared_pointer rr = context->getResponseRequest(ioid);
            if (rr.get())
            {
                DataResponse::shared_pointer nrr = dynamic_pointer_cast<DataResponse>(rr);
                if (nrr.get())
                    nrr->response(transport, version, payloadBuffer);
                else
                    return; // we cannot deserialize, we are lost in stream, we must stop
            }
            else
                return; // we cannot deserialize, we are lost in stream, we must stop
        }
    }
};

class SearchResponseHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    SearchResponseHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Search response")
    {
    }

    virtual ~SearchResponseHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(12+4+16+2);

        GUID guid;
        payloadBuffer->get(guid.value, 0, sizeof(guid.value));

        int32 searchSequenceId = payloadBuffer->getInt();

        osiSockAddr serverAddress;
        serverAddress.ia.sin_family = AF_INET;

        // 128-bit IPv6 address
        if (!decodeAsIPv6Address(payloadBuffer, &serverAddress)) return;

        // accept given address if explicitly specified by sender
        if (serverAddress.ia.sin_addr.s_addr == INADDR_ANY)
            serverAddress.ia.sin_addr = responseFrom->ia.sin_addr;

        // NOTE: htons might be a macro (e.g. vxWorks)
        int16 port = payloadBuffer->getShort();
        serverAddress.ia.sin_port = htons(port);

        /*string protocol =*/ SerializeHelper::deserializeString(payloadBuffer, transport.get());

        transport->ensureData(1);
        bool found = payloadBuffer->getByte() != 0;
        if (!found)
            return;

        // reads CIDs
        // TODO optimize
        std::tr1::shared_ptr<epics::pvAccess::ChannelSearchManager> csm = _context.lock()->getChannelSearchManager();
        int16 count = payloadBuffer->getShort();
        for (int i = 0; i < count; i++)
        {
            transport->ensureData(4);
            pvAccessID cid = payloadBuffer->getInt();
            csm->searchResponse(guid, cid, searchSequenceId, version, &serverAddress);
        }


    }
};

class SearchHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    SearchHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Search")
    {
    }

    virtual ~SearchHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(4+1+3+16+2);

        size_t startPosition = payloadBuffer->getPosition();

        /*const int32 searchSequenceId =*/ payloadBuffer->getInt();
        const int8 qosCode = payloadBuffer->getByte();

        // reserved part
        payloadBuffer->getByte();
        payloadBuffer->getShort();

        osiSockAddr responseAddress;
        responseAddress.ia.sin_family = AF_INET;

        // 128-bit IPv6 address
        if (!decodeAsIPv6Address(payloadBuffer, &responseAddress)) return;

        // accept given address if explicitly specified by sender
        if (responseAddress.ia.sin_addr.s_addr == INADDR_ANY)
            responseAddress.ia.sin_addr = responseFrom->ia.sin_addr;

        // NOTE: htons might be a macro (e.g. vxWorks)
        int16 port = payloadBuffer->getShort();
        responseAddress.ia.sin_port = htons(port);

        // we ignore the rest, since we care only about data relevant
        // to do the local multicast

        //
        // locally broadcast if unicast (qosCode & 0x80 == 0x80) via UDP
        //
        if ((qosCode & 0x80) == 0x80)
        {
            // TODO optimize
            ClientContextImpl::shared_pointer context = _context.lock();
            if (!context)
                return;

            BlockingUDPTransport::shared_pointer bt = dynamic_pointer_cast<BlockingUDPTransport>(transport);
            if (bt && bt->hasLocalMulticastAddress())
            {
                // RECEIVE_BUFFER_PRE_RESERVE allows to pre-fix message
                size_t newStartPos = (startPosition-PVA_MESSAGE_HEADER_SIZE)-PVA_MESSAGE_HEADER_SIZE-16;
                payloadBuffer->setPosition(newStartPos);

                // copy part of a header, and add: command, payloadSize, NIF address
                payloadBuffer->put(payloadBuffer->getArray(), startPosition-PVA_MESSAGE_HEADER_SIZE, PVA_MESSAGE_HEADER_SIZE-5);
                payloadBuffer->putByte(CMD_ORIGIN_TAG);
                payloadBuffer->putInt(16);
                // encode this socket bind address
                encodeAsIPv6Address(payloadBuffer, bt->getBindAddress());

                // clear unicast flag
                payloadBuffer->put(startPosition+4, (int8)(qosCode & ~0x80));

                // update response address
                payloadBuffer->setPosition(startPosition+8);
                encodeAsIPv6Address(payloadBuffer, &responseAddress);

                // set to end of a message
                payloadBuffer->setPosition(payloadBuffer->getLimit());

                bt->send(payloadBuffer->getArray()+newStartPos, payloadBuffer->getPosition()-newStartPos,
                         bt->getLocalMulticastAddress());

                return;
            }
        }

    }
};

class BeaconResponseHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    BeaconResponseHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Beacon")
    {
    }

    virtual ~BeaconResponseHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        // reception timestamp
        TimeStamp timestamp;
        timestamp.getCurrent();

        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(12+2+2+16+2);

        GUID guid;
        payloadBuffer->get(guid.value, 0, sizeof(guid.value));

        /*int8 qosCode =*/ payloadBuffer->getByte();
        int8 sequentalID = payloadBuffer->getByte();
        int16 changeCount = payloadBuffer->getShort();

        osiSockAddr serverAddress;
        serverAddress.ia.sin_family = AF_INET;

        // 128-bit IPv6 address
        if (!decodeAsIPv6Address(payloadBuffer, &serverAddress)) return;

        // accept given address if explicitly specified by sender
        if (serverAddress.ia.sin_addr.s_addr == INADDR_ANY)
            serverAddress.ia.sin_addr = responseFrom->ia.sin_addr;

        // NOTE: htons might be a macro (e.g. vxWorks)
        int16 port = payloadBuffer->getShort();
        serverAddress.ia.sin_port = htons(port);

        string protocol = SerializeHelper::deserializeString(payloadBuffer, transport.get());

        // TODO optimize
        ClientContextImpl::shared_pointer context = _context.lock();
        if (!context)
            return;

        std::tr1::shared_ptr<epics::pvAccess::BeaconHandler> beaconHandler = context->getBeaconHandler(protocol, responseFrom);
        // currently we care only for servers used by this context
        if (!beaconHandler)
            return;

        // extra data
        PVFieldPtr data;
        const FieldConstPtr field = getFieldCreate()->deserialize(payloadBuffer, transport.get());
        if (field)
        {
            data = getPVDataCreate()->createPVField(field);
            data->deserialize(payloadBuffer, transport.get());
        }

        // notify beacon handler
        beaconHandler->beaconNotify(responseFrom, version, &timestamp, guid, sequentalID, changeCount, data);
    }
};

class ClientConnectionValidationHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    ClientConnectionValidationHandler(ClientContextImpl::shared_pointer context) :
        AbstractClientResponseHandler(context, "Connection validation")
    {
    }

    virtual ~ClientConnectionValidationHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->setRemoteRevision(version);

        transport->ensureData(4+2);

        transport->setRemoteTransportReceiveBufferSize(payloadBuffer->getInt());
        // TODO
        // TODO serverIntrospectionRegistryMaxSize
        /*int serverIntrospectionRegistryMaxSize = */ payloadBuffer->getShort();

        // authNZ
        size_t size = SerializeHelper::readSize(payloadBuffer, transport.get());
        vector<string> offeredSecurityPlugins;
        offeredSecurityPlugins.reserve(size);
        for (size_t i = 0; i < size; i++)
            offeredSecurityPlugins.push_back(
                SerializeHelper::deserializeString(payloadBuffer, transport.get())
            );

        transport->authNZInitialize(&offeredSecurityPlugins);
    }
};

class ClientConnectionValidatedHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    ClientConnectionValidatedHandler(ClientContextImpl::shared_pointer context) :
        AbstractClientResponseHandler(context, "Connection validated")
    {
    }

    virtual ~ClientConnectionValidatedHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        Status status;
        status.deserialize(payloadBuffer, transport.get());
        transport->verified(status);

    }
};

class MessageHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    MessageHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Message")
    {
    }

    virtual ~MessageHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(5);

        // TODO optimize
        ResponseRequest::shared_pointer rr = _context.lock()->getResponseRequest(payloadBuffer->getInt());
        if (rr.get())
        {
            DataResponse::shared_pointer nrr = dynamic_pointer_cast<DataResponse>(rr);
            if (nrr.get())
            {
                Requester::shared_pointer requester = nrr->getRequester();
                if (requester.get()) {
                    MessageType type = (MessageType)payloadBuffer->getByte();
                    string message = SerializeHelper::deserializeString(payloadBuffer, transport.get());
                    requester->message(message, type);
                }
            }
        }

    }
};

class CreateChannelHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    CreateChannelHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Create channel")
    {
    }

    virtual ~CreateChannelHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(8);
        pvAccessID cid = payloadBuffer->getInt();
        pvAccessID sid = payloadBuffer->getInt();

        Status status;
        status.deserialize(payloadBuffer, transport.get());

        // TODO optimize
        ChannelImpl::shared_pointer channel = static_pointer_cast<ChannelImpl>(_context.lock()->getChannel(cid));
        if (channel.get())
        {
            // failed check
            if (!status.isSuccess()) {

                if (IS_LOGGABLE(logLevelDebug))
                {
                    std::stringstream ss;
                    ss << "Failed to create channel '" << channel->getChannelName() << "': ";
                    ss << status.getMessage();
                    if (!status.getStackDump().empty())
                        ss << std::endl << status.getStackDump();
                    LOG(logLevelDebug, "%s", ss.str().c_str());
                }

                channel->createChannelFailed();
                return;
            }

            //int16 acl = payloadBuffer->getShort();

            channel->connectionCompleted(sid);
        }
    }
};


class DestroyChannelHandler : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
public:
    DestroyChannelHandler(ClientContextImpl::shared_pointer const & context) :
        AbstractClientResponseHandler(context, "Destroy channel")
    {
    }

    virtual ~DestroyChannelHandler() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer)
    {
        AbstractClientResponseHandler::handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

        transport->ensureData(8);
        pvAccessID cid = payloadBuffer->getInt();
        /*pvAccessID sid =*/ payloadBuffer->getInt();

        // TODO optimize
        ChannelImpl::shared_pointer channel = static_pointer_cast<ChannelImpl>(_context.lock()->getChannel(cid));
        if (channel.get())
            channel->channelDestroyedOnServer();
    }
};


/**
 * PVA response handler - main handler which dispatches responses to appripriate handlers.
 * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
 */
class ClientResponseHandler : public ResponseHandler, private epics::pvData::NoDefaultMethods {
private:

    /**
     * Table of response handlers for each command ID.
     */
    vector<ResponseHandler::shared_pointer> m_handlerTable;

public:

    virtual ~ClientResponseHandler() {
    }

    /**
     * @param context
     */
    ClientResponseHandler(ClientContextImpl::shared_pointer const & context) {
        ResponseHandler::shared_pointer badResponse(new BadResponse(context));
        ResponseHandler::shared_pointer dataResponse(new DataResponseHandler(context));

        m_handlerTable.resize(CMD_CANCEL_REQUEST+1);

        m_handlerTable[CMD_BEACON].reset(new BeaconResponseHandler(context)); /*  0 */
        m_handlerTable[CMD_CONNECTION_VALIDATION].reset(new ClientConnectionValidationHandler(context)); /*  1 */
        m_handlerTable[CMD_ECHO].reset(new NoopResponse(context, "Echo")); /*  2 */
        m_handlerTable[CMD_SEARCH].reset(new SearchHandler(context)); /*  3 */
        m_handlerTable[CMD_SEARCH_RESPONSE].reset(new SearchResponseHandler(context)); /*  4 */
        m_handlerTable[CMD_AUTHNZ].reset(new AuthNZHandler(context.get())); /*  5 */
        m_handlerTable[CMD_ACL_CHANGE].reset(new NoopResponse(context, "Access rights change")); /*  6 */
        m_handlerTable[CMD_CREATE_CHANNEL].reset(new CreateChannelHandler(context)); /*  7 */
        m_handlerTable[CMD_DESTROY_CHANNEL].reset(new DestroyChannelHandler(context)); /*  8 */
        m_handlerTable[CMD_CONNECTION_VALIDATED].reset(new ClientConnectionValidatedHandler(context)); /*  9 */
        m_handlerTable[CMD_GET] = dataResponse; /* 10 - get response */
        m_handlerTable[CMD_PUT] = dataResponse; /* 11 - put response */
        m_handlerTable[CMD_PUT_GET] = dataResponse; /* 12 - put-get response */
        m_handlerTable[CMD_MONITOR] = dataResponse; /* 13 - monitor response */
        m_handlerTable[CMD_ARRAY] = dataResponse; /* 14 - array response */
        m_handlerTable[CMD_DESTROY_REQUEST] = badResponse; /* 15 - destroy request */
        m_handlerTable[CMD_PROCESS] = dataResponse; /* 16 - process response */
        m_handlerTable[CMD_GET_FIELD] = dataResponse; /* 17 - get field response */
        m_handlerTable[CMD_MESSAGE].reset(new MessageHandler(context)); /* 18 - message to Requester */
        m_handlerTable[CMD_MULTIPLE_DATA].reset(new MultipleDataResponseHandler(context)); /* 19 - grouped monitors */
        m_handlerTable[CMD_RPC] = dataResponse; /* 20 - RPC response */
        m_handlerTable[CMD_CANCEL_REQUEST] = badResponse; /* 21 - cancel request */
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, int8 version, int8 command,
                                size_t payloadSize, ByteBuffer* payloadBuffer)
    {
        if (command < 0 || command >= (int8)m_handlerTable.size())
        {
            // TODO remove debug output
            char buf[100];
            sprintf(buf, "Invalid (or unsupported) command %d, its payload", command);
            hexDump(buf, (const int8*)(payloadBuffer->getArray()), payloadBuffer->getPosition(), payloadSize);
            return;
        }
        // delegate
        m_handlerTable[command]->handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);
    }
};





PVACCESS_REFCOUNT_MONITOR_DEFINE(channel);


/**
 * Context state enum.
 */
enum ContextState {
    /**
     * State value of non-initialized context.
     */
    CONTEXT_NOT_INITIALIZED,

    /**
     * State value of initialized context.
     */
    CONTEXT_INITIALIZED,

    /**
     * State value of destroyed context.
     */
    CONTEXT_DESTROYED
};




PVACCESS_REFCOUNT_MONITOR_DEFINE(remoteClientContext);

class InternalClientContextImpl :
    public ClientContextImpl,
    public virtual ChannelProvider,
    public std::tr1::enable_shared_from_this<InternalClientContextImpl>
{
public:
    POINTER_DEFINITIONS(InternalClientContextImpl);

    virtual std::string getProviderName()
    {
        return PROVIDER_NAME;
    }

    virtual ChannelFind::shared_pointer channelFind(
        std::string const & channelName,
        ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        // TODO not implemented

        checkChannelName(channelName);

        if (!channelFindRequester.get())
            throw std::runtime_error("null requester");

        Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
        ChannelFind::shared_pointer nullChannelFind;
        EXCEPTION_GUARD(channelFindRequester->channelFindResult(errorStatus, nullChannelFind, false));
        return nullChannelFind;
    }

    virtual ChannelFind::shared_pointer channelList(
        ChannelListRequester::shared_pointer const & channelListRequester)
    {
        if (!channelListRequester.get())
            throw std::runtime_error("null requester");

        Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
        ChannelFind::shared_pointer nullChannelFind;
        PVStringArray::const_svector none;
        EXCEPTION_GUARD(channelListRequester->channelListResult(errorStatus, nullChannelFind, none, false));
        return nullChannelFind;
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority)
    {
        return createChannel(channelName, channelRequester, priority, emptyString);
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority,
        std::string const & addressesStr)
    {
        auto_ptr<InetAddrVector> addresses;
        if (!addressesStr.empty())
        {
            addresses.reset(getSocketAddressList(addressesStr, PVA_SERVER_PORT));
            if (addresses->empty())
                addresses.reset();
        }

        Channel::shared_pointer channel = createChannelInternal(channelName, channelRequester, priority, addresses);
        if (channel.get())
            channelRequester->channelCreated(Status::Ok, channel);
        return channel;

        // NOTE it's up to internal code to respond w/ error to requester and return 0 in case of errors
    }

private:
    /**
     * Implementation of <code>Channel</code>.
     */
    class InternalChannelImpl :
        public ChannelImpl,
        public std::tr1::enable_shared_from_this<InternalChannelImpl>,
        public TimerCallback
    {
    private:

        /**
         * Context.
         */
        std::tr1::shared_ptr<InternalClientContextImpl> m_context;

        /**
         * Client channel ID.
         */
        pvAccessID m_channelID;

        /**
         * Channel name.
         */
        string m_name;

        /**
         * Channel requester.
         */
        ChannelRequester::shared_pointer m_requester;

        /**
         * Process priority.
         */
        short m_priority;

        /**
         * List of fixed addresses, if <code<0</code> name resolution will be used.
         */
        auto_ptr<InetAddrVector> m_addresses;

        /**
         * @brief m_addressIndex Index of currently used address (rollover pointer in a list).
         */
        int m_addressIndex;

        /**
         * Connection status.
         */
        ConnectionState m_connectionState;

        /**
         * List of all channel's pending requests (keys are subscription IDs).
         */
        IOIDResponseRequestMap m_responseRequests;

        /**
         * Mutex for response requests.
         */
        Mutex m_responseRequestsMutex;

        bool m_needSubscriptionUpdate;

        /**
         * Allow reconnection flag.
         */
        bool m_allowCreation;

        /* ****************** */
        /* PVA protocol fields */
        /* ****************** */

        /**
         * Server transport.
         */
        Transport::shared_pointer m_transport;

        /**
         * Server channel ID.
         */
        pvAccessID m_serverChannelID;

        /**
         * Context sync. mutex.
         */
        Mutex m_channelMutex;

        /**
         * Flag indicting what message to send.
         */
        bool m_issueCreateMessage;

        /// Used by SearchInstance.
        int32_t m_userValue;

        /**
         * @brief Server GUID.
         */
        GUID m_guid;

        /**
         * Constructor.
         * @param context
         * @param name
         * @param listener
         * @throws PVAException
         */
        InternalChannelImpl(
            InternalClientContextImpl::shared_pointer const & context,
            pvAccessID channelID,
            string const & name,
            ChannelRequester::shared_pointer const & requester,
            short priority,
            auto_ptr<InetAddrVector>& addresses) :
            m_context(context),
            m_channelID(channelID),
            m_name(name),
            m_requester(requester),
            m_priority(priority),
            m_addresses(addresses),
            m_addressIndex(0),
            m_connectionState(NEVER_CONNECTED),
            m_needSubscriptionUpdate(false),
            m_allowCreation(true),
            m_serverChannelID(0xFFFFFFFF),
            m_issueCreateMessage(true)
        {
            PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channel);
        }

        void activate()
        {
            // register before issuing search request
            ChannelImpl::shared_pointer thisPointer = shared_from_this();
            m_context->registerChannel(thisPointer);

            // connect
            connect();
        }

    public:

        static ChannelImpl::shared_pointer create(InternalClientContextImpl::shared_pointer context,
                pvAccessID channelID,
                string const & name,
                ChannelRequester::shared_pointer requester,
                short priority,
                auto_ptr<InetAddrVector>& addresses)
        {
            // TODO use std::make_shared
            std::tr1::shared_ptr<InternalChannelImpl> tp(
                new InternalChannelImpl(context, channelID, name, requester, priority, addresses),
                delayed_destroyable_deleter());
            ChannelImpl::shared_pointer thisPointer = tp;
            static_cast<InternalChannelImpl*>(thisPointer.get())->activate();
            return thisPointer;
        }

        ~InternalChannelImpl()
        {
            PVACCESS_REFCOUNT_MONITOR_DESTRUCT(channel);
        }

        virtual void destroy()
        {
            destroy(false);
        };

        virtual string getRequesterName()
        {
            return getChannelName();
        };

        virtual void message(std::string const & message,MessageType messageType)
        {
            std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
        }

        int32_t& getUserValue() {
            return m_userValue;
        }

        virtual ChannelProvider::shared_pointer getProvider()
        {
            return m_context;
        }

        // NOTE: synchronization guarantees that <code>transport</code> is non-<code>0</code> and <code>state == CONNECTED</code>.
        virtual std::string getRemoteAddress()
        {
            Lock guard(m_channelMutex);
            if (m_connectionState != CONNECTED) {
                return "";
            }
            else
            {
                return m_transport->getRemoteName();
            }
        }

        virtual std::string getChannelName()
        {
            return m_name;
        }

        virtual ChannelRequester::shared_pointer getChannelRequester()
        {
            return m_requester;
        }

        virtual ConnectionState getConnectionState()
        {
            Lock guard(m_channelMutex);
            return m_connectionState;
        }

        virtual bool isConnected()
        {
            return getConnectionState() == CONNECTED;
        }

        virtual AccessRights getAccessRights(std::tr1::shared_ptr<epics::pvData::PVField> const &)
        {
            return readWrite;
        }

        virtual pvAccessID getID() {
            return m_channelID;
        }

        pvAccessID getChannelID() {
            return m_channelID;
        }

        virtual ClientContextImpl::shared_pointer getContext() {
            return m_context;
        }

        virtual pvAccessID getSearchInstanceID() {
            return m_channelID;
        }

        virtual string getSearchInstanceName() {
            return m_name;
        }

        virtual pvAccessID getServerChannelID() {
            Lock guard(m_channelMutex);
            return m_serverChannelID;
        }

        virtual void registerResponseRequest(ResponseRequest::shared_pointer const & responseRequest)
        {
            Lock guard(m_responseRequestsMutex);
            m_responseRequests[responseRequest->getIOID()] = ResponseRequest::weak_pointer(responseRequest);
        }

        virtual void unregisterResponseRequest(pvAccessID ioid)
        {
            if (ioid == INVALID_IOID) return;
            Lock guard(m_responseRequestsMutex);
            m_responseRequests.erase(ioid);
        }

        void connect() {
            Lock guard(m_channelMutex);
            // if not destroyed...
            if (m_connectionState == DESTROYED)
                throw std::runtime_error("Channel destroyed.");
            else if (m_connectionState != CONNECTED)
                initiateSearch();
        }

        void disconnect() {
            {
                Lock guard(m_channelMutex);
                // if not destroyed...
                if (m_connectionState == DESTROYED)
                    throw std::runtime_error("Channel destroyed.");
                else if (m_connectionState == CONNECTED)
                    disconnect(false, true);
            }

            // should be called without any lock hold
            reportChannelStateChange();
        }

        /**
         * Create a channel, i.e. submit create channel request to the server.
         * This method is called after search is complete.
         * @param transport
         */
        void createChannel(Transport::shared_pointer const & transport)
        {
            Lock guard(m_channelMutex);

            // do not allow duplicate creation to the same transport
            if (!m_allowCreation)
                return;
            m_allowCreation = false;

            // check existing transport
            if (m_transport.get() && m_transport.get() != transport.get())
            {
                disconnectPendingIO(false);

                m_transport->release(getID());
            }
            else if (m_transport.get() == transport.get())
            {
                // request to sent create request to same transport, ignore
                // this happens when server is slower (processing search requests) than client generating it
                return;
            }

            m_transport = transport;
            m_transport->enqueueSendRequest(shared_from_this());
        }

        virtual void cancel() {
            // noop
        }

        virtual void timeout() {
            createChannelFailed();
        }

        /**
         * Create channel failed.
         */
        virtual void createChannelFailed()
        {
            Lock guard(m_channelMutex);

            cancel();

            // release transport if active
            if (m_transport)
            {
                m_transport->release(getID());
                m_transport.reset();
            }

            // ... and search again, with penalty
            initiateSearch(true);
        }

        /**
         * Called when channel created succeeded on the server.
         * <code>sid</code> might not be valid, this depends on protocol revision.
         * @param sid
         */
        virtual void connectionCompleted(pvAccessID sid/*,  rights*/)
        {
            {
                Lock guard(m_channelMutex);

                try
                {
                    // do this silently
                    if (m_connectionState == DESTROYED)
                    {
                        // end connection request
                        cancel();
                        return;
                    }

                    // store data
                    m_serverChannelID = sid;
                    //setAccessRights(rights);

                    m_addressIndex = 0; // reset

                    // user might create monitors in listeners, so this has to be done before this can happen
                    // however, it would not be nice if events would come before connection event is fired
                    // but this cannot happen since transport (TCP) is serving in this thread
                    resubscribeSubscriptions();
                    setConnectionState(CONNECTED);
                }
                catch (...) {
                    // noop
                    // TODO at least log something??
                }

                // NOTE: always call cancel
                // end connection request
                cancel();
            }

            // should be called without any lock hold
            reportChannelStateChange();
        }

        /**
         * @param force force destruction regardless of reference count (not used now)
         */
        void destroy(bool force) {
            {
                Lock guard(m_channelMutex);
                if (m_connectionState == DESTROYED)
                    return;
                //throw std::runtime_error("Channel already destroyed.");
            }

            // do destruction via context
            m_context->destroyChannel(shared_from_this(), force);
        }


        /**
         * Actual destroy method, to be called <code>CAJContext</code>.
         * @param force force destruction regardless of reference count
         * @throws PVAException
         * @throws std::runtime_error
         * @throws IOException
         */
        void destroyChannel(bool /*force*/) {
            {
                Lock guard(m_channelMutex);

                if (m_connectionState == DESTROYED)
                    throw std::runtime_error("Channel already destroyed.");

                // stop searching...
                SearchInstance::shared_pointer thisChannelPointer = shared_from_this();
                m_context->getChannelSearchManager()->unregisterSearchInstance(thisChannelPointer);
                cancel();

                disconnectPendingIO(true);

                if (m_connectionState == CONNECTED)
                {
                    disconnect(false, true);
                }
                else if (m_transport)
                {
                    // unresponsive state, do not forget to release transport
                    m_transport->release(getID());
                    m_transport.reset();
                }


                setConnectionState(DESTROYED);

                // unregister
                m_context->unregisterChannel(shared_from_this());
            }

            // should be called without any lock hold
            reportChannelStateChange();
        }

        /**
         * Disconnected notification.
         * @param initiateSearch    flag to indicate if searching (connect) procedure should be initiated
         * @param remoteDestroy        issue channel destroy request.
         */
        void disconnect(bool initiateSearch, bool remoteDestroy) {
            // order of oldchan and guard is important to ensure
            // oldchan is destoryed after unlock
            Transport::shared_pointer oldchan;
            Lock guard(m_channelMutex);

            if (m_connectionState != CONNECTED)
                return;

            if (!initiateSearch) {
                // stop searching...
                m_context->getChannelSearchManager()->unregisterSearchInstance(shared_from_this());
                cancel();
            }
            setConnectionState(DISCONNECTED);

            disconnectPendingIO(false);

            // release transport
            if (m_transport)
            {
                if (remoteDestroy) {
                    m_issueCreateMessage = false;
                    m_transport->enqueueSendRequest(shared_from_this());
                }

                m_transport->release(getID());
                oldchan.swap(m_transport);
            }

            if (initiateSearch)
                this->initiateSearch();

        }

        void channelDestroyedOnServer() {
            if (isConnected())
            {
                disconnect(true, false);

                // should be called without any lock hold
                reportChannelStateChange();
            }
        }

#define STATIC_SEARCH_BASE_DELAY_SEC 5
#define STATIC_SEARCH_MAX_MULTIPLIER 10

        /**
         * Initiate search (connect) procedure.
         */
        void initiateSearch(bool penalize = false)
        {
            Lock guard(m_channelMutex);

            m_allowCreation = true;

            if (!m_addresses.get())
            {
                m_context->getChannelSearchManager()->registerSearchInstance(shared_from_this(), penalize);
            }
            else if (!m_addresses->empty())
            {
                TimerCallback::shared_pointer tc = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
                m_context->getTimer()->scheduleAfterDelay(tc,
                        (m_addressIndex / m_addresses->size())*STATIC_SEARCH_BASE_DELAY_SEC);
            }
        }

        virtual void callback() {
            // TODO cancellaction?!
            // TODO not in this timer thread !!!
            // TODO boost when a server (from address list) is started!!! IP vs address !!!
            int ix = m_addressIndex % m_addresses->size();
            m_addressIndex++;
            if (m_addressIndex >= static_cast<int>(m_addresses->size()*(STATIC_SEARCH_MAX_MULTIPLIER+1)))
                m_addressIndex = m_addresses->size()*STATIC_SEARCH_MAX_MULTIPLIER;

            // NOTE: calls channelConnectFailed() on failure
            static GUID guid = { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } };
            searchResponse(guid, PVA_PROTOCOL_REVISION, &((*m_addresses)[ix]));
        }

        virtual void timerStopped() {
            // noop
        }

        virtual void searchResponse(const GUID & guid, int8 minorRevision, osiSockAddr* serverAddress) {
            Lock guard(m_channelMutex);
            Transport::shared_pointer transport = m_transport;
            if (transport.get())
            {
                // GUID check case: same server listening on different NIF

                if (!sockAddrAreIdentical(transport->getRemoteAddress(), serverAddress) &&
                        !std::equal(guid.value, guid.value + 12, m_guid.value))
                {
                    EXCEPTION_GUARD(m_requester->message("More than one channel with name '" + m_name +
                                                         "' detected, connected to: " + inetAddressToString(*transport->getRemoteAddress()) + ", ignored: " + inetAddressToString(*serverAddress), warningMessage));
                }

                // do not pass (create transports) with we already have one
                return;
            }

            // NOTE: this creates a new or acquires an existing transport (implies increases usage count)
            transport = m_context->getTransport(shared_from_this(), serverAddress, minorRevision, m_priority);
            if (!transport.get())
            {
                createChannelFailed();
                return;
            }


            // remember GUID
            std::copy(guid.value, guid.value + 12, m_guid.value);

            // create channel
            createChannel(transport);
        }

        virtual void transportClosed() {
            disconnect(true, false);

            // should be called without any lock hold
            reportChannelStateChange();
        }

        virtual void transportChanged() {
//                    initiateSearch();
            // TODO
            // this will be called immediately after reconnect... bad...

        }

        virtual Transport::shared_pointer checkAndGetTransport()
        {
            Lock guard(m_channelMutex);

            if (m_connectionState == DESTROYED)
                throw std::runtime_error("Channel destroyed.");
            else if (m_connectionState != CONNECTED)
                throw  std::runtime_error("Channel not connected.");
            return m_transport;
        }

        virtual Transport::shared_pointer checkDestroyedAndGetTransport()
        {
            Lock guard(m_channelMutex);

            if (m_connectionState == DESTROYED)
                throw std::runtime_error("Channel destroyed.");
            else if (m_connectionState == CONNECTED)
                return m_transport;
            else
                return Transport::shared_pointer();
        }

        virtual Transport::shared_pointer getTransport()
        {
            Lock guard(m_channelMutex);
            return m_transport;
        }

        virtual void transportResponsive(Transport::shared_pointer const & /*transport*/) {
            Lock guard(m_channelMutex);
            if (m_connectionState == DISCONNECTED)
            {
                updateSubscriptions();

                // reconnect using existing IDs, data
                connectionCompleted(m_serverChannelID/*, accessRights*/);
            }
        }

        void transportUnresponsive() {
            /*
            {
                Lock guard(m_channelMutex);
                if (m_connectionState == CONNECTED)
                {
            		// TODO 2 types of disconnected state - distinguish them otherwise disconnect will handle connection loss right
                    setConnectionState(DISCONNECTED);

                    // ... PVA notifies also w/ no access rights callback, although access right are not changed
                }
            }

            // should be called without any lock hold
            reportChannelStateChange();
            */
        }

        /**
         * Set connection state and if changed, notifies listeners.
         * @param newState    state to set.
         */
        void setConnectionState(ConnectionState connectionState)
        {
            Lock guard(m_channelMutex);
            if (m_connectionState != connectionState)
            {
                m_connectionState = connectionState;

                //bool connectionStatusToReport = (connectionState == CONNECTED);
                //if (connectionStatusToReport != lastReportedConnectionState)
                {
                    //lastReportedConnectionState = connectionStatusToReport;
                    // TODO via dispatcher ?!!!
                    //Channel::shared_pointer thisPointer = shared_from_this();
                    //EXCEPTION_GUARD(m_requester->channelStateChange(thisPointer, connectionState));
                    channelStateChangeQueue.push(connectionState);
                }
            }
        }


        std::queue<ConnectionState> channelStateChangeQueue;

        void reportChannelStateChange()
        {
            Channel::shared_pointer thisPointer = shared_from_this();

            while (true)
            {
                ConnectionState connectionState;
                {
                    Lock guard(m_channelMutex);
                    if (channelStateChangeQueue.empty())
                        break;
                    connectionState = channelStateChangeQueue.front();
                    channelStateChangeQueue.pop();
                }

                EXCEPTION_GUARD(m_requester->channelStateChange(thisPointer, connectionState));
            }


        }


        virtual void lock() {
            // noop
        }


        virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
            m_channelMutex.lock();
            bool issueCreateMessage = m_issueCreateMessage;
            m_channelMutex.unlock();

            if (issueCreateMessage)
            {
                control->startMessage((int8)CMD_CREATE_CHANNEL, 2+4);

                // count
                buffer->putShort((int16)1);
                // array of CIDs and names
                buffer->putInt(m_channelID);
                SerializeHelper::serializeString(m_name, buffer, control);
                // send immediately
                // TODO
                control->flush(true);
            }
            else
            {
                control->startMessage((int8)CMD_DESTROY_CHANNEL, 4+4);
                // SID
                m_channelMutex.lock();
                pvAccessID sid = m_serverChannelID;
                m_channelMutex.unlock();
                buffer->putInt(sid);
                // CID
                buffer->putInt(m_channelID);
                // send immediately
                // TODO
                control->flush(true);
            }
        }

        virtual void unlock() {
            // noop
        }


        /**
         * Disconnects (destroys) all channels pending IO.
         * @param destroy    <code>true</code> if channel is being destroyed.
         */
        void disconnectPendingIO(bool destroy)
        {
            Status& status = destroy ? channelDestroyed : channelDisconnected;

            Lock guard(m_responseRequestsMutex);

            m_needSubscriptionUpdate = true;

            int count = 0;
            std::vector<ResponseRequest::weak_pointer> rrs(m_responseRequests.size());
            for (IOIDResponseRequestMap::iterator iter = m_responseRequests.begin();
                    iter != m_responseRequests.end();
                    iter++)
            {
                rrs[count++] = iter->second;
            }

            ResponseRequest::shared_pointer ptr;
            for (int i = 0; i< count; i++)
            {
                if((ptr = rrs[i].lock()))
                {
                    EXCEPTION_GUARD(ptr->reportStatus(status));
                }
            }
        }

        /**
         * Resubscribe subscriptions.
         */
        // TODO to be called from non-transport thread !!!!!!
        void resubscribeSubscriptions()
        {
            Lock guard(m_responseRequestsMutex);

            Transport::shared_pointer transport = getTransport();

            // NOTE: elements cannot be removed within rrs->updateSubscription callbacks
            for (IOIDResponseRequestMap::iterator iter = m_responseRequests.begin();
                    iter != m_responseRequests.end();
                    iter++)
            {
                ResponseRequest::shared_pointer ptr = iter->second.lock();
                if (ptr)
                {
                    SubscriptionRequest::shared_pointer rrs = dynamic_pointer_cast<SubscriptionRequest>(ptr);
                    if (rrs)
                        EXCEPTION_GUARD(rrs->resubscribeSubscription(transport));
                }
            }
        }

        /**
         * Update subscriptions.
         */
        // TODO to be called from non-transport thread !!!!!!
        void updateSubscriptions()
        {
            Lock guard(m_responseRequestsMutex);

            if (m_needSubscriptionUpdate)
                m_needSubscriptionUpdate = false;
            else
                return;    // noop

            // NOTE: elements cannot be removed within rrs->updateSubscription callbacks
            for (IOIDResponseRequestMap::iterator iter = m_responseRequests.begin();
                    iter != m_responseRequests.end();
                    iter++)
            {
                ResponseRequest::shared_pointer ptr = iter->second.lock();
                if (ptr)
                {
                    SubscriptionRequest::shared_pointer rrs = dynamic_pointer_cast<SubscriptionRequest>(ptr);
                    if (rrs)
                        EXCEPTION_GUARD(rrs->updateSubscription());
                }
            }
        }

        virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField)
        {
            ChannelGetFieldRequestImpl::create(shared_from_this(), requester, subField);
        }

        virtual ChannelProcess::shared_pointer createChannelProcess(
            ChannelProcessRequester::shared_pointer const & channelProcessRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelProcessRequestImpl::create(shared_from_this(), channelProcessRequester, pvRequest);
        }

        virtual ChannelGet::shared_pointer createChannelGet(
            ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelGetImpl::create(shared_from_this(), channelGetRequester, pvRequest);
        }

        virtual ChannelPut::shared_pointer createChannelPut(
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelPutImpl::create(shared_from_this(), channelPutRequester, pvRequest);
        }

        virtual ChannelPutGet::shared_pointer createChannelPutGet(
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelPutGetImpl::create(shared_from_this(), channelPutGetRequester, pvRequest);
        }

        virtual ChannelRPC::shared_pointer createChannelRPC(
            ChannelRPCRequester::shared_pointer const & channelRPCRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelRPCImpl::create(shared_from_this(), channelRPCRequester, pvRequest);
        }

        virtual Monitor::shared_pointer createMonitor(
            MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelMonitorImpl::create(shared_from_this(), monitorRequester, pvRequest);
        }

        virtual ChannelArray::shared_pointer createChannelArray(
            ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
        {
            return ChannelArrayImpl::create(shared_from_this(), channelArrayRequester, pvRequest);
        }



        virtual void printInfo() {
            printInfo(std::cout);
        }

        virtual void printInfo(std::ostream& out) {
            //Lock lock(m_channelMutex);

            out << "CHANNEL  : " << m_name << std::endl;
            out << "STATE    : " << ConnectionStateNames[m_connectionState] << std::endl;
            if (m_connectionState == CONNECTED)
            {
                out << "ADDRESS  : " << getRemoteAddress() << std::endl;
                //out << "RIGHTS   : " << getAccessRights() << std::endl;
            }
        }
    };




public:

    InternalClientContextImpl(const Configuration::shared_pointer& conf) :
        m_addressList(""), m_autoAddressList(true), m_connectionTimeout(30.0f), m_beaconPeriod(15.0f),
        m_broadcastPort(PVA_BROADCAST_PORT), m_receiveBufferSize(MAX_TCP_RECV),
        m_namedLocker(), m_lastCID(0), m_lastIOID(0),
        m_version("pvAccess Client", "cpp",
                  EPICS_PVA_MAJOR_VERSION,
                  EPICS_PVA_MINOR_VERSION,
                  EPICS_PVA_MAINTENANCE_VERSION,
                  EPICS_PVA_DEVELOPMENT_FLAG),
        m_contextState(CONTEXT_NOT_INITIALIZED),
        m_configuration(conf),
        m_flushStrategy(DELAYED)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(remoteClientContext);
        MB_INIT;
        if(!m_configuration) m_configuration = ConfigurationFactory::getConfiguration("pvAccess-client");
        m_flushTransports.reserve(64);
        loadConfiguration();
    }

    virtual Configuration::shared_pointer getConfiguration() {
        return m_configuration;
    }

    virtual const Version& getVersion() {
        return m_version;
    }

    virtual Timer::shared_pointer getTimer()
    {
        return m_timer;
    }

    virtual TransportRegistry::shared_pointer getTransportRegistry()
    {
        return m_transportRegistry;
    }

    virtual Transport::shared_pointer getSearchTransport()
    {
        return m_searchTransport;
    }

    virtual void initialize() {
        Lock lock(m_contextMutex);

        if (m_contextState == CONTEXT_DESTROYED)
            throw std::runtime_error("Context destroyed.");
        else if (m_contextState == CONTEXT_INITIALIZED)
            throw std::runtime_error("Context already initialized.");

        internalInitialize();

        m_contextState = CONTEXT_INITIALIZED;
    }

    virtual void printInfo() {
        printInfo(std::cout);
    }

    virtual void printInfo(std::ostream& out) {
        Lock lock(m_contextMutex);

        out << "CLASS              : ::epics::pvAccess::ClientContextImpl" << std::endl;
        out << "VERSION            : " << m_version.getVersionString() << std::endl;
        out << "ADDR_LIST          : " << m_addressList << std::endl;
        out << "AUTO_ADDR_LIST     : " << (m_autoAddressList ? "true" : "false") << std::endl;
        out << "CONNECTION_TIMEOUT : " << m_connectionTimeout << std::endl;
        out << "BEACON_PERIOD      : " << m_beaconPeriod << std::endl;
        out << "BROADCAST_PORT     : " << m_broadcastPort << std::endl;;
        out << "RCV_BUFFER_SIZE    : " << m_receiveBufferSize << std::endl;
        out << "STATE              : ";
        switch (m_contextState)
        {
        case CONTEXT_NOT_INITIALIZED:
            out << "CONTEXT_NOT_INITIALIZED" << std::endl;
            break;
        case CONTEXT_INITIALIZED:
            out << "CONTEXT_INITIALIZED" << std::endl;
            break;
        case CONTEXT_DESTROYED:
            out << "CONTEXT_DESTROYED" << std::endl;
            break;
        default:
            out << "UNKNOWN" << std::endl;
        }
    }

    virtual void destroy()
    {
        {
            Lock guard(m_contextMutex);

            if (m_contextState == CONTEXT_DESTROYED)
                return;

            // go into destroyed state ASAP
            m_contextState = CONTEXT_DESTROYED;
        }

        internalDestroy();
    }

    virtual void dispose()
    {
        try {
            destroy();
        } catch (std::exception& ex) {
            printf("dispose(): %s\n", ex.what()); // tODO remove
        } catch (...) {
            /* TODO log with low level */
        }
    }

    ~InternalClientContextImpl()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(remoteClientContext);
    }

private:

    void loadConfiguration() {

        // TODO for now just a simple switch
        int32 debugLevel = m_configuration->getPropertyAsInteger(PVACCESS_DEBUG, 0);
        if (debugLevel > 0)
            SET_LOG_LEVEL(logLevelDebug);

        m_addressList = m_configuration->getPropertyAsString("EPICS_PVA_ADDR_LIST", m_addressList);
        m_autoAddressList = m_configuration->getPropertyAsBoolean("EPICS_PVA_AUTO_ADDR_LIST", m_autoAddressList);
        m_connectionTimeout = m_configuration->getPropertyAsFloat("EPICS_PVA_CONN_TMO", m_connectionTimeout);
        m_beaconPeriod = m_configuration->getPropertyAsFloat("EPICS_PVA_BEACON_PERIOD", m_beaconPeriod);
        m_broadcastPort = m_configuration->getPropertyAsInteger("EPICS_PVA_BROADCAST_PORT", m_broadcastPort);
        m_receiveBufferSize = m_configuration->getPropertyAsInteger("EPICS_PVA_MAX_ARRAY_BYTES", m_receiveBufferSize);
    }

    void internalInitialize() {

        osiSockAttach();
        m_timer.reset(new Timer("pvAccess-client timer", lowPriority));
        Context::shared_pointer thisPointer = shared_from_this();
        m_connector.reset(new BlockingTCPConnector(thisPointer, m_receiveBufferSize, m_connectionTimeout));
        m_transportRegistry.reset(new TransportRegistry());

        m_responseHandler.reset(new ClientResponseHandler(shared_from_this()));

        // preinitialize security plugins
        SecurityPluginRegistry::instance();

        // TODO put memory barrier here... (if not already called within a lock?)

        // setup UDP transport
        initializeUDPTransport();

        // setup search manager
        m_channelSearchManager = SimpleChannelSearchManagerImpl::create(thisPointer);

        // TODO what if initialization failed!!!
    }

    /**
     * Initialized UDP transport (broadcast socket and repeater connection).
     */
    bool initializeUDPTransport() {

        // query broadcast addresses of all IFs
        SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
        if (socket == INVALID_SOCKET)
        {
            LOG(logLevelError, "Failed to create a socket to introspect network interfaces.");
            return false;
        }

        IfaceNodeVector ifaceList;
        if (discoverInterfaces(ifaceList, socket, 0) || ifaceList.size() == 0)
        {
            LOG(logLevelError, "Failed to introspect interfaces or no network interfaces available.");
            return false;
        }
        epicsSocketDestroy (socket);

        initializeUDPTransports(false, m_udpTransports, ifaceList, m_responseHandler, m_searchTransport,
                                m_broadcastPort, m_autoAddressList, m_addressList, std::string());

        return true;
    }

    void internalDestroy() {

        //
        // cleanup
        //

        // this will also close all PVA transports
        destroyAllChannels();

        // stop UDPs
        for (BlockingUDPTransportVector::const_iterator iter = m_udpTransports.begin();
                iter != m_udpTransports.end(); iter++)
            (*iter)->close();
        m_udpTransports.clear();

        // stop UDPs
        if (m_searchTransport)
            m_searchTransport->close();

        // wait for all transports to cleanly exit
        int tries = 40;
        epics::pvData::int32 transportCount;
        while ((transportCount = m_transportRegistry->numberOfActiveTransports()) && tries--)
            epicsThreadSleep(0.025);

        if (transportCount)
            LOG(logLevelDebug, "PVA client context destroyed with %d transport(s) active.", transportCount);
    }

    void destroyAllChannels() {
        Lock guard(m_cidMapMutex);

        int count = 0;
        std::vector<ChannelImpl::weak_pointer> channels(m_channelsByCID.size());
        for (CIDChannelMap::iterator iter = m_channelsByCID.begin();
                iter != m_channelsByCID.end();
                iter++)
        {
            channels[count++] = iter->second;
        }

        guard.unlock();


        ChannelImpl::shared_pointer ptr;
        for (int i = 0; i < count; i++)
        {
            ptr = channels[i].lock();
            if (ptr)
            {
                EXCEPTION_GUARD(ptr->destroy());
            }
        }
    }

    /**
     * Check channel name.
     */
    void checkChannelName(std::string const & name) {
        if (name.empty())
            throw std::runtime_error("0 or empty channel name");
        else if (name.length() > MAX_CHANNEL_NAME_LENGTH)
            throw std::runtime_error("name too long");
    }

    /**
     * Check context state and tries to establish necessary state.
     */
    void checkState() {
        Lock lock(m_contextMutex); // TODO check double-lock?!!!

        if (m_contextState == CONTEXT_DESTROYED)
            throw std::runtime_error("Context destroyed.");
        else if (m_contextState == CONTEXT_NOT_INITIALIZED)
            initialize();
    }

    /**
     * Register channel.
     * @param channel
     */
    void registerChannel(ChannelImpl::shared_pointer const & channel)
    {
        Lock guard(m_cidMapMutex);
        m_channelsByCID[channel->getChannelID()] = ChannelImpl::weak_pointer(channel);
    }

    /**
     * Unregister channel.
     * @param channel
     */
    void unregisterChannel(ChannelImpl::shared_pointer const & channel)
    {
        Lock guard(m_cidMapMutex);
        m_channelsByCID.erase(channel->getChannelID());
    }

    /**
     * Searches for a channel with given channel ID.
     * @param channelID CID.
     * @return channel with given CID, <code>0</code> if non-existent.
     */
    Channel::shared_pointer getChannel(pvAccessID channelID)
    {
        Lock guard(m_cidMapMutex);
        CIDChannelMap::iterator it = m_channelsByCID.find(channelID);
        return (it == m_channelsByCID.end() ? Channel::shared_pointer() : static_pointer_cast<Channel>(it->second.lock()));
    }

    /**
     * Generate Client channel ID (CID).
     * @return Client channel ID (CID).
     */
    pvAccessID generateCID()
    {
        Lock guard(m_cidMapMutex);

        // search first free (theoretically possible loop of death)
        while (m_channelsByCID.find(++m_lastCID) != m_channelsByCID.end()) ;
        // reserve CID
        m_channelsByCID[m_lastCID].reset();
        return m_lastCID;
    }

    /**
     * Free generated channel ID (CID).
     */
    void freeCID(int cid)
    {
        Lock guard(m_cidMapMutex);
        m_channelsByCID.erase(cid);
    }


    /**
     * Searches for a response request with given channel IOID.
     * @param ioid    I/O ID.
     * @return request response with given I/O ID.
     */
    ResponseRequest::shared_pointer getResponseRequest(pvAccessID ioid)
    {
        Lock guard(m_ioidMapMutex);
        IOIDResponseRequestMap::iterator it = m_pendingResponseRequests.find(ioid);
        if (it == m_pendingResponseRequests.end()) return ResponseRequest::shared_pointer();
        return it->second.lock();
    }

    /**
     * Register response request.
     * @param request request to register.
     * @return request ID (IOID).
     */
    pvAccessID registerResponseRequest(ResponseRequest::shared_pointer const & request)
    {
        Lock guard(m_ioidMapMutex);
        pvAccessID ioid = generateIOID();
        m_pendingResponseRequests[ioid] = ResponseRequest::weak_pointer(request);
        return ioid;
    }

    /**
     * Unregister response request.
     * @param request
     * @return removed object, can be <code>0</code>
     */
    ResponseRequest::shared_pointer unregisterResponseRequest(pvAccessID ioid)
    {
        if (ioid == INVALID_IOID) return ResponseRequest::shared_pointer();

        Lock guard(m_ioidMapMutex);
        IOIDResponseRequestMap::iterator it = m_pendingResponseRequests.find(ioid);
        if (it == m_pendingResponseRequests.end())
            return ResponseRequest::shared_pointer();

        ResponseRequest::shared_pointer retVal = it->second.lock();
        m_pendingResponseRequests.erase(it);
        return retVal;
    }

    /**
     * Generate IOID.
     * @return IOID.
     */
    pvAccessID generateIOID()
    {
        Lock guard(m_ioidMapMutex);

        // search first free (theoretically possible loop of death)
        do {
            while (m_pendingResponseRequests.find(++m_lastIOID) != m_pendingResponseRequests.end()) ;
        } while (m_lastIOID == INVALID_IOID);

        // reserve IOID
        m_pendingResponseRequests[m_lastIOID].reset();
        return m_lastIOID;
    }

    /**
     * Called each time beacon anomaly is detected.
     */
    virtual void newServerDetected()
    {
        if (m_channelSearchManager)
            m_channelSearchManager->newServerDetected();
    }

    /**
     * Get (and if necessary create) beacon handler.
     * @param protocol the protocol.
     * @param responseFrom remote source address of received beacon.
     * @return beacon handler for particular server.
     */
    BeaconHandler::shared_pointer getBeaconHandler(std::string const & protocol, osiSockAddr* responseFrom)
    {
        // TODO !!! protocol !!!
        if (protocol != "tcp")
            return BeaconHandler::shared_pointer();

        Lock guard(m_beaconMapMutex);
        AddressBeaconHandlerMap::iterator it = m_beaconHandlers.find(*responseFrom);
        BeaconHandler::shared_pointer handler;
        if (it == m_beaconHandlers.end())
        {
            handler.reset(new BeaconHandler(shared_from_this(), protocol, responseFrom));
            m_beaconHandlers[*responseFrom] = handler;
        }
        else
            handler = it->second;
        return handler;
    }

    /**
     * Get, or create if necessary, transport of given server address.
     * @param serverAddress    required transport address
     * @param priority process priority.
     * @return transport for given address
     */
    Transport::shared_pointer getTransport(TransportClient::shared_pointer const & client, osiSockAddr* serverAddress, int8 minorRevision, int16 priority)
    {
        try
        {
            Transport::shared_pointer t = m_connector->connect(client, m_responseHandler, *serverAddress, minorRevision, priority);
            // TODO !!!
            //static_pointer_cast<BlockingTCPTransport>(t)->setFlushStrategy(m_flushStrategy);
            return t;
        }
        catch (...)
        {
            // TODO log
            //printf("failed to get transport\n");
            return Transport::shared_pointer();
        }
    }

    /**
     * Internal create channel.
     */
    // TODO no minor version with the addresses
    // TODO what if there is an channel with the same name, but on different host!
    ChannelImpl::shared_pointer createChannelInternal(std::string const & name, ChannelRequester::shared_pointer const & requester, short priority,
            auto_ptr<InetAddrVector>& addresses) { // TODO addresses

        checkState();
        checkChannelName(name);

        if (!requester)
            throw std::runtime_error("0 requester");

        if (priority < ChannelProvider::PRIORITY_MIN || priority > ChannelProvider::PRIORITY_MAX)
            throw std::range_error("priority out of bounds");

        bool lockAcquired = true; // TODO namedLocker->acquireSynchronizationObject(name, LOCK_TIMEOUT);
        if (lockAcquired)
        {
            try
            {
                pvAccessID cid = generateCID();
                return InternalChannelImpl::create(shared_from_this(), cid, name, requester, priority, addresses);
            }
            catch(...) {
                // TODO
                return ChannelImpl::shared_pointer();
            }
            // TODO namedLocker.releaseSynchronizationObject(name);
        }
        else
        {
            // TODO is this OK?
            throw std::runtime_error("Failed to obtain synchronization lock for '" + name + "', possible deadlock.");
        }
    }

    /**
     * Destroy channel.
     * @param channel
     * @param force
     * @throws PVAException
     * @throws std::runtime_error
     */
    void destroyChannel(ChannelImpl::shared_pointer const & channel, bool force) {

        string name = channel->getChannelName();
        bool lockAcquired = true; //namedLocker->acquireSynchronizationObject(name, LOCK_TIMEOUT);
        if (lockAcquired)
        {
            try
            {
                channel->destroyChannel(force);
            }
            catch(...) {
                // TODO
            }
            // TODO    namedLocker->releaseSynchronizationObject(channel.getChannelName());
        }
        else
        {
            // TODO is this OK?
            throw std::runtime_error("Failed to obtain synchronization lock for '" + name + "', possible deadlock.");
        }
    }

    virtual void configure(epics::pvData::PVStructure::shared_pointer configuration)
    {   // remove?
        if (m_transportRegistry->numberOfActiveTransports() > 0)
            throw std::runtime_error("Configure must be called when there is no transports active.");

        PVInt::shared_pointer pvStrategy = dynamic_pointer_cast<PVInt>(configuration->getSubField("strategy"));
        if (pvStrategy.get())
        {
            int32 value = pvStrategy->get();
            switch (value)
            {
            case IMMEDIATE:
            case DELAYED:
            case USER_CONTROLED:
                m_flushStrategy = static_cast<FlushStrategy>(value);
                break;
            default:
                // TODO report warning
                break;
            }
        }

    }

    virtual void flush()
    {
        m_transportRegistry->toArray(m_flushTransports);
        TransportRegistry::transportVector_t::const_iterator iter = m_flushTransports.begin();
        while (iter != m_flushTransports.end())
            (*iter++)->flushSendQueue();
        m_flushTransports.clear();
    }

    virtual void poll()
    {
        // TODO
    }

    std::map<std::string, std::tr1::shared_ptr<SecurityPlugin> >& getSecurityPlugins()
    {
        return SecurityPluginRegistry::instance().getClientSecurityPlugins();
    }

    /**
     * Get channel search manager.
     * @return channel search manager.
     */
    ChannelSearchManager::shared_pointer getChannelSearchManager() {
        return m_channelSearchManager;
    }

    /**
     * A space-separated list of broadcast address for process variable name resolution.
     * Each address must be of the form: ip.number:port or host.name:port
     */
    string m_addressList;

    /**
     * Define whether or not the network interfaces should be discovered at runtime.
     */
    bool m_autoAddressList;

    /**
     * If the context doesn't see a beacon from a server that it is connected to for
     * connectionTimeout seconds then a state-of-health message is sent to the server over TCP/IP.
     * If this state-of-health message isn't promptly replied to then the context will assume that
     * the server is no longer present on the network and disconnect.
     */
    float m_connectionTimeout;

    /**
     * Period in second between two beacon signals.
     */
    float m_beaconPeriod;

    /**
     * Broadcast (beacon, search) port number to listen to.
     */
    int32 m_broadcastPort;

    /**
     * Receive buffer size (max size of payload).
     */
    int m_receiveBufferSize;

    /**
     * Timer.
     */
    Timer::shared_pointer m_timer;

    /**
     * UDP transports needed to receive channel searches.
     */
    BlockingUDPTransportVector m_udpTransports;

    /**
     * UDP transport needed for channel searches.
     */
    BlockingUDPTransport::shared_pointer m_searchTransport;

    /**
     * PVA connector (creates PVA virtual circuit).
     */
    auto_ptr<BlockingTCPConnector> m_connector;

    /**
     * PVA transport (virtual circuit) registry.
     * This registry contains all active transports - connections to PVA servers.
     */
    TransportRegistry::shared_pointer m_transportRegistry;

    /**
     * Response handler.
     */
    ClientResponseHandler::shared_pointer m_responseHandler;

    /**
     * Context instance.
     */
    NamedLockPattern<string> m_namedLocker;

    /**
     * Context instance.
     */
    static const int LOCK_TIMEOUT = 20 * 1000;    // 20s

    /**
     * Map of channels (keys are CIDs).
     */
    // TODO consider std::unordered_map
    typedef std::map<pvAccessID, ChannelImpl::weak_pointer> CIDChannelMap;
    CIDChannelMap m_channelsByCID;

    /**
     *  CIDChannelMap mutex.
     */
    Mutex m_cidMapMutex;

    /**
     * Last CID cache.
     */
    pvAccessID m_lastCID;

    /**
     * Map of pending response requests (keys are IOID).
     */
    IOIDResponseRequestMap m_pendingResponseRequests;

    /**
     *  IOIDResponseRequestMap mutex.
     */
    Mutex m_ioidMapMutex;

    /**
     * Last IOID cache.
     */
    pvAccessID m_lastIOID;

    /**
     * Channel search manager.
     * Manages UDP search requests.
     */
    ChannelSearchManager::shared_pointer m_channelSearchManager;

    /**
     * Beacon handler map.
     */
    // TODO consider std::unordered_map
    typedef std::map<osiSockAddr, BeaconHandler::shared_pointer, comp_osiSock_lt> AddressBeaconHandlerMap;
    AddressBeaconHandlerMap m_beaconHandlers;

    /**
     *  IOIDResponseRequestMap mutex.
     */
    Mutex m_beaconMapMutex;

    /**
     * Version.
     */
    Version m_version;

private:

    /**
     * Context state.
     */
    ContextState m_contextState;

    /**
     * Context sync. mutex.
     */
    Mutex m_contextMutex;

    friend class ChannelProviderImpl;

    Configuration::shared_pointer m_configuration;

    TransportRegistry::transportVector_t m_flushTransports;

    FlushStrategy m_flushStrategy;
};

ChannelProvider::shared_pointer createClientProvider(const Configuration::shared_pointer& conf)
{
    InternalClientContextImpl::shared_pointer t(new InternalClientContextImpl(conf), delayed_destroyable_deleter());
    t->initialize();
    return t;
}

}
};

