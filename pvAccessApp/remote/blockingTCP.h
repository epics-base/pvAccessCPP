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

/* pvData */
#include <byteBuffer.h>
#include <pvType.h>
#include <lock.h>
#include <epicsThread.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>

using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        enum ReceiveStage {
            READ_FROM_SOCKET, PROCESS_HEADER, PROCESS_PAYLOAD, NONE
        };

        enum SendQueueFlushStrategy {
            IMMEDIATE, DELAYED, USER_CONTROLED
        };

        class BlockingTCPTransport : public Transport,
                public TransportSendControl {
        public:
            BlockingTCPTransport(SOCKET channel,
                    ResponseHandler* responseHandler, int receiveBufferSize,
                    short priority);

            ~BlockingTCPTransport();

            bool isClosed() const {
                return _closed;
            }

            void setRemoteMinorRevision(int minorRevision) {
                _remoteTransportRevision = minorRevision;
            }

            void setRemoteTransportReceiveBufferSize(
                    int remoteTransportReceiveBufferSize) {
                _remoteTransportReceiveBufferSize
                        = remoteTransportReceiveBufferSize;
            }

            void setRemoteTransportSocketReceiveBufferSize(
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
            ByteBuffer* _sendBuffer;

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
             * NOTE: Priority cannot just be changed, since it is registered in transport registry with given priority.
             */
            short _priority;
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
            int volatile _markerToSend;

            bool _verified;

            int64 volatile _remoteBufferFreeSpace;

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
            virtual bool send(ByteBuffer* buffer);

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

            ByteBuffer* _socketBuffer;

            int _startPosition;

            Mutex* _mutex;
            Mutex* _sendQueueMutex;
            Mutex* _verifiedMutex;
            Mutex* _monitorMutex;

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

            bool _flushRequested;

            int _sendBufferSentPosition;

            SendQueueFlushStrategy _flushStrategy;

            GrowingCircularBuffer<TransportSender*>* _sendQueue;

            epicsThreadId _rcvThreadId;

            epicsThreadId _sendThreadId;

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

    }
}

#endif /* BLOCKINGTCP_H_ */
