/*
 * blockingUDPTransport.cpp
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingUDPTransport.h"

#include "caConstants.h"

/* pvData */
#include <byteBuffer.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <errlog.h>

#include <cstdio>


namespace epics {
    namespace pvAccess {

        using namespace epics::pvData;

        BlockingUDPTransport::BlockingUDPTransport(SOCKET channel,
                osiSockAddr* bindAddress, osiSockAddr* sendAddresses,
                                  short remoteTransportRevision) {
            this->channel = channel;
            this->bindAddress = bindAddress;
            this->sendAddresses = sendAddresses;

            socketAddress = bindAddress;

            // allocate receive buffer
            receiveBuffer = new ByteBuffer(MAX_UDP_RECV);

            // allocate send buffer and non-reentrant lock
            sendBuffer = new ByteBuffer(MAX_UDP_SEND);

            ignoredAddresses = NULL;
            closed = false;
            lastMessageStartPosition = 0;
        }

        BlockingUDPTransport::~BlockingUDPTransport() {
            delete receiveBuffer;
            delete sendBuffer;
        }

        void BlockingUDPTransport::start() {
            // TODO implement
        }

        void BlockingUDPTransport::close(bool forced) {
            if (closed)
                return;
            closed = true;

            if (bindAddress != NULL)
                errlogSevPrintf( errlogInfo, "UDP connection to %d closed.", *bindAddress);

            //std::fclose(channel);
        }

    }
}
