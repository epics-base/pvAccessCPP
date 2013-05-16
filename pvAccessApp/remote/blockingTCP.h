/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef BLOCKINGTCP_H_
#define BLOCKINGTCP_H_

#include <pv/pvaConstants.h>
#include <pv/remote.h>
#include <pv/transportRegistry.h>
#include <pv/introspectionRegistry.h>
#include <pv/namedLockPattern.h>
#include <pv/inetAddressUtil.h>

#include <pv/byteBuffer.h>
#include <pv/pvType.h>
#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/event.h>

#include <osdSock.h>
#include <osiSock.h>
#include <epicsTime.h>
#include <epicsThread.h>

#include <set>
#include <map>
#include <deque>

// not implemented anyway
#define FLOW_CONTROL 0

namespace epics {
    namespace pvAccess {

        //class MonitorSender;

        enum ReceiveStage {
            READ_FROM_SOCKET, PROCESS_HEADER, PROCESS_PAYLOAD, UNDEFINED_STAGE
        };

        class BlockingTCPTransport :
                public Transport,
                public TransportSendControl,
                public std::tr1::enable_shared_from_this<BlockingTCPTransport>
        {
        protected:
            BlockingTCPTransport(Context::shared_pointer const & context, SOCKET channel,
                    std::auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize,
                    epics::pvData::int16 priority);
                    
        public:
            virtual bool isClosed() {
                return _closed.get();
            }

            virtual epics::pvData::int8 getRevision() const {
                return PVA_PROTOCOL_REVISION;
            }

            virtual void setRemoteRevision(epics::pvData::int8 revision) {
                _remoteTransportRevision = revision;
            }

            virtual void setRemoteTransportReceiveBufferSize(std::size_t remoteTransportReceiveBufferSize) {
                _remoteTransportReceiveBufferSize = remoteTransportReceiveBufferSize;
            }

            virtual void setRemoteTransportSocketReceiveBufferSize(std::size_t socketReceiveBufferSize) {
                _remoteTransportSocketReceiveBufferSize = socketReceiveBufferSize;
            }

            virtual epics::pvData::String getType() const {
                return epics::pvData::String("TCP");
            }

            virtual void aliveNotification() {
                // noop
            }

            virtual void changedTransport() {
                // noop
            }

            virtual const osiSockAddr* getRemoteAddress() const {
                return &_socketAddress;
            }

            virtual epics::pvData::int16 getPriority() const {
                return _priority;
            }

            virtual std::size_t getReceiveBufferSize() const {
                return _socketBuffer->getSize();
            }

            /**
             * Get remote transport receive buffer size (in bytes).
             * @return remote transport receive buffer size
             */
            virtual std::size_t getRemoteTransportReceiveBufferSize() const {
                return _remoteTransportReceiveBufferSize;
            }

            virtual std::size_t getSocketReceiveBufferSize() const;

            virtual bool verify(epics::pvData::int32 timeoutMs) {
                return _verifiedEvent.wait(timeoutMs/1000.0);

                //epics::pvData::Lock lock(_verifiedMutex);
                //return _verified;
            }

            virtual void verified() {
                epics::pvData::Lock lock(_verifiedMutex);
                _verified = true;
                _verifiedEvent.signal();
            }

            virtual void setRecipient(const osiSockAddr& /*sendTo*/) {
                // noop
            }

            virtual void flush(bool lastMessageCompleted);
            virtual void startMessage(epics::pvData::int8 command, std::size_t ensureCapacity);
            virtual void endMessage();

            virtual void flushSerializeBuffer() {
                flush(false);
            }

            virtual void ensureBuffer(std::size_t size);

            virtual void alignBuffer(std::size_t alignment);

            virtual void ensureData(std::size_t size);

            virtual void alignData(std::size_t alignment);

            virtual bool directSerialize(epics::pvData::ByteBuffer *existingBuffer, const char* toSerialize,
                                         std::size_t elementCount, std::size_t elementSize);

            virtual bool directDeserialize(epics::pvData::ByteBuffer *existingBuffer, char* deserializeTo,
                                           std::size_t elementCount, std::size_t elementSize);

            void processReadIntoDirectBuffer(std::size_t bytesToRead);

            virtual void close();

            virtual void setByteOrder(int /*byteOrder*/)
            {
            	// not used this this implementation
            }

            FlushStrategy getFlushStrategy() {
                return _flushStrategy;
            }

            void setFlushStrategy(FlushStrategy flushStrategy) {
                _flushStrategy = flushStrategy;
            }

            //void requestFlush();

            /**
             * Close and free connection resources.
             */
            void freeConnectionResorces();

            /**
             * Starts the receive and send threads
             */
            virtual void start();

            virtual void enqueueSendRequest(TransportSender::shared_pointer const & sender);

            //void enqueueMonitorSendRequest(TransportSender::shared_pointer const & sender);

            virtual void enqueueOnlySendRequest(TransportSender::shared_pointer const & sender);
         
            virtual void flushSendQueue();
        
            virtual void cachedSerialize(
            const std::tr1::shared_ptr<const epics::pvData::Field>& field, epics::pvData::ByteBuffer* buffer)
            {
        		outgoingIR.serialize(field, buffer, this);
            }

            virtual std::tr1::shared_ptr<const epics::pvData::Field>
            cachedDeserialize(epics::pvData::ByteBuffer* buffer)
            {
               return incomingIR.deserialize(buffer, this);
            }

        protected:
        
            virtual void processReadCached(bool nestedCall,
                    ReceiveStage inStage, std::size_t requiredBytes);

            /**
             * Called to any resources just before closing transport
             * @param[in] force   flag indicating if forced (e.g. forced
             * disconnect) is required
             */
            virtual void internalClose(bool force);

            /**
             * Called to any resources just after closing transport and without any locks held on transport
             * @param[in] force   flag indicating if forced (e.g. forced
             * disconnect) is required
             */
            virtual void internalPostClose(bool force);

            /**
             * Send a buffer through the transport.
             * NOTE: TCP sent buffer/sending has to be synchronized (not done by this method).
             * @param buffer[in]    buffer to be sent
             * @return success indicator
             */
            virtual bool send(epics::pvData::ByteBuffer* buffer);

            virtual ~BlockingTCPTransport();


#if FLOW_CONTROL
            /**
             * Default marker period.
             */
            static const std::size_t MARKER_PERIOD = 1024;
#endif
            static const std::size_t MAX_ENSURE_DATA_BUFFER_SIZE = 1024;

// TODO
              double _delay;

            /****** finally initialized at construction time and after start (called by the same thread) ********/
            
            /**
             * Corresponding channel.
             */
            SOCKET _channel;

            /**
             * Cached socket address.
             */
            osiSockAddr _socketAddress;

            /**
             * Priority.
             * NOTE: Priority cannot just be changed, since it is registered
             * in transport registry with given priority.
             */
            epics::pvData::int16 _priority;
            // TODO to be implemeneted

            /**
             * PVAS response handler.
             */
            std::auto_ptr<ResponseHandler> _responseHandler;

            // TODO review int vs std::size_t

            /**
             * Send buffer size.
             */
            std::size_t _maxPayloadSize;

            /**
             * Send buffer size.
             */
            int _socketSendBufferSize;

            /**
             * Marker "period" in bytes (every X bytes marker should be set).
             */
            epics::pvData::int64 _markerPeriodBytes;


            FlushStrategy _flushStrategy;


            epicsThreadId _rcvThreadId;

            epicsThreadId _sendThreadId;

            // TODO
            //MonitorSender* _monitorSender;

            Context::shared_pointer _context;

            bool _autoDelete;



            /**** after verification ****/
            
            /**
             * Remote side transport revision (minor).
             */
            epics::pvData::int8 _remoteTransportRevision;

            /**
             * Remote side transport receive buffer size.
             */
            int _remoteTransportReceiveBufferSize;

            /**
             * Remote side transport socket receive buffer size.
             */
            int _remoteTransportSocketReceiveBufferSize;



            /*** send thread only - no need to sync ***/
            // NOTE: now all send-related external calls are TransportSender IF
            // and its reference is only valid when called from send thread
            
            // initialized at construction time
            std::deque<TransportSender::shared_pointer> _sendQueue;
            epics::pvData::Mutex _sendQueueMutex;

            // initialized at construction time
//            std::deque<TransportSender::shared_pointer> _monitorSendQueue;
//            epics::pvData::Mutex _monitorMutex;

            /**
             * Send buffer.
             */
            epics::pvData::ByteBuffer* _sendBuffer;

#if FLOW_CONTROL
            /**
             * Next planned marker position.
             */
            epics::pvData::int64 _nextMarkerPosition;
#endif

            /**
             * Send pending flag.
             */
            bool _sendPending;

            /**
             * Last message start position.
             */
            int _lastMessageStartPosition;

            epics::pvData::int8 _lastSegmentedMessageType;
            epics::pvData::int8 _lastSegmentedMessageCommand;

            bool _flushRequested;

            int _sendBufferSentPosition;

            epics::pvData::int8 _byteOrderFlag;

        	/**
        	 * Outgoing (codes generated by this party) introspection registry.
        	 */
        	IntrospectionRegistry outgoingIR;





            
            /*** receive thread only - no need to sync ***/

            // initialized at construction time
            epics::pvData::ByteBuffer* _socketBuffer;

            std::size_t _startPosition;

            std::size_t _storedPayloadSize;
            std::size_t _storedPosition;
            std::size_t _storedLimit;

            epics::pvData::int8 _version;
            epics::pvData::int8 _packetType;
            epics::pvData::int8 _command;
            std::size_t _payloadSize;

            ReceiveStage _stage;

            std::size_t _directPayloadRead;
            char * _directBuffer;

#if FLOW_CONTROL

            /**
             * Total bytes received.
             */
            epics::pvData::int64 _totalBytesReceived;
#endif

        	/**
        	 * Incoming (codes generated by other party) introspection registry.
        	 */
        	IntrospectionRegistry incomingIR;



            /*** send/receive thread shared ***/

            /**
             * Connection status
             * NOTE: synced by _mutex
             */
            AtomicBoolean _closed;

            // NOTE: synced by _mutex
            bool _sendThreadExited;

            epics::pvData::Mutex _mutex;


            bool _verified;
            epics::pvData::Mutex _verifiedMutex;



            
            epics::pvData::Event _sendQueueEvent;

            epics::pvData::Event _verifiedEvent;





#if FLOW_CONTROL
            /**
             * Marker to send.
             * NOTE: synced by _flowControlMutex
             */
            int _markerToSend;

            /**
             * Total bytes sent.
             * NOTE: synced by _flowControlMutex
             */
            epics::pvData::int64 _totalBytesSent;

            /**
             * Calculated remote free buffer size.
             * NOTE: synced by _flowControlMutex
             */
            epics::pvData::int64 _remoteBufferFreeSpace;
            
            epics::pvData::Mutex _flowControlMutex;
#endif

        private:
        
            /**
             * Internal method that clears and releases buffer.
             * sendLock and sendBufferLock must be hold while calling this method.
             */
            void clearAndReleaseBuffer();

            void endMessage(bool hasMoreSegments);

            bool flush();

            void processSendQueue();

            static void rcvThreadRunner(void* param);

            static void sendThreadRunner(void* param);

            /**
             * Free all send buffers (return them to the cached buffer allocator).
             */
            void freeSendBuffers();
        };

        
        class BlockingClientTCPTransport : public BlockingTCPTransport,
                public TransportSender,
                public epics::pvData::TimerCallback {

        public:
        	POINTER_DEFINITIONS(BlockingClientTCPTransport);

        private:
            BlockingClientTCPTransport(Context::shared_pointer const & context, SOCKET channel,
                    std::auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize,
                    TransportClient::shared_pointer client, epics::pvData::int8 remoteTransportRevision,
                    float beaconInterval, epics::pvData::int16 priority);

        public:
            static shared_pointer create(Context::shared_pointer const & context, SOCKET channel,
                                       std::auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize,
                                       TransportClient::shared_pointer client, epics::pvData::int8 remoteTransportRevision,
                                       float beaconInterval, epics::pvData::int16 priority)
            {
                shared_pointer thisPointer(
                            new BlockingClientTCPTransport(context, channel, responseHandler, receiveBufferSize,
                                                           client, remoteTransportRevision, beaconInterval, priority)
                );
                thisPointer->start();
                return thisPointer;
            }
            
            virtual void start();

            virtual ~BlockingClientTCPTransport();
                    
            virtual void timerStopped() {
                // noop
            }

            virtual void callback();

            /**
             * Acquires transport.
             * @param client client (channel) acquiring the transport
             * @return <code>true</code> if transport was granted, <code>false</code> otherwise.
             */
            virtual bool acquire(TransportClient::shared_pointer const & client);

            /**
             * Releases transport.
             * @param client client (channel) releasing the transport
             */
            virtual void release(pvAccessID clientId);
            //virtual void release(TransportClient::shared_pointer const & client);

            /**
             * Alive notification.
             * This method needs to be called (by newly received data or beacon)
             * at least once in this period, if not echo will be issued
             * and if there is not response to it, transport will be considered as unresponsive.
             */
            virtual void aliveNotification();

            /**
             * Changed transport (server restared) notify.
             */
            virtual void changedTransport();

            virtual void lock() {
                // noop
            }

            virtual void unlock() {
                // noop
            }

            virtual void acquire() {
                // noop, since does not make sence on itself
            }

            virtual void release() {
                // noop, since does not make sence on itself
            }

            virtual void send(epics::pvData::ByteBuffer* buffer,
                    TransportSendControl* control);

        protected:

            virtual void internalClose(bool force);
            virtual void internalPostClose(bool force);

        private:

            /**
             * Owners (users) of the transport.
             */
            // TODO consider using TR1 hash map
            typedef std::map<pvAccessID, TransportClient::weak_pointer> TransportClientMap_t;
            TransportClientMap_t _owners;

            /**
             * Connection timeout (no-traffic) flag.
             */
            double _connectionTimeout;

            /**
             * Unresponsive transport flag.
             */
            bool _unresponsiveTransport;

            /**
             * Timestamp of last "live" event on this transport.
             */
            epicsTimeStamp _aliveTimestamp;

            bool _verifyOrEcho;

            /**
             * Unresponsive transport notify.
             */
            void unresponsiveTransport();

            /**
             * Notifies clients about disconnect.
             */
            void closedNotifyClients();

            /**
             * Responsive transport notify.
             */
            void responsiveTransport();
        };

