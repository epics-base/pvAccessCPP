/*
 * blockingUDPTransport.h
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

#ifndef BLOCKINGUDP_H_
#define BLOCKINGUDP_H_

/* pvAccess */
#include "remote.h"
#include "caConstants.h"
#include "inetAddressUtil.h"

/* pvData */
#include <noDefaultMethods.h>
#include <byteBuffer.h>
#include <lock.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <epicsThread.h>

namespace epics {
    namespace pvAccess {

        class BlockingUDPTransport : public epics::pvData::NoDefaultMethods,
                public Transport,
                public TransportSendControl {
        public:
            BlockingUDPTransport(ResponseHandler* responseHandler,
                    SOCKET channel, osiSockAddr* bindAddress,
                    InetAddrVector* sendAddresses,
                    short remoteTransportRevision);

            virtual ~BlockingUDPTransport();

            virtual bool isClosed() const {
                return _closed;
            }

            virtual const osiSockAddr* getRemoteAddress() const {
                return _socketAddress;
            }

            virtual const String getType() const {
                return String("UDP");
            }

            virtual int getReceiveBufferSize() const {
                return _receiveBuffer->getSize();
            }

            virtual int getSocketReceiveBufferSize() const;

            virtual int16 getPriority() const {
                return CA_DEFAULT_PRIORITY;
            }

            virtual void setRemoteMinorRevision(int8 minor) {
                // noop
            }

            virtual void setRemoteTransportReceiveBufferSize(
                    int receiveBufferSize) {
                // noop for UDP (limited by 64k; MAX_UDP_SEND for CA)
            }

            virtual void setRemoteTransportSocketReceiveBufferSize(
                    int socketReceiveBufferSize) {
                // noop for UDP (limited by 64k; MAX_UDP_SEND for CA)
            }

            virtual void aliveNotification() {
                // noop
            }

            virtual void changedTransport() {
                // noop
            }

            virtual bool isVerified() const {
                return false;
            }

            virtual void verified() {
                // noop
            }

            virtual void enqueueSendRequest(TransportSender* sender);

            void start();

            virtual void close(bool forced);

            virtual void ensureData(int size) {
                // TODO Auto-generated method stub
            }

            virtual void startMessage(int8 command, int ensureCapacity);
            virtual void endMessage();

            virtual void flush(bool lastMessageCompleted) {
                // noop since all UDP requests are sent immediately
            }

            virtual void setRecipient(const osiSockAddr* sendTo) {
                _sendTo = sendTo;
            }

            virtual void flushSerializeBuffer() {
                // TODO Auto-generated method stub
            }

            virtual void ensureBuffer(int size) {
                // noop
            }

            /**
             * Set ignore list.
             * @param addresses list of ignored addresses.
             */
            void setIgnoredAddresses(InetAddrVector* addresses) {
                _ignoredAddresses = addresses;
            }

            /**
             * Get list of ignored addresses.
             * @return ignored addresses.
             */
            InetAddrVector* getIgnoredAddresses() const {
                return _ignoredAddresses;
            }

            bool send(ByteBuffer* buffer, const osiSockAddr* address = NULL);

            /**
             * Get list of send addresses.
             * @return send addresses.
             */
            InetAddrVector* getSendAddresses() {
                return _sendAddresses;
            }

            /**
             * Get bind address.
             * @return bind address.
             */
            osiSockAddr* getBindAddress() {
                return _bindAddress;
            }

            /**
             * Set list of send addresses.
             * @param addresses list of send addresses, non-<code>null</code>.
             */
            void setBroadcastAddresses(InetAddrVector* addresses) {
                _sendAddresses = addresses;
            }

        protected:
            bool volatile _closed;

            /**
             * Response handler.
             */
            ResponseHandler* _responseHandler;

            virtual void processRead();
        private:
            static void threadRunner(void* param);

            bool processBuffer(osiSockAddr* fromAddress,
                    epics::pvData::ByteBuffer* receiveBuffer);

            // Context only used for logging in this class

            /**
             * Corresponding channel.
             */
            SOCKET _channel;

            /**
             * Cached socket address.
             */
            osiSockAddr* _socketAddress;

            /**
             * Bind address.
             */
            osiSockAddr* _bindAddress;

            /**
             * Send addresses.
             */
            InetAddrVector* _sendAddresses;

            /**
             * Ignore addresses.
             */
            InetAddrVector* _ignoredAddresses;

            const osiSockAddr* _sendTo;

            /**
             * Receive buffer.
             */
            epics::pvData::ByteBuffer* _receiveBuffer;

            /**
             * Send buffer.
             */
            epics::pvData::ByteBuffer* _sendBuffer;

            /**
             * Last message start position.
             */
            int _lastMessageStartPosition;

            /**
             * Read buffer
             */
            char* _readBuffer;

            /**
             * Used for process sync.
             */
            Mutex* _mutex;

            /**
             * Thread ID
             */
            epicsThreadId _threadId;

        };

        class BlockingUDPConnector : public Connector,
                epics::pvData::NoDefaultMethods {
        public:

            BlockingUDPConnector(bool reuseSocket,
                    InetAddrVector* sendAddresses, bool broadcast) :
                _sendAddresses(sendAddresses), _reuseSocket(reuseSocket),
                        _broadcast(broadcast) {
            }

            virtual ~BlockingUDPConnector() {
                // TODO: delete _sendAddresses here?
            }

            /**
             * NOTE: transport client is ignored for broadcast (UDP).
             */
            virtual Transport* connect(TransportClient* client,
                    ResponseHandler* responseHandler, osiSockAddr* bindAddress,
                    short transportRevision, short priority);

        private:

            /**
             * Send address.
             */
            InetAddrVector* _sendAddresses;

            /**
             * Reuse socket flag.
             */
            bool _reuseSocket;

            /**
             * Broadcast flag.
             */
            bool _broadcast;

        };

    }
}

#endif /* BLOCKINGUDP_H_ */
