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

/* pvData */
#include <byteBuffer.h>
#include <pvType.h>
#include <lock.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>

using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        enum ReceiveStage {
            READ_FROM_SOCKET, PROCESS_HEADER, PROCESS_PAYLOAD, NONE
        };

        class BlockingTCPTransport : public Transport,
                public TransportSendControl {
        public:
            BlockingTCPTransport(SOCKET channel,
                    ResponseHandler* responseHandler, int receiveBufferSize,
                    short priority);

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

            void processReadCached(bool nestedCall, ReceiveStage inStage,
                    int requiredBytes, bool addToBuffer);

        private:
            /**
             * Default marker period.
             */
            static const int MARKER_PERIOD = 1024;

            static const int MAX_ENSURE_DATA_BUFFER_SIZE = 1024;

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

            /**
             * Internal method that clears and releases buffer.
             * sendLock and sendBufferLock must be hold while calling this method.
             */
            void clearAndReleaseBuffer();

            void endMessage(bool hasMoreSegments);

            bool flush();
        };

    }
}

#endif /* BLOCKINGTCP_H_ */