        /**
         * Channel Access TCP connector.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: BlockingTCPConnector.java,v 1.1 2010/05/03 14:45:47 mrkraimer Exp $
         */
        class BlockingTCPConnector : public Connector {
        public:
        	POINTER_DEFINITIONS(BlockingTCPConnector);

            BlockingTCPConnector(Context::shared_pointer const & context, int receiveBufferSize,
                    float beaconInterval);

            virtual ~BlockingTCPConnector();

            virtual Transport::shared_pointer connect(TransportClient::shared_pointer const & client,
                    std::auto_ptr<ResponseHandler>& responseHandler, osiSockAddr& address,
                    epics::pvData::int8 transportRevision, epics::pvData::int16 priority);
        private:
            /**
             * Lock timeout
             */
            static const int LOCK_TIMEOUT = 20*1000; // 20s

            /**
             * Context instance.
             */
            Context::weak_pointer _context;

            /**
             * named lock
             */
            NamedLockPattern<const osiSockAddr*, comp_osiSockAddrPtr> _namedLocker;

            /**
             * Receive buffer size.
             */
            int _receiveBufferSize;

            /**
             * Beacon interval.
             */
            float _beaconInterval;

            /**
             * Tries to connect to the given address.
             * @param[in] address
             * @param[in] tries
             * @return the SOCKET
             * @throws IOException
             */
            SOCKET tryConnect(osiSockAddr& address, int tries);

        };

