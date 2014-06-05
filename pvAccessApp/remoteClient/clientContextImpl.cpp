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
#include <pv/serializationHelper.h>
#include <pv/convert.h>
#include <pv/queue.h>
#include <pv/standardPVField.h>

#include <pv/pvAccess.h>
#include <pv/pvaConstants.h>
#include <pv/blockingUDP.h>
#include <pv/blockingTCP.h>
#include <pv/namedLockPattern.h>
#include <pv/inetAddressUtil.h>
#include <pv/hexDump.h>
#include <pv/remote.h>
#include <pv/channelSearchManager.h>
#include <pv/simpleChannelSearchManagerImpl.h>
#include <pv/clientContextImpl.h>
#include <pv/configuration.h>
#include <pv/beaconHandler.h>
#include <pv/logger.h>

#include <pv/pvAccessMB.h>

//#include <tr1/unordered_map>

using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;

using namespace std;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        String ClientContextImpl::PROVIDER_NAME = "pva";
        Status ChannelImpl::channelDestroyed = Status(
            Status::STATUSTYPE_WARNING, "channel destroyed");
        Status ChannelImpl::channelDisconnected = Status(
            Status::STATUSTYPE_WARNING, "channel disconnected");
        String emptyString;
        ConvertPtr convert = getConvert();

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
                
                Status m_status;
                m_status.deserialize(payloadBuffer, transport.get());
                
                try
                {
                    if (qos & QOS_INIT)
                    {
                        if (m_status.isSuccess())
                        {
                            // once created set destroy flag
                            m_mutex.lock();
                            m_initialized = true;
                            m_mutex.unlock();
                        }
                        
                        // we are initialized now, release pointer
                        // this is safe since at least caller owns it
                        m_thisPointer.reset();

                        initResponse(transport, version, payloadBuffer, qos, m_status);
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
                        
                        normalResponse(transport, version, payloadBuffer, qos, m_status);
                        
                        if (destroyReq)
                            destroy();
                    }
                }
                catch (std::exception &e) {
                    LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what());
                }
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__);
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

        Status BaseRequestImpl::notInitializedStatus = Status(Status::STATUSTYPE_ERROR, "request not initialized");
        Status BaseRequestImpl::destroyedStatus = Status(Status::STATUSTYPE_ERROR, "request destroyed");
        Status BaseRequestImpl::channelNotConnected = Status(Status::STATUSTYPE_ERROR, "channel not connected");
        Status BaseRequestImpl::channelDestroyed = Status(Status::STATUSTYPE_ERROR, "channel destroyed");
        Status BaseRequestImpl::otherRequestPendingStatus = Status(Status::STATUSTYPE_ERROR, "other request pending");
        Status BaseRequestImpl::invalidPutStructureStatus = Status(Status::STATUSTYPE_ERROR, "incompatible put structure");
        Status BaseRequestImpl::invalidPutArrayStatus = Status(Status::STATUSTYPE_ERROR, "incompatible put array");
        Status BaseRequestImpl::invalidBitSetLengthStatus = Status(Status::STATUSTYPE_ERROR, "invalid bit-set length");
        Status BaseRequestImpl::pvRequestNull = Status(Status::STATUSTYPE_ERROR, "pvRequest == 0");

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
                ChannelProcess::shared_pointer thisPointer(
                            new ChannelProcessRequestImpl(channel, callback, pvRequest),
                            delayed_destroyable_deleter()
                        );
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
                if (m_pvRequest == 0)
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
                ChannelGet::shared_pointer thisPointer(new ChannelGetImpl(channel, channelGetRequester, pvRequest), delayed_destroyable_deleter());
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

            // get structure container
            PVStructure::shared_pointer m_structure;
            BitSet::shared_pointer m_bitSet;
            
            // put reference store
            PVStructure::shared_pointer m_pvPutStructure;
            BitSet::shared_pointer m_pvPutBitSet;

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
                if (m_pvRequest == 0)
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
                ChannelPut::shared_pointer thisPointer(new ChannelPutImpl(channel, channelPutRequester, pvRequest), delayed_destroyable_deleter());
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
                        m_pvPutBitSet->serialize(buffer, control);
                        m_pvPutStructure->serialize(buffer, control, m_pvPutBitSet.get());
                        
                        // release references
                        m_pvPutBitSet.reset();
                        m_pvPutStructure.reset();
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
                    m_pvPutStructure = pvPutStructure;
                    m_pvPutBitSet = pvPutBitSet;
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

            // putGet reference store
            PVStructure::shared_pointer m_putPutData;
            BitSet::shared_pointer m_putPutDataBitSet;
            
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
                if (m_pvRequest == 0)
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
                ChannelPutGet::shared_pointer thisPointer(new ChannelPutGetImpl(channel, channelPutGetRequester, pvRequest), delayed_destroyable_deleter());
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
                        m_putPutDataBitSet->serialize(buffer, control);
                        m_putPutData->serialize(buffer, control, m_putPutDataBitSet.get());
                        
                        // release references
                        m_putPutDataBitSet.reset();
                        m_putPutData.reset();
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
                    m_putPutData = pvPutStructure;
                    m_putPutDataBitSet = bitSet;
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
                if (m_pvRequest == 0)
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
                ChannelRPC::shared_pointer thisPointer(new ChannelRPCImpl(channel, channelRPCRequester, pvRequest), delayed_destroyable_deleter());
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

            // data container (for get)
            PVArray::shared_pointer m_data;

            // reference store (for put
            PVArray::shared_pointer m_putData;
            
            size_t m_offset;
            size_t m_count;
            size_t m_stride;

            size_t m_length;
            size_t m_capacity;
            
            Mutex m_structureMutex;

            ChannelArrayImpl(ChannelImpl::shared_pointer const & channel, ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvRequest) :
                    BaseRequestImpl(channel, channelArrayRequester),
                    m_channelArrayRequester(channelArrayRequester),
                    m_pvRequest(pvRequest),
                    m_offset(0), m_count(0),
                    m_length(0), m_capacity(0)
            {
                PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelArray);
            }
            
            void activate()
            {
                if (m_pvRequest == 0)
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
                ChannelArray::shared_pointer thisPointer(new ChannelArrayImpl(channel, channelArrayRequester, pvRequest), delayed_destroyable_deleter());
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
                    SerializeHelper::writeSize(m_capacity, buffer, control);
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
                        m_putData->serialize(buffer, control, 0, m_count ? m_count : m_putData->getLength()); // put from 0 offset (see API doc), m_count == 0 means entire array
                        // release reference
                        m_putData.reset();
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
                    m_data = dynamic_pointer_cast<PVArray>(getPVDataCreate()->createPVField(field));
                }
                
                // notify
                ChannelArray::shared_pointer thisChannelArray = dynamic_pointer_cast<ChannelArray>(shared_from_this());
                EXCEPTION_GUARD(m_channelArrayRequester->channelArrayConnect(status, thisChannelArray, m_data->getArray()));
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
                        m_data->deserialize(payloadBuffer, transport.get());
                    }
                    
                    EXCEPTION_GUARD(m_channelArrayRequester->getArrayDone(status, thisChannelArray, m_data));
                }
                else if (qos & QOS_GET_PUT)
                {
                    EXCEPTION_GUARD(m_channelArrayRequester->setLengthDone(status, thisChannelArray));
                }
                else if (qos & QOS_PROCESS)
                {
                    size_t length = SerializeHelper::readSize(payloadBuffer, transport.get());
                    size_t capacity = SerializeHelper::readSize(payloadBuffer, transport.get());
                    
                    EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(status, thisChannelArray, length, capacity));
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
                
                if (!(*m_data->getArray() == *putArray->getArray()))
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
                        m_putData = putArray;
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

            virtual void setLength(size_t length, size_t capacity) {

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
						m_capacity = capacity;
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
                        EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(destroyedStatus, thisChannelArray, 0, 0));
                        return;
                    }
                    if (!m_initialized) {
                        EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(notInitializedStatus, thisChannelArray, 0, 0));
                        return;
                    }
                 }
                 
                if (!startRequest(m_lastRequest.get() ? QOS_DESTROY | QOS_PROCESS : QOS_PROCESS)) {
                    EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(otherRequestPendingStatus, thisChannelArray, 0, 0));
                    return;
                }

                try {
                    m_channel->checkAndGetTransport()->enqueueSendRequest(shared_from_this());
                } catch (std::runtime_error &rte) {
                    stopRequest();
                    EXCEPTION_GUARD(m_channelArrayRequester->getLengthDone(channelNotConnected, thisChannelArray, 0, 0));
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
            String m_subField;
            
            pvAccessID m_ioid;

            Mutex m_mutex;
            bool m_destroyed;
            
            ResponseRequest::shared_pointer m_thisPointer;
            
            ChannelGetFieldRequestImpl(ChannelImpl::shared_pointer const & channel, GetFieldRequester::shared_pointer const & callback, String const & subField) :
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
            static shared_pointer create(ChannelImpl::shared_pointer const & channel, GetFieldRequester::shared_pointer const & callback, String const & subField)
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



        class ElementQueue : public Monitor {
            public:
            virtual ~ElementQueue() {};
    		virtual void init(StructureConstPtr const & structure) = 0;
    		virtual void response(Transport::shared_pointer const & transport, ByteBuffer* payloadBuffer) = 0;
    	};
    	

       typedef Queue<MonitorElement> MonitorElementQueue;
       typedef std::tr1::shared_ptr<MonitorElementQueue> MonitorElementQueuePtr;

       class MultipleElementQueue :
            public ElementQueue,
            public std::tr1::enable_shared_from_this<MultipleElementQueue>
       {
    	   private:
    	   
           MonitorRequester::shared_pointer m_callback;
           int queueSize;
           bool queueIsFull;
           MonitorElementQueuePtr queue;
           BitSetPtr changedBitSet;
           BitSetPtr overrunBitSet;
           MonitorElementPtr latestMonitorElement;
    	   Mutex m_mutex;

    	   public:
    	   
    	    MultipleElementQueue(MonitorRequester::shared_pointer const & callback,int queueSize)
            :
    	       m_callback(callback),
               queueSize(queueSize),
               queueIsFull(false)
    	    {
    	    }
    	   
    	    virtual ~MultipleElementQueue()
    	    {
    	    }
    	    
            virtual void init(StructureConstPtr const & structure) {
                Lock guard(m_mutex);
                size_t nfields = 0;
                std::vector<MonitorElementPtr> monitorElementArray;
                monitorElementArray.reserve(queueSize);
                for(int i=0; i<queueSize; i++) {
                     PVStructurePtr pvStructure = getPVDataCreate()->createPVStructure(structure);
                     if(nfields==0) nfields = pvStructure->getNumberFields();
                     MonitorElementPtr monitorElement(
                         new MonitorElement(pvStructure));
                     monitorElementArray.push_back(monitorElement);
                }
                queue.reset(new MonitorElementQueue(monitorElementArray));
                changedBitSet.reset(new BitSet(nfields));
                overrunBitSet.reset(new BitSet(nfields));
            }
    
            virtual void response(Transport::shared_pointer const & transport, ByteBuffer* payloadBuffer) {
               {
                    Lock guard(m_mutex);
                    if(queueIsFull) {
                         MonitorElementPtr monitorElement = latestMonitorElement;
                         PVStructurePtr pvStructure = monitorElement->pvStructurePtr;
                         changedBitSet->deserialize(payloadBuffer, transport.get());
                         pvStructure->deserialize(
                              payloadBuffer,
                              transport.get(),
                              changedBitSet.get());
                         overrunBitSet->deserialize(payloadBuffer, transport.get());
                         (*monitorElement->changedBitSet)|= (*changedBitSet);
                         (*monitorElement->overrunBitSet)|= (*changedBitSet);
                         changedBitSet->clear();
                         overrunBitSet->clear();
                         return;
                    }
                     MonitorElementPtr monitorElement = queue->getFree();
                     if(monitorElement==NULL) {
                         throw  std::logic_error(String("RealQueue::dataChanged() logic error"));
                     }
                     if(queue->getNumberFree()==0){
                         queueIsFull = true;
                         latestMonitorElement = monitorElement;
                     }
                     PVStructurePtr pvStructure = monitorElement->pvStructurePtr;
                     changedBitSet->deserialize(payloadBuffer, transport.get());
                     pvStructure->deserialize(
                          payloadBuffer,
                          transport.get(),
                          changedBitSet.get());
                     overrunBitSet->deserialize(payloadBuffer, transport.get());
                     BitSetUtil::compress(changedBitSet,pvStructure);
                     BitSetUtil::compress(overrunBitSet,pvStructure);
                     monitorElement->changedBitSet->clear();
                     (*monitorElement->changedBitSet)|=(*changedBitSet);
                     monitorElement->overrunBitSet->clear();
                     (*monitorElement->overrunBitSet)|=(*overrunBitSet);
                     changedBitSet->clear();
                     overrunBitSet->clear();
                     queue->setUsed(monitorElement);
                 }
                 EXCEPTION_GUARD(m_callback->monitorEvent(shared_from_this()));
            }
    
            virtual MonitorElement::shared_pointer poll() {
                Lock guard(m_mutex);
                return queue->getUsed();
            }
            	
            virtual void release(MonitorElement::shared_pointer const & currentElement) {
                Lock guard(m_mutex);
                if(queueIsFull) {
                    MonitorElementPtr monitorElement = latestMonitorElement;
                    PVStructurePtr pvStructure = monitorElement->pvStructurePtr;
                    BitSetUtil::compress(monitorElement->changedBitSet,pvStructure);
                    BitSetUtil::compress(monitorElement->overrunBitSet,pvStructure);
                    queueIsFull = false;
                    latestMonitorElement.reset();
                }
                queue->releaseUsed(currentElement);
    	    }
    
    	    Status start() {
                Lock guard(m_mutex);
                queue->clear();
                changedBitSet->clear();
                overrunBitSet->clear();
                queueIsFull = false;
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
            
            std::tr1::shared_ptr<ElementQueue> m_ElementQueue;

            ChannelMonitorImpl(
                ChannelImpl::shared_pointer const & channel,
                MonitorRequester::shared_pointer const & monitorRequester,
                PVStructure::shared_pointer const & pvRequest)
                :
                    BaseRequestImpl(channel, monitorRequester),
                    m_monitorRequester(monitorRequester),
                    m_started(false),
                    m_pvRequest(pvRequest)
            {
                PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(channelMonitor);
            }
            
            void activate()
            {
                if (m_pvRequest == 0)
                {
                    Monitor::shared_pointer thisPointer = dynamic_pointer_cast<Monitor>(shared_from_this());
                    EXCEPTION_GUARD(m_monitorRequester->monitorConnect(pvRequestNull, thisPointer, StructureConstPtr()));
                    return;
                }
                
               int queueSize = 2;
               PVFieldPtr pvField = m_pvRequest->getSubField("record._options");
               if (pvField.get()) {
                   PVStructurePtr pvOptions = static_pointer_cast<PVStructure>(pvField);
                   pvField = pvOptions->getSubField("queueSize");
                   if (pvField.get()) {
                       PVStringPtr pvString = pvOptions->getStringField("queueSize");
                       if(pvString.get()!=NULL) {
                           int32 size;
                           std::stringstream ss;
                           ss << pvString->get();
                           ss >> size;
                           queueSize = size;
                       }
                   }
               }
 
                BaseRequestImpl::activate();

               if(queueSize<2) queueSize = 2;
               m_ElementQueue.reset(new MultipleElementQueue(m_monitorRequester,queueSize));
                
                // subscribe
                try {
                    resubscribeSubscription(m_channel->checkDestroyedAndGetTransport());
                } catch (std::runtime_error &rte) {
                    Monitor::shared_pointer thisPointer = dynamic_pointer_cast<Monitor>(shared_from_this());
                    EXCEPTION_GUARD(m_monitorRequester->monitorConnect(channelDestroyed, thisPointer, StructureConstPtr()));
                    BaseRequestImpl::destroy(true);
                }
            }

        public:
            static Monitor::shared_pointer create(
               ChannelImpl::shared_pointer const & channel,
               MonitorRequester::shared_pointer const & monitorRequester,
               PVStructure::shared_pointer const & pvRequest)
            {
                Monitor::shared_pointer thisPointer(
                    new ChannelMonitorImpl(
                        channel, monitorRequester,
                        pvRequest),
                        delayed_destroyable_deleter());
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
                m_ElementQueue->init(structure);
                
                // notify
                Monitor::shared_pointer thisChannelMonitor = dynamic_pointer_cast<Monitor>(shared_from_this());
                EXCEPTION_GUARD(m_monitorRequester->monitorConnect(status, thisChannelMonitor, structure));

                if (m_started)
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
                else
                {
        			m_ElementQueue->response(transport, payloadBuffer);
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
                    
                m_ElementQueue->start();

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

                m_ElementQueue->stop();

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
                return m_ElementQueue->poll();
            }

            virtual void release(MonitorElement::shared_pointer const & monitorElement)
            {
                m_ElementQueue->release(monitorElement);
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
            AbstractClientResponseHandler(ClientContextImpl::shared_pointer const & context, String const & description) :
                    AbstractResponseHandler(context.get(), description), _context(ClientContextImpl::weak_pointer(context)) {
            }

            virtual ~AbstractClientResponseHandler() {
            }
        };

        class NoopResponse : public AbstractClientResponseHandler, private epics::pvData::NoDefaultMethods {
        public:
            NoopResponse(ClientContextImpl::shared_pointer const & context, String const & description) :
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

                transport->ensureData(5);
                int32 searchSequenceId = payloadBuffer->getInt();
                bool found = payloadBuffer->getByte() != 0;
                if (!found)
                    return;

                transport->ensureData((128+2*16)/8);

                osiSockAddr serverAddress;
                serverAddress.ia.sin_family = AF_INET;

                // 128-bit IPv6 address
                /*
        int8* byteAddress = new int8[16];
        for (int i = 0; i < 16; i++) 
          byteAddress[i] = payloadBuffer->getByte(); };
                */

                // IPv4 compatible IPv6 address expected
                // first 80-bit are 0
                if (payloadBuffer->getLong() != 0) return;
                if (payloadBuffer->getShort() != 0) return;
                if (payloadBuffer->getShort() != (int16)0xFFFF) return;

                // accept given address if explicitly specified by sender
                serverAddress.ia.sin_addr.s_addr = htonl(payloadBuffer->getInt());
                if (serverAddress.ia.sin_addr.s_addr == INADDR_ANY)
                    serverAddress.ia.sin_addr = responseFrom->ia.sin_addr;

                serverAddress.ia.sin_port = htons(payloadBuffer->getShort());
                
                // reads CIDs
                // TODO optimize
                std::tr1::shared_ptr<epics::pvAccess::ChannelSearchManager> csm = _context.lock()->getChannelSearchManager();
                int16 count = payloadBuffer->getShort();
                for (int i = 0; i < count; i++)
                {
                    transport->ensureData(4);
                    pvAccessID cid = payloadBuffer->getInt();
                    csm->searchResponse(cid, searchSequenceId, version, &serverAddress);
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

                transport->ensureData((2*sizeof(int16)+2*sizeof(int32)+128)/sizeof(int8));

                int16 sequentalID = payloadBuffer->getShort();
                TimeStamp startupTimestamp(payloadBuffer->getLong(),payloadBuffer->getInt());

                osiSockAddr serverAddress;
                serverAddress.ia.sin_family = AF_INET;

                // 128-bit IPv6 address
                /*
        int8* byteAddress = new int8[16];
        for (int i = 0; i < 16; i++) 
          byteAddress[i] = payloadBuffer->getByte(); };
                */

                // IPv4 compatible IPv6 address expected
                // first 80-bit are 0
                if (payloadBuffer->getLong() != 0) return;
                if (payloadBuffer->getShort() != 0) return;
                if (payloadBuffer->getShort() != (int16)0xFFFF) return;

                // accept given address if explicitly specified by sender
                serverAddress.ia.sin_addr.s_addr = htonl(payloadBuffer->getInt());
                if (serverAddress.ia.sin_addr.s_addr == INADDR_ANY)
                    serverAddress.ia.sin_addr = responseFrom->ia.sin_addr;

                serverAddress.ia.sin_port = htons(payloadBuffer->getShort());

                // TODO optimize
                ClientContextImpl::shared_pointer context = _context.lock();
                if (!context)
                    return;
                
                std::tr1::shared_ptr<epics::pvAccess::BeaconHandler> beaconHandler = context->getBeaconHandler(responseFrom);
                // currently we care only for servers used by this context
                if (beaconHandler == 0)
                    return;

                // extra data
                PVFieldPtr data;
                const FieldConstPtr field = getFieldCreate()->deserialize(payloadBuffer, transport.get());
                if (field != 0)
                {
                    data = getPVDataCreate()->createPVField(field);
                    data->deserialize(payloadBuffer, transport.get());
                }

                // notify beacon handler
                beaconHandler->beaconNotify(responseFrom, version, &timestamp, &startupTimestamp, sequentalID, data);
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

                transport->ensureData(8);
                transport->setRemoteTransportReceiveBufferSize(payloadBuffer->getInt());
                transport->setRemoteTransportSocketReceiveBufferSize(payloadBuffer->getInt());

                transport->setRemoteRevision(version);
                TransportSender::shared_pointer sender = dynamic_pointer_cast<TransportSender>(transport);
                if (sender.get()) {
                    transport->enqueueSendRequest(sender);
                }
                transport->verified();

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
                            String message = SerializeHelper::deserializeString(payloadBuffer, transport.get());
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
                        channel->createChannelFailed();
                        return;
                    }

                    //int16 acl = payloadBuffer->getShort();

                    channel->connectionCompleted(sid);
                }
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
                m_handlerTable[CMD_SEARCH].reset(new NoopResponse(context, "Search")); /*  3 */
                m_handlerTable[CMD_SEARCH_RESPONSE].reset(new SearchResponseHandler(context)); /*  4 */
                m_handlerTable[CMD_INTROSPECTION_SEARCH].reset(new NoopResponse(context, "Introspection search")); /*  5 */
                m_handlerTable[CMD_INTROSPECTION_SEARCH_RESPONSE] = dataResponse; /*  6 - introspection search */
                m_handlerTable[CMD_CREATE_CHANNEL].reset(new CreateChannelHandler(context)); /*  7 */
                m_handlerTable[CMD_DESTROY_CHANNEL].reset(new NoopResponse(context, "Destroy channel")); /*  8 */ // TODO it might be useful to implement this...
                m_handlerTable[CMD_RESERVED0] = badResponse; /*  9 */
                m_handlerTable[CMD_GET] = dataResponse; /* 10 - get response */
                m_handlerTable[CMD_PUT] = dataResponse; /* 11 - put response */
                m_handlerTable[CMD_PUT_GET] = dataResponse; /* 12 - put-get response */
                m_handlerTable[CMD_MONITOR] = dataResponse; /* 13 - monitor response */
                m_handlerTable[CMD_ARRAY] = dataResponse; /* 14 - array response */
                m_handlerTable[CMD_DESTROY_REQUEST] = badResponse; /* 15 - destroy request */
                m_handlerTable[CMD_PROCESS] = dataResponse; /* 16 - process response */
                m_handlerTable[CMD_GET_FIELD] = dataResponse; /* 17 - get field response */
                m_handlerTable[CMD_MESSAGE].reset(new MessageHandler(context)); /* 18 - message to Requester */
                m_handlerTable[CMD_MULTIPLE_DATA] = badResponse; // TODO new MultipleDataResponseHandler(context), /* 19 - grouped monitors */
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
            public std::tr1::enable_shared_from_this<InternalClientContextImpl>
        {


            class ChannelProviderImpl;
/*
            class ChannelImplFind : public ChannelFind
            {
            public:
                ChannelImplFind(ChannelProvider::shared_pointer const & provider) : m_provider(provider)
                {
                }

                virtual void destroy()
                {
                    // one instance for all, do not delete at all
                }

                virtual ChannelProvider::shared_pointer getChannelProvider()
                {
                    return m_provider;
                };

                virtual void cancel()
                {
                    throw std::runtime_error("not supported");
                }

            private:

                // only to be destroyed by it
                friend class ChannelProviderImpl;
                virtual ~ChannelImplFind() {}

                ChannelProvider::shared_pointer m_provider;
            };
*/
            class ChannelProviderImpl : public ChannelProvider {
            public:

                ChannelProviderImpl(std::tr1::shared_ptr<ClientContextImpl> const & context) :
                        m_context(context)
                {
                    MB_INIT;
                }

                virtual epics::pvData::String getProviderName()
                {
                    return PROVIDER_NAME;
                }

                virtual void destroy()
                {
                }

                virtual ChannelFind::shared_pointer channelFind(
                        epics::pvData::String const & channelName,
                        ChannelFindRequester::shared_pointer const & channelFindRequester)
                {
                    // TODO not implemented

                	std::tr1::shared_ptr<ClientContextImpl> context = m_context.lock();
                	if (context.get())
                		context->checkChannelName(channelName);

                    if (!channelFindRequester.get())
                        throw std::runtime_error("null requester");

                    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
                    ChannelFind::shared_pointer nullChannelFind;
                    EXCEPTION_GUARD(channelFindRequester->channelFindResult(errorStatus, nullChannelFind, false));
                    return nullChannelFind;
                }

                virtual Channel::shared_pointer createChannel(
                        epics::pvData::String const & channelName,
                        ChannelRequester::shared_pointer const & channelRequester,
                        short priority)
                {
                    return createChannel(channelName, channelRequester, priority, emptyString);
                }

                virtual Channel::shared_pointer createChannel(
                        epics::pvData::String const & channelName,
                        ChannelRequester::shared_pointer const & channelRequester,
                        short priority,
                        epics::pvData::String const & addressesStr)
                {
                	std::tr1::shared_ptr<ClientContextImpl> context = m_context.lock();
                	if (!context.get())
                	{
                        Status errorStatus(Status::STATUSTYPE_ERROR, "context already destroyed");
                        Channel::shared_pointer nullChannel;
                        EXCEPTION_GUARD(channelRequester->channelCreated(errorStatus, nullChannel));
                        return nullChannel;
                	}

                    auto_ptr<InetAddrVector> addresses;
                    if (!addressesStr.empty())
                        addresses.reset(getSocketAddressList(addressesStr, PVA_SERVER_PORT));
                    if (addresses->empty())
                        addresses.reset();
                        
                    Channel::shared_pointer channel = context->createChannelInternal(channelName, channelRequester, priority, addresses);
                    if (channel.get())
                        channelRequester->channelCreated(Status::Ok, channel);
                    return channel;

                    // NOTE it's up to internal code to respond w/ error to requester and return 0 in case of errors
                }

                virtual void configure(epics::pvData::PVStructure::shared_pointer configuration)
                {
                    std::tr1::shared_ptr<ClientContextImpl> context = m_context.lock();
                    if (context.get())
                        context->configure(configuration);
                }

                virtual void flush()
                {
                    std::tr1::shared_ptr<ClientContextImpl> context = m_context.lock();
                    if (context.get())
                        context->flush();
                }

                virtual void poll()
                {
                    std::tr1::shared_ptr<ClientContextImpl> context = m_context.lock();
                    if (context.get())
                        context->poll();
                }

                ~ChannelProviderImpl() {};

            private:

                std::tr1::weak_ptr<ClientContextImpl> m_context;
            };

            
            
            
            
            
            /**
             * Implementation of PVAJ JCA <code>Channel</code>.
             */
            class InternalChannelImpl :
                public ChannelImpl,
                public std::tr1::enable_shared_from_this<InternalChannelImpl>
            {
            private:
                
                /**
                 * Context.
                 */
                ClientContextImpl::shared_pointer m_context;
                
                /**
                 * Client channel ID.
                 */
                pvAccessID m_channelID;
                
                /**
                 * Channel name.
                 */
                String m_name;
                
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
                
                /**
                 * Reference counting.
                 * NOTE: synced on <code>m_channelMutex</code>.
                 */
                int m_references;
                
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
                 * Constructor.
                 * @param context
                 * @param name
                 * @param listener
                 * @throws PVAException
                 */
                InternalChannelImpl(
                                    ClientContextImpl::shared_pointer const & context,
                                    pvAccessID channelID,
                                    String const & name,
                                    ChannelRequester::shared_pointer const & requester,
                                    short priority,
                                    auto_ptr<InetAddrVector>& addresses) :
                m_context(context),
                m_channelID(channelID),
                m_name(name),
                m_requester(requester),
                m_priority(priority),
                m_addresses(addresses),
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
                
                static ChannelImpl::shared_pointer create(ClientContextImpl::shared_pointer context,
                                                   pvAccessID channelID,
                                                   String const & name,
                                                   ChannelRequester::shared_pointer requester,
                                                   short priority,
                                                   auto_ptr<InetAddrVector>& addresses)
                {
                    ChannelImpl::shared_pointer thisPointer(new InternalChannelImpl(context, channelID, name, requester, priority, addresses), delayed_destroyable_deleter());
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
                
                virtual String getRequesterName()
                {
                    return getChannelName();
                };
                
                virtual void message(String const & message,MessageType messageType)
                {
                    std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
                }
                
                int32_t& getUserValue() { return m_userValue; }
                
                virtual ChannelProvider::shared_pointer getProvider()
                {
                    return m_context->getProvider();
                }
                
                // NOTE: synchronization guarantees that <code>transport</code> is non-<code>0</code> and <code>state == CONNECTED</code>.
                virtual epics::pvData::String getRemoteAddress()
                {
                    Lock guard(m_channelMutex);
                    if (m_connectionState != CONNECTED) {
                        static String emptyString;
                        return emptyString;
                    }
                    else
                    {
                        return inetAddressToString(*m_transport->getRemoteAddress());
                    }
                }
                
                virtual epics::pvData::String getChannelName()
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
                
                virtual String getSearchInstanceName() {
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
                    // ... and search again
                    initiateSearch();
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
                        m_transport.reset();
                    }
                    
                    if (initiateSearch)
                        this->initiateSearch();
                    
                }
                
                /**
                 * Initiate search (connect) procedure.
                 */
                void initiateSearch()
                {
                    Lock guard(m_channelMutex);
                    
                    m_allowCreation = true;
                    
                    if (!m_addresses.get())
                    {
                        m_context->getChannelSearchManager()->registerSearchInstance(shared_from_this());
                    }
                    else if (!m_addresses->empty())
                    {
                        // TODO not only first !!!
                        // TODO minor version !!!
                        // TODO what to do if there is no channel, do not search in a loop!!! do this in other thread...!
                        searchResponse(PVA_PROTOCOL_REVISION, &((*m_addresses)[0]));
                    }
                }
                
                virtual void searchResponse(int8 minorRevision, osiSockAddr* serverAddress) {
                    Lock guard(m_channelMutex);
                    Transport::shared_pointer transport = m_transport;
                    if (transport.get())
                    {
                        // multiple defined PV or reconnect request (same server address)
                        if (!sockAddrAreIdentical(transport->getRemoteAddress(), serverAddress))
                        {
                            EXCEPTION_GUARD(m_requester->message("More than one channel with name '" + m_name +
                                                                 "' detected, additional response from: " + inetAddressToString(*serverAddress), warningMessage));
                            return;
                        }
                    }
                    
                    transport = m_context->getTransport(shared_from_this(), serverAddress, minorRevision, m_priority);
                    if (!transport.get())
                    {
                        createChannelFailed();
                        return;
                    }
                    
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
                        control->startMessage((int8)7, 2+4);
                        
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
                        control->startMessage((int8)8, 4+4);
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
                        if(ptr = rrs[i].lock())
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
                
                virtual void getField(GetFieldRequester::shared_pointer const & requester,epics::pvData::String const & subField)
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
                
                virtual epics::pvData::Monitor::shared_pointer createMonitor(
                                                            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
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
                    String info;
                    printInfo(&info);
                    std::cout << info.c_str() << std::endl;
                }
                
                virtual void printInfo(epics::pvData::StringBuilder out) {
                    //Lock lock(m_channelMutex);
                    //std::ostringstream ostr;
                    //static String emptyString;
                    
                    out->append(  "CHANNEL  : "); out->append(m_name);
                    out->append("\nSTATE    : "); out->append(ConnectionStateNames[m_connectionState]);
                    if (m_connectionState == CONNECTED)
                    {
                        out->append("\nADDRESS  : "); out->append(getRemoteAddress());
                        //out->append("\nRIGHTS   : "); out->append(getAccessRights());
                    }
                    out->append("\n");
                }
            };
            
            
            
            
            

        private:
            
            InternalClientContextImpl() :
                    m_addressList(""), m_autoAddressList(true), m_connectionTimeout(30.0f), m_beaconPeriod(15.0f),
                    m_broadcastPort(PVA_BROADCAST_PORT), m_receiveBufferSize(MAX_TCP_RECV),
                    m_namedLocker(), m_lastCID(0), m_lastIOID(0),
                    m_version("pvAccess Client", "cpp", 
                                EPICS_PVA_MAJOR_VERSION,
                                EPICS_PVA_MINOR_VERSION,
                                EPICS_PVA_MAINTENANCE_VERSION,
                                EPICS_PVA_DEVELOPMENT_FLAG),
                    m_contextState(CONTEXT_NOT_INITIALIZED),
                    m_configuration(new SystemConfigurationImpl()),
                    m_flushStrategy(DELAYED)
            {
                PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(remoteClientContext);
                m_flushTransports.reserve(64);
                loadConfiguration();
            }

        public:
            
            static shared_pointer create()
            {
                shared_pointer thisPointer(new InternalClientContextImpl(), delayed_destroyable_deleter());
                static_cast<InternalClientContextImpl*>(thisPointer.get())->activate();
                return thisPointer;
            }

            void activate()
            {
            	m_provider.reset(new ChannelProviderImpl(shared_from_this()));
            }

            virtual Configuration::shared_pointer getConfiguration() {
                /*
TODO
        final ConfigurationProvider configurationProvider = ConfigurationFactory.getProvider();
        Configuration config = configurationProvider.getConfiguration("pvAccess-client");
        if (config == 0)
            config = configurationProvider.getConfiguration("system");
        return config;
*/
                return m_configuration;
            }

            virtual const Version& getVersion() {
                return m_version;
            }

            virtual ChannelProvider::shared_pointer const & getProvider() {
                return m_provider;
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
                String info;
                printInfo(&info);
                std::cout << info.c_str() << std::endl;
            }

            virtual void printInfo(epics::pvData::StringBuilder out) {
                Lock lock(m_contextMutex);
                std::ostringstream ostr;
                static String emptyString;

                out->append(  "CLASS : ::epics::pvAccess::ClientContextImpl");
                out->append("\nVERSION : "); out->append(m_version.getVersionString());
                out->append("\nADDR_LIST : "); ostr << m_addressList; out->append(ostr.str()); ostr.str(emptyString);
                out->append("\nAUTO_ADDR_LIST : ");  out->append(m_autoAddressList ? "true" : "false");
                out->append("\nCONNECTION_TIMEOUT : "); ostr << m_connectionTimeout; out->append(ostr.str()); ostr.str(emptyString);
                out->append("\nBEACON_PERIOD : "); ostr << m_beaconPeriod; out->append(ostr.str()); ostr.str(emptyString);
                out->append("\nBROADCAST_PORT : "); ostr << m_broadcastPort; out->append(ostr.str()); ostr.str(emptyString);
                out->append("\nRCV_BUFFER_SIZE : "); ostr << m_receiveBufferSize; out->append(ostr.str()); ostr.str(emptyString);
                out->append("\nSTATE : ");
                switch (m_contextState)
                {
                case CONTEXT_NOT_INITIALIZED:
                    out->append("CONTEXT_NOT_INITIALIZED");
                    break;
                case CONTEXT_INITIALIZED:
                    out->append("CONTEXT_INITIALIZED");
                    break;
                case CONTEXT_DESTROYED:
                    out->append("CONTEXT_DESTROYED");
                    break;
                default:
                    out->append("UNKNOWN");
                }
                out->append("\n");
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
                } catch (std::exception& ex) { printf("dispose(): %s\n", ex.what()); // tODO remove
                } catch (...) { /* TODO log with low level */ }
            }

            ~InternalClientContextImpl()
            {
                PVACCESS_REFCOUNT_MONITOR_DESTRUCT(remoteClientContext);
            };

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
                m_connector.reset(new BlockingTCPConnector(thisPointer, m_receiveBufferSize, m_beaconPeriod));
                m_transportRegistry.reset(new TransportRegistry());

                // setup search manager
                m_channelSearchManager = SimpleChannelSearchManagerImpl::create(thisPointer);

                // TODO put memory barrier here... (if not already called withing a lock?)

                // setup UDP transport
                initializeUDPTransport();

                // TODO what if initialization failed!!!
            }

            /**
             * Initialized UDP transport (broadcast socket and repeater connection).
             */
            bool initializeUDPTransport() {

                // quary broadcast addresses of all IFs
                SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
                if (socket == INVALID_SOCKET) return false;
                auto_ptr<InetAddrVector> broadcastAddresses(getBroadcastAddresses(socket, m_broadcastPort));
                epicsSocketDestroy (socket);

                // set broadcast address list
                if (!m_addressList.empty())
                {
                    // if auto is true, add it to specified list
                    InetAddrVector* appendList = 0;
                    if (m_autoAddressList)
                        appendList = broadcastAddresses.get();

                    auto_ptr<InetAddrVector> list(getSocketAddressList(m_addressList, m_broadcastPort, appendList));
                    if (list.get() && list->size()) {
                        // delete old list and take ownership of a new one
                        broadcastAddresses = list;
                    }
                }
                
                for (size_t i = 0; broadcastAddresses.get() && i < broadcastAddresses->size(); i++)
                    LOG(logLevelDebug,
                        "Broadcast address #%d: %s", i, inetAddressToString((*broadcastAddresses)[i]).c_str());

                // where to bind (listen) address
                osiSockAddr listenLocalAddress;
                listenLocalAddress.ia.sin_family = AF_INET;
                listenLocalAddress.ia.sin_port = htons(m_broadcastPort);
                listenLocalAddress.ia.sin_addr.s_addr = htonl(INADDR_ANY);
                
                ClientContextImpl::shared_pointer thisPointer = shared_from_this();

                TransportClient::shared_pointer nullTransportClient;

                auto_ptr<ResponseHandler> clientResponseHandler(new ClientResponseHandler(thisPointer));
                auto_ptr<BlockingUDPConnector> broadcastConnector(new BlockingUDPConnector(true, true));
                m_broadcastTransport = static_pointer_cast<BlockingUDPTransport>(broadcastConnector->connect(
                        nullTransportClient, clientResponseHandler,
                        listenLocalAddress, PVA_PROTOCOL_REVISION,
                        PVA_DEFAULT_PRIORITY));
                if (!m_broadcastTransport.get())
                    return false;
                m_broadcastTransport->setBroadcastAddresses(broadcastAddresses.get());

                // undefined address
                osiSockAddr undefinedAddress;
                undefinedAddress.ia.sin_family = AF_INET;
                undefinedAddress.ia.sin_port = htons(0);
                undefinedAddress.ia.sin_addr.s_addr = htonl(INADDR_ANY);

                clientResponseHandler.reset(new ClientResponseHandler(thisPointer));
                auto_ptr<BlockingUDPConnector> searchConnector(new BlockingUDPConnector(false, true));
                m_searchTransport = static_pointer_cast<BlockingUDPTransport>(searchConnector->connect(
                        nullTransportClient, clientResponseHandler,
                        undefinedAddress, PVA_PROTOCOL_REVISION,
                        PVA_DEFAULT_PRIORITY));
                if (!m_searchTransport.get())
                    return false;
                m_searchTransport->setBroadcastAddresses(broadcastAddresses.get());

                // become active
                m_broadcastTransport->start();
                m_searchTransport->start();

                return true;
            }

            void internalDestroy() {

                //
                // cleanup
                //

                // this will also close all PVA transports
                destroyAllChannels();
                
                // stop UDPs
                m_searchTransport->close();
                m_broadcastTransport->close();
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
            void checkChannelName(String const & name) {
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
             * @param responseFrom remote source address of received beacon.    
             * @return beacon handler for particular server.
             */
            BeaconHandler::shared_pointer getBeaconHandler(osiSockAddr* responseFrom)
            {
                Lock guard(m_beaconMapMutex);
                AddressBeaconHandlerMap::iterator it = m_beaconHandlers.find(*responseFrom);
                BeaconHandler::shared_pointer handler;
                if (it == m_beaconHandlers.end())
                {
                    handler.reset(new BeaconHandler(shared_from_this(), responseFrom));
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
                    // TODO we are creating a new response handler even-though we might not need a new transprot !!!
                    auto_ptr<ResponseHandler> handler(new ClientResponseHandler(shared_from_this()));
                    Transport::shared_pointer t = m_connector->connect(client, handler, *serverAddress, minorRevision, priority);
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
            ChannelImpl::shared_pointer createChannelInternal(String const & name, ChannelRequester::shared_pointer const & requester, short priority,
                                               auto_ptr<InetAddrVector>& addresses) { // TODO addresses

                checkState();
                checkChannelName(name);

                if (requester == 0)
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

                String name = channel->getChannelName();
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
            {
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
            String m_addressList;

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
            int m_broadcastPort;

            /**
             * Receive buffer size (max size of payload).
             */
            int m_receiveBufferSize;

            /**
             * Timer.
             */
            Timer::shared_pointer m_timer;

            /**
             * Broadcast transport needed to listen for broadcasts.
             */
            BlockingUDPTransport::shared_pointer m_broadcastTransport;

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
             * Context instance.
             */
            NamedLockPattern<String> m_namedLocker;

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

            /**
             * Provider implementation.
             */
            ChannelProviderImpl::shared_pointer m_provider;

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

        ClientContextImpl::shared_pointer createClientContextImpl()
        {
            ClientContextImpl::shared_pointer ptr = InternalClientContextImpl::create();
            return ptr;
        }

    }};

