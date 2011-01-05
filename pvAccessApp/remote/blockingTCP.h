/*
 * blockingTCP.h
 *
 *  Created on: Dec 29, 2010
 *      Author: Miha Vitorovic
 */

#ifndef BLOCKINGTCP_H_
#define BLOCKINGTCP_H_

/* pvAccess */
#include "caConstants.h"
#include "remote.h"
#include "growingCircularBuffer.h"
#include "transportRegistry.h"
#include "introspectionRegistry.h"
#include "namedLockPattern.h"
#include "inetAddressUtil.h"

/* pvData */
#include <byteBuffer.h>
#include <pvType.h>
#include <lock.h>
#include <timer.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <epicsTime.h>
#include <epicsThread.h>

/* standard */
#include <set>
#include <map>

namespace epics {
    namespace pvAccess {

        class MonitorSender;

        enum ReceiveStage {
            READ_FROM_SOCKET, PROCESS_HEADER, PROCESS_PAYLOAD, NONE
        };

        enum SendQueueFlushStrategy {
            IMMEDIATE, DELAYED, USER_CONTROLED
        };

        class BlockingTCPTransport : public Transport,
                public TransportSendControl {
        public:
            BlockingTCPTransport(Context* context, SOCKET channel,
                    ResponseHandler* responseHandler, int receiveBufferSize,
                    int16 priority);

            virtual ~BlockingTCPTransport();

            virtual bool isClosed() const {
                return _closed;
            }

            virtual void setRemoteMinorRevision(int8 minorRevision) {
                _remoteTransportRevision = minorRevision;
            }

            virtual void setRemoteTransportReceiveBufferSize(
                    int remoteTransportReceiveBufferSize) {
                _remoteTransportReceiveBufferSize
                        = remoteTransportReceiveBufferSize;
            }

            virtual void setRemoteTransportSocketReceiveBufferSize(
                    int socketReceiveBufferSize) {
                _remoteTransportSocketReceiveBufferSize
                        = socketReceiveBufferSize;
            }

            virtual const String getType() const {
                return String("TCP");
            }

            virtual void aliveNotification() {
                // noop
            }

            virtual void changedTransport() {
                // noop
            }

            virtual const osiSockAddr* getRemoteAddress() const {
                return _socketAddress;
            }

            virtual int16 getPriority() const {
                return _priority;
            }

            virtual int getReceiveBufferSize() const {
                return _socketBuffer->getSize();
            }

            /**
             * Get remote transport receive buffer size (in bytes).
             * @return remote transport receive buffer size
             */
            int getRemoteTransportReceiveBufferSize() {
                return _remoteTransportReceiveBufferSize;
            }

            virtual int getSocketReceiveBufferSize() const;

            virtual bool isVerified() const {
                Lock lock(_verifiedMutex);
                return _verified;
            }

            virtual void verified() {
                Lock lock(_verifiedMutex);
                _verified = true;
            }

            virtual void setRecipient(const osiSockAddr* sendTo) {
                // noop
            }

            /**
             * @param[in] timeout Timeout in seconds
             */
            bool waitUntilVerified(double timeout);

            virtual void flush(bool lastMessageCompleted);
            virtual void startMessage(int8 command, int ensureCapacity);
            virtual void endMessage();

            virtual void flushSerializeBuffer() {
                flush(false);
            }

            virtual void ensureBuffer(int size);

            virtual void ensureData(int size);

            virtual void close(bool force);

            SendQueueFlushStrategy getSendQueueFlushStrategy() {
                return _flushStrategy;
            }

            void setSendQueueFlushStrategy(SendQueueFlushStrategy flushStrategy) {
                _flushStrategy = flushStrategy;
            }

            void requestFlush();

            /**
             * Close and free connection resources.
             */
            void freeConnectionResorces();

            /**
             * Starts the receive and send threads
             */
            void start();

            virtual void enqueueSendRequest(TransportSender* sender);

            void enqueueMonitorSendRequest(TransportSender* sender);

        protected:
            /**
             * Connection status
             */
            bool volatile _closed;

            /**
             * Corresponding channel.
             */
            SOCKET _channel;

            /**
             * Cached socket address.
             */
            osiSockAddr* _socketAddress;

            /**
             * Send buffer.
             */
            epics::pvData::ByteBuffer* _sendBuffer;

            /**
             * Remote side transport revision (minor).
             */
            int8 _remoteTransportRevision;

            /**
             * Remote side transport receive buffer size.
             */
            int _remoteTransportReceiveBufferSize;

            /**
             * Remote side transport socket receive buffer size.
             */
            int _remoteTransportSocketReceiveBufferSize;

            /**
             * Priority.
             * NOTE: Priority cannot just be changed, since it is registered
             * in transport registry with given priority.
             */
            int16 _priority;
            // TODO to be implemeneted

            /**
             * CAS response handler.
             */
            ResponseHandler* _responseHandler;

            /**
             * Read sync. object monitor.
             */
            //Object _readMonitor = new Object();

            /**
             * Total bytes received.
             */
            int64 volatile _totalBytesReceived;

            /**
             * Total bytes sent.
             */
            int64 volatile _totalBytesSent;

            /**
             * Marker to send.
             */
            volatile int _markerToSend;

            volatile bool _verified;

            volatile int64 _remoteBufferFreeSpace;

            virtual void processReadCached(bool nestedCall,
                    ReceiveStage inStage, int requiredBytes, bool addToBuffer);

            /**
             * Called to any resources just before closing transport
             * @param[in] force   flag indicating if forced (e.g. forced
             * disconnect) is required
             */
            virtual void internalClose(bool force);

            /**
             * Send a buffer through the transport.
             * NOTE: TCP sent buffer/sending has to be synchronized (not done by this method).
             * @param buffer[in]    buffer to be sent
             * @return success indicator
             */
            virtual bool send(epics::pvData::ByteBuffer* buffer);

        private:
            /**
             * Default marker period.
             */
            static const int MARKER_PERIOD = 1024;

            static const int MAX_ENSURE_DATA_BUFFER_SIZE = 1024;

            static const double delay = 0.01;

            /**
             * Send buffer size.
             */
            int _maxPayloadSize;

            /**
             * Send buffer size.
             */
            int _socketSendBufferSize;

            /**
             * Marker "period" in bytes (every X bytes marker should be set).
             */
            int64 _markerPeriodBytes;

            /**
             * Next planned marker position.
             */
            int64 _nextMarkerPosition;

            /**
             * Send pending flag.
             */
            bool _sendPending;

            /**
             * Last message start position.
             */
            int _lastMessageStartPosition;

            epics::pvData::ByteBuffer* _socketBuffer;

            int _startPosition;

            epics::pvData::Mutex* _mutex;
            epics::pvData::Mutex* _sendQueueMutex;
            epics::pvData::Mutex* _verifiedMutex;
            epics::pvData::Mutex* _monitorMutex;

            ReceiveStage _stage;

            int8 _lastSegmentedMessageType;
            int8 _lastSegmentedMessageCommand;

            int _storedPayloadSize;
            int _storedPosition;
            int _storedLimit;

            short _magicAndVersion;
            int8 _packetType;
            int8 _command;
            int _payloadSize;

            volatile bool _flushRequested;

            int _sendBufferSentPosition;

            SendQueueFlushStrategy _flushStrategy;

            GrowingCircularBuffer<TransportSender*>* _sendQueue;

            epicsThreadId _rcvThreadId;

            epicsThreadId _sendThreadId;

            GrowingCircularBuffer<TransportSender*>* _monitorSendQueue;

            MonitorSender* _monitorSender;

            Context* _context;

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

            TransportSender* extractFromSendQueue();
        };

