/*
 * blockingUDPTransport.h
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

#ifndef BLOCKINGUDPTRANSPORT_H_
#define BLOCKINGUDPTRANSPORT_H_

#include <noDefaultMethods.h>
#include <byteBuffer.h>

#include <osdSock.h>
#include <osiSock.h>

namespace epics {
    namespace pvAccess {

        class BlockingUDPTransport : public epics::pvData::NoDefaultMethods {
        public:
            BlockingUDPTransport(SOCKET channel, osiSockAddr* bindAddress,
                    osiSockAddr* sendAddresses, short remoteTransportRevision);

            ~BlockingUDPTransport();

            bool isClosed() {
                return closed;
            }

            void start();
            void close(bool forced);

        protected:
            bool closed;

        private:
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
