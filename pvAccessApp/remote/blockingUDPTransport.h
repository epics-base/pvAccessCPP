/*
 * blockingUDPTransport.h
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

#ifndef BLOCKINGUDPTRANSPORT_H_
#define BLOCKINGUDPTRANSPORT_H_

#include "remote.h"
#include "caConstants.h"

#include <noDefaultMethods.h>
#include <byteBuffer.h>

#include <osdSock.h>
#include <osiSock.h>

namespace epics {
    namespace pvAccess {

        class BlockingUDPTransport : public epics::pvData::NoDefaultMethods,
                public Transport,
                public TransportSendControl {
        public:
            BlockingUDPTransport(SOCKET channel, osiSockAddr* bindAddress,
                    osiSockAddr* sendAddresses, short remoteTransportRevision);

            virtual ~BlockingUDPTransport();

            virtual bool isClosed() const {
                return closed;
            }

            virtual const osiSockAddr* getRemoteAddress() const {
                return socketAddress;
            }

            virtual const String getType() const {
                return String("UDP");
            }

            virtual int8 getMajorRevision() const {
                return CA_MAJOR_PROTOCOL_REVISION;
            }

            virtual int8 getMinorRevision() const {
                return CA_MINOR_PROTOCOL_REVISION;
            }

            virtual int getReceiveBufferSize() const {
                return receiveBuffer->getSize();
            }

            virtual int getSocketReceiveBufferSize() const {
                // Get value of the SO_RCVBUF option for this DatagramSocket,
                // that is the buffer size used by the platform for input on
                // this DatagramSocket.

                // TODO: real implementation
                return MAX_UDP_RECV;
            }

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
                // TODO: implement
            }

            virtual void startMessage(int8 command, int ensureCapacity);
            virtual void endMessage();

            virtual void flush(bool lastMessageCompleted) {
                // noop since all UDP requests are sent immediately
            }

            virtual void setRecipient(const osiSockAddr* sendTo) {
                this->sendTo = sendTo;
            }

            virtual void flushSerializeBuffer() {
                // TODO Auto-generated method stub
            }

            virtual void ensureBuffer(int size) {
                // noop
            }

        protected:
            bool closed;

            virtual void processRead();

        private:
            bool processBuffer(osiSockAddr* fromAddress,
                    ByteBuffer* receiveBuffer);

            // Context only used for logging in this class

            /**
             * Corresponding channel.
             */
            SOCKET channel;

            /**
             * Cached socket address.
             */
            osiSockAddr* socketAddress;

            /**
             * Bind address.
             */
            osiSockAddr* bindAddress;

            /**
             * Send addresses.
             */
            osiSockAddr* sendAddresses;

            /**
             * Ignore addresses.
             */
            osiSockAddr* ignoredAddresses;

            const osiSockAddr* sendTo;

            /**
             * Receive buffer.
             */
            epics::pvData::ByteBuffer* receiveBuffer;

            /**
             * Send buffer.
             */
            epics::pvData::ByteBuffer* sendBuffer;

            /**
             * Last message start position.
             */
            int lastMessageStartPosition;

        };

    }
}

#endif /* BLOCKINGUDPTRANSPORT_H_ */