        class BlockingServerTCPTransport : public BlockingTCPTransport,
                public ChannelHostingTransport,
                public TransportSender {
        public:
        	POINTER_DEFINITIONS(BlockingServerTCPTransport);

        private:
            BlockingServerTCPTransport(Context::shared_pointer const & context, SOCKET channel,
                    std::auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize);
        public:
            static shared_pointer create(Context::shared_pointer const & context, SOCKET channel,
                                       std::auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize)
            {
                shared_pointer thisPointer(
                        new BlockingServerTCPTransport(context, channel, responseHandler, receiveBufferSize)
                );
                thisPointer->start();
                return thisPointer;
            }
                    
            virtual bool acquire(std::tr1::shared_ptr<TransportClient> const & /*client*/)
            {
            	return false;
            }

            virtual void release(pvAccessID /*clientId*/) {}

            /**
             * Preallocate new channel SID.
             * @return new channel server id (SID).
             */
            virtual pvAccessID preallocateChannelSID();

            /**
             * De-preallocate new channel SID.
             * @param sid preallocated channel SID.
             */
            virtual void depreallocateChannelSID(pvAccessID /*sid*/) {
                // noop
            }

            /**
             * Register a new channel.
             * @param sid preallocated channel SID.
             * @param channel channel to register.
             */
            virtual void registerChannel(pvAccessID sid, ServerChannel::shared_pointer const & channel);

            /**
             * Unregister a new channel (and deallocates its handle).
             * @param sid SID
             */
            virtual void unregisterChannel(pvAccessID sid);

            /**
             * Get channel by its SID.
             * @param sid channel SID
             * @return channel with given SID, <code>NULL</code> otherwise
             */
            virtual ServerChannel::shared_pointer getChannel(pvAccessID sid);

            /**
             * Get channel count.
             * @return channel count.
             */
            virtual int getChannelCount();

            virtual epics::pvData::PVField::shared_pointer getSecurityToken() {
                return epics::pvData::PVField::shared_pointer();
            }

            virtual void lock() {
                // noop
            }

            virtual void unlock() {
                // noop
            }

            virtual void acquire() {
                // noop, since does not make sence on itself
            }

            virtual void release() {
                // noop, since does not make sence on itself
            }

            /**
             * Verify transport. Server side is self-verified.
             */
            virtual bool verify(epics::pvData::int32 /*timeoutMs*/) {
            	TransportSender::shared_pointer transportSender = std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
            	enqueueSendRequest(transportSender);
            	verified();
            	return true;
            }

            /**
             * PVA connection validation request.
             * A server sends a validate connection message when it receives a new connection.
             * The message indicates that the server is ready to receive requests; the client must
             * not send any messages on the connection until it has received the validate connection message
             * from the server. No reply to the message is expected by the server.
             * The purpose of the validate connection message is two-fold:
             * It informs the client of the protocol version supported by the server.
             * It prevents the client from writing a request message to its local transport
             * buffers until after the server has acknowledged that it can actually process the
             * request. This avoids a race condition caused by the server's TCP/IP stack
             * accepting connections in its backlog while the server is in the process of shutting down:
             * if the client were to send a request in this situation, the request
             * would be lost but the client could not safely re-issue the request because that
             * might violate at-most-once semantics.
             * The validate connection message guarantees that a server is not in the middle
             * of shutting down when the server's TCP/IP stack accepts an incoming connection
             * and so avoids the race condition.
             * @see org.epics.ca.impl.remote.TransportSender#send(java.nio.ByteBuffer, org.epics.ca.impl.remote.TransportSendControl)
             */
            virtual void send(epics::pvData::ByteBuffer* buffer,
                    TransportSendControl* control);

            virtual ~BlockingServerTCPTransport();

        protected:

            virtual void internalClose(bool force);
            virtual void internalPostClose(bool force);

        private:
            /**
             * Last SID cache.
             */
            pvAccessID _lastChannelSID;

            /**
             * Channel table (SID -> channel mapping).
             */
            std::map<pvAccessID, ServerChannel::shared_pointer> _channels;

            epics::pvData::Mutex _channelsMutex;

            /**
             * Destroy all channels.
             */
            void destroyAllChannels();
        };
        
