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
#include <event.h>

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
                    SOCKET channel, osiSockAddr& bindAddress,
                    short remoteTransportRevision);

            virtual ~BlockingUDPTransport();

            virtual bool isClosed() {
                Lock guard(&_mutex);
                return _closed;
            }

            virtual const osiSockAddr* getRemoteAddress() const {
                return &_bindAddress;
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

            virtual bool isVerified() {
                return false;
            }

            virtual void verified() {
                // noop
            }

            virtual void enqueueSendRequest(TransportSender* sender);

            void start();

            virtual void close(bool forced);

            virtual void ensureData(int size) {
                // noop
            }

            virtual void startMessage(int8 command, int ensureCapacity);
            virtual void endMessage();

            virtual void flush(bool lastMessageCompleted) {
                // noop since all UDP requests are sent immediately
            }

            virtual void setRecipient(const osiSockAddr& sendTo) {
                _sendToEnabled = true;
                _sendTo = sendTo;
            }

            virtual void flushSerializeBuffer() {
                // noop
            }

            virtual void ensureBuffer(int size) {
                // noop
            }

            /**
             * Set ignore list.
             * @param addresses list of ignored addresses.
             */
            void setIgnoredAddresses(InetAddrVector* addresses) {
                if (addresses)
                {
                    if (!_ignoredAddresses) _ignoredAddresses = new InetAddrVector;
                    *_ignoredAddresses = *addresses;
                }
                else
                {
                    if (_ignoredAddresses) { delete _ignoredAddresses; _ignoredAddresses = 0; }
                }
            }

            /**
             * Get list of ignored addresses.
             * @return ignored addresses.
             */
            InetAddrVector* getIgnoredAddresses() const {
                return _ignoredAddresses;
            }

            bool send(ByteBuffer* buffer, const osiSockAddr& address);

            bool send(ByteBuffer* buffer);

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
            const osiSockAddr* getBindAddress() const {
                return &_bindAddress;
            }

            /**
             * Set list of send addresses.
             * @param addresses list of send addresses, non-<code>null</code>.
             */
            void setBroadcastAddresses(InetAddrVector* addresses) {
                if (addresses)
                {
                    if (!_sendAddresses) _sendAddresses = new InetAddrVector;
                    *_sendAddresses = *addresses;
                }
                else
                {
                    if (_sendAddresses) { delete _sendAddresses; _sendAddresses = 0; }
                }
            }

            virtual IntrospectionRegistry* getIntrospectionRegistry() {
                return 0;
            }

        protected:
            bool _closed;

            /**
             * Response handler.
             */
            ResponseHandler* _responseHandler;

            virtual void processRead();
        private:
            static void threadRunner(void* param);

            bool processBuffer(osiSockAddr& fromAddress,
                    epics::pvData::ByteBuffer* receiveBuffer);

            void close(bool forced, bool waitForThreadToComplete);

            // Context only used for logging in this class

            /**
             * Corresponding channel.
             */
            SOCKET _channel;

            /**
             * Bind address.
             */
            osiSockAddr _bindAddress;

            /**
             * Send addresses.
             */
            InetAddrVector* _sendAddresses;

            /**
             * Ignore addresses.
             */
            InetAddrVector* _ignoredAddresses;

            /**
             * Send address.
             */
            osiSockAddr _sendTo;
            bool _sendToEnabled;
            
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
             * Used for process sync.
             */
            Mutex _mutex;
            Mutex _sendMutex;
            Event _shutdownEvent;

            /**
             * Thread ID
             */
            epicsThreadId _threadId;

        };

        class BlockingUDPConnector :
                public Connector,
                private epics::pvData::NoDefaultMethods {
        public:

            BlockingUDPConnector(
                    bool reuseSocket,
                    bool broadcast) :
                _reuseSocket(reuseSocket),
                _broadcast(broadcast) {
            }

            virtual ~BlockingUDPConnector() {
            }

            /**
             * NOTE: transport client is ignored for broadcast (UDP).
             */
            virtual Transport* connect(TransportClient* client,
                    ResponseHandler* responseHandler, osiSockAddr& bindAddress,
                    short transportRevision, int16 priority);

        private:

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