        class BlockingClientTCPTransport : public BlockingTCPTransport,
                public TransportSender,
                public epics::pvData::TimerCallback,
                public ReferenceCountingTransport {

        public:
            BlockingClientTCPTransport(Context* context, SOCKET channel,
                    ResponseHandler* responseHandler, int receiveBufferSize,
                    TransportClient* client, short remoteTransportRevision,
                    float beaconInterval, int16 priority);

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
            virtual bool acquire(TransportClient* client);

            virtual IntrospectionRegistry* getIntrospectionRegistry() {
                return _introspectionRegistry;
            }

            /**
             * Releases transport.
             * @param client client (channel) releasing the transport
             */
            virtual void release(TransportClient* client);

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

            virtual void send(epics::pvData::ByteBuffer* buffer,
                    TransportSendControl* control);

        protected:
            /**
             * Introspection registry.
             */
            IntrospectionRegistry* _introspectionRegistry;

            virtual void internalClose(bool force);

        private:

            /**
             * Owners (users) of the transport.
             */
            std::set<TransportClient*>* _owners;

            /**
             * Connection timeout (no-traffic) flag.
             */
            double _connectionTimeout;

            /**
             * Unresponsive transport flag.
             */
            volatile bool _unresponsiveTransport;

            /**
             * Timer task node.
             */
            TimerNode* _timerNode;

            /**
             * Timestamp of last "live" event on this transport.
             */
            volatile epicsTimeStamp _aliveTimestamp;

            epics::pvData::Mutex* _mutex;
            epics::pvData::Mutex* _ownersMutex;

            bool _verifyOrEcho;

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
            BlockingTCPConnector(Context* context, int receiveBufferSize,
                    float beaconInterval);

            virtual ~BlockingTCPConnector();

            virtual Transport* connect(TransportClient* client,
                    ResponseHandler* responseHandler, osiSockAddr* address,
                    short transportRevision, int16 priority);
        private:
            /**
             * Lock timeout
             */
            static const int LOCK_TIMEOUT = 20*1000; // 20s

            /**
             * Context instance.
             */
            Context* _context;

            /**
             * named lock
             */
            NamedLockPattern<const osiSockAddr*, comp_osiSockAddrPtr>* _namedLocker;

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
            SOCKET tryConnect(osiSockAddr* address, int tries);

        };