        class ResponseHandlerFactory
        {
            public:
        	POINTER_DEFINITIONS(ResponseHandlerFactory);
            
            virtual ~ResponseHandlerFactory() {};

            virtual std::auto_ptr<ResponseHandler> createResponseHandler() = 0;
        };
        
        /**
         * Channel Access Server TCP acceptor.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: BlockingTCPAcceptor.java,v 1.1 2010/05/03 14:45:42 mrkraimer Exp $
         */
        class BlockingTCPAcceptor {
        public:
        	POINTER_DEFINITIONS(BlockingTCPAcceptor);

            /**
             * @param context
             * @param port
             * @param receiveBufferSize
             * @throws PVAException
             */
            BlockingTCPAcceptor(Context::shared_pointer const & context,
                                ResponseHandlerFactory::shared_pointer const & responseHandlerFactory,
                                int port, int receiveBufferSize);

            virtual ~BlockingTCPAcceptor();

            void handleEvents();

            /**
             * Bind socket address.
             * @return bind socket address, <code>null</code> if not binded.
             */
            const osiSockAddr* getBindAddress() {
                return &_bindAddress;
            }

            /**
             * Destroy acceptor (stop listening).
             */
            void destroy();

        private:
            /**
             * Context instance.
             */
            Context::shared_pointer _context;
            
            /**
             * ResponseHandler factory.
             */
            ResponseHandlerFactory::shared_pointer _responseHandlerFactory;

            /**
             * Bind server socket address.
             */
            osiSockAddr _bindAddress;

            /**
             * Server socket channel.
             */
            SOCKET _serverSocketChannel;

            /**
             * Receive buffer size.
             */
            int _receiveBufferSize;

            /**
             * Destroyed flag.
             */
            bool _destroyed;
            
            epics::pvData::Mutex _mutex;

            epicsThreadId _threadId;

            /**
             * Initialize connection acception.
             * @return port where server is listening
             */
            int initialize(unsigned short port);

            /**
             * Validate connection by sending a validation message request.
             * @return <code>true</code> on success.
             */
            bool validateConnection(Transport::shared_pointer const & transport, const char* address);

            static void handleEventsRunner(void* param);
        };

    }
}

#endif /* BLOCKINGTCP_H_ */
