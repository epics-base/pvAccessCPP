/*
 * blockingUDPTransport.cpp
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingUDPTransport.h"

#include "caConstants.h"
#include "inetAddressUtil.h"

/* pvData */
#include <byteBuffer.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <errlog.h>

/* standard */
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
            sendTo = NULL;
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
            if(closed) return;
            closed = true;

            if(bindAddress!=NULL) errlogSevPrintf(errlogInfo,
                    "UDP connection to %s closed.", inetAddressToString(
                            bindAddress).c_str());

            // TODO: finish implementation

        }

        void BlockingUDPTransport::enqueueSendRequest(TransportSender* sender) {
            // TODO implement
        }

        void BlockingUDPTransport::startMessage(int8 command,
                int ensureCapacity) {
            lastMessageStartPosition = sendBuffer->getPosition();
            sendBuffer->putShort(CA_MAGIC_AND_VERSION);
            sendBuffer->putByte(0); // data
            sendBuffer->putByte(command); // command
            sendBuffer->putInt(0); // temporary zero payload
        }

        void BlockingUDPTransport::endMessage() {
            int32 data = lastMessageStartPosition+(16/8+2);
            sendBuffer->put((char*)&data, sendBuffer->getPosition()
                    -lastMessageStartPosition-CA_MESSAGE_HEADER_SIZE,
                    sizeof(int32));
        }

    }
}