        class BlockingServerTCPTransport : public BlockingTCPTransport,
                public ChannelHostingTransport,
                public TransportSender {
        public:
            BlockingServerTCPTransport(Context* context, SOCKET channel,
                    ResponseHandler* responseHandler, int receiveBufferSize);

            virtual ~BlockingServerTCPTransport();

            virtual IntrospectionRegistry* getIntrospectionRegistry() {
                return _introspectionRegistry;
            }

            /**
             * Preallocate new channel SID.
             * @return new channel server id (SID).
             */
            virtual int preallocateChannelSID();

            /**
             * De-preallocate new channel SID.
             * @param sid preallocated channel SID.
             */
            virtual void depreallocateChannelSID(int sid) {
                // noop
            }

            /**
             * Register a new channel.
             * @param sid preallocated channel SID.
             * @param channel channel to register.
             */
            virtual void registerChannel(int sid, ServerChannel* channel);

            /**
             * Unregister a new channel (and deallocates its handle).
             * @param sid SID
             */
            virtual void unregisterChannel(int sid);

            /**
             * Get channel by its SID.
             * @param sid channel SID
             * @return channel with given SID, <code>NULL</code> otherwise
             */
            virtual ServerChannel* getChannel(int sid);

            /**
             * Get channel count.
             * @return channel count.
             */
            virtual int getChannelCount();

            virtual epics::pvData::PVField* getSecurityToken() {
                return NULL;
            }

            virtual void lock() {
                // noop
            }

            virtual void unlock() {
                // noop
            }

            /**
             * Verify transport. Server side is self-verified.
             */
            void verify() {
                enqueueSendRequest(this);
                verified();
            }

            /**
             * CA connection validation request.
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

        protected:
            /**
             * Introspection registry.
             */
            IntrospectionRegistry* _introspectionRegistry;

            virtual void internalClose(bool force);

        private:
            /**
             * Last SID cache.
             */
            volatile int _lastChannelSID;

            /**
             * Channel table (SID -> channel mapping).
             */
            std::map<int, ServerChannel*>* _channels;

            Mutex* _channelsMutex;

            /**
             * Destroy all channels.
             */
            void destroyAllChannels();
        };

        /**
         * Channel Access Server TCP acceptor.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: BlockingTCPAcceptor.java,v 1.1 2010/05/03 14:45:42 mrkraimer Exp $
         */
        class BlockingTCPAcceptor {
        public:

            /**
             * @param context
             * @param port
             * @param receiveBufferSize
             * @throws CAException
             */
            BlockingTCPAcceptor(Context* context, int port,
                    int receiveBufferSize);

            ~BlockingTCPAcceptor();

            void handleEvents();

            /**
             * Bind socket address.
             * @return bind socket address, <code>null</code> if not binded.
             */
            osiSockAddr* getBindAddress() {
                return _bindAddress;
            }

            /**
             * Destroy acceptor (stop listening).
             */
            void destroy();

        private:
            /**
             * Context instance.
             */
            Context* _context;

            /**
             * Bind server socket address.
             */
            osiSockAddr* _bindAddress;

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
            volatile bool _destroyed;

            epicsThreadId _threadId;

            /**
             * Initialize connection acception.
             * @return port where server is listening
             */
            int initialize(in_port_t port);

            /**
             * Validate connection by sending a validation message request.
             * @return <code>true</code> on success.
             */
            bool validateConnection(BlockingServerTCPTransport* transport,
                    const char* address);

            static void handleEventsRunner(void* param);
        };

    }
}

#endif /* BLOCKINGTCP_H_ */
