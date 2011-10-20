/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#ifndef BLOCKINGUDP_H_
#define BLOCKINGUDP_H_

#include <pv/remote.h>
#include <pv/caConstants.h>
#include <pv/inetAddressUtil.h>

#include <pv/noDefaultMethods.h>
#include <pv/byteBuffer.h>
#include <pv/lock.h>
#include <pv/event.h>

#include <osdSock.h>
#include <osiSock.h>
#include <epicsThread.h>

namespace epics {
    namespace pvAccess {

        class BlockingUDPTransport : public epics::pvData::NoDefaultMethods,
                public Transport,
                public TransportSendControl,
                public std::tr1::enable_shared_from_this<BlockingUDPTransport>
        {
        public:
            typedef std::tr1::shared_ptr<BlockingUDPTransport> shared_pointer;
            typedef std::tr1::shared_ptr<const BlockingUDPTransport> const_shared_pointer;

        private:
            BlockingUDPTransport(std::auto_ptr<ResponseHandler>& responseHandler,
                                 SOCKET channel, osiSockAddr& bindAddress,
                                 short remoteTransportRevision);
        public:
            static BlockingUDPTransport::shared_pointer create(std::auto_ptr<ResponseHandler>& responseHandler,
                    SOCKET channel, osiSockAddr& bindAddress,
                    short remoteTransportRevision)
            {
                BlockingUDPTransport::shared_pointer thisPointer(
                            new BlockingUDPTransport(responseHandler, channel, bindAddress, remoteTransportRevision)
                );
                return thisPointer;
            }

            virtual ~BlockingUDPTransport();

            virtual bool isClosed() {
                return _closed.get();
            }

            virtual const osiSockAddr* getRemoteAddress() const {
                // always connected
                return &_bindAddress;
            }

            virtual const epics::pvData::String getType() const {
                return epics::pvData::String("UDP");
            }

            virtual int getReceiveBufferSize() const {
                return _receiveBuffer->getSize();
            }

            virtual int getSocketReceiveBufferSize() const;

            virtual epics::pvData::int16 getPriority() const {
                return CA_DEFAULT_PRIORITY;
            }

            virtual void setRemoteMinorRevision(epics::pvData::int8 minor) {
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

            virtual void enqueueSendRequest(TransportSender::shared_pointer const & sender);

            void start();

            virtual void close(bool forced);

            virtual void ensureData(int size) {
                // noop
            }

            virtual void alignData(int alignment) {
                _receiveBuffer->align(alignment);
            }

            virtual void startMessage(epics::pvData::int8 command, int ensureCapacity);
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

            virtual void alignBuffer(int alignment) {
                _sendBuffer->align(alignment);
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

            bool send(epics::pvData::ByteBuffer* buffer, const osiSockAddr& address);

            bool send(epics::pvData::ByteBuffer* buffer);

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
            AtomicBoolean _closed;

            /**
             * Response handler.
             */
            std::auto_ptr<ResponseHandler> _responseHandler;

            virtual void processRead();
            
        private:
            static void threadRunner(void* param);

            bool processBuffer(Transport::shared_pointer const & transport, osiSockAddr& fromAddress, epics::pvData::ByteBuffer* receiveBuffer);

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
            epics::pvData::Mutex _mutex;
            epics::pvData::Mutex _sendMutex;
            epics::pvData::Event _shutdownEvent;

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
            virtual Transport::shared_pointer connect(TransportClient::shared_pointer const & client,
                    std::auto_ptr<ResponseHandler>& responseHandler, osiSockAddr& bindAddress,
                    epics::pvData::int8 transportRevision, epics::pvData::int16 priority);

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
