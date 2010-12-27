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
#include <unistd.h>
#include <poll.h>
#include <errno.h>

namespace epics {
    namespace pvAccess {

        using namespace epics::pvData;

        BlockingUDPTransport::BlockingUDPTransport(SOCKET channel,
                osiSockAddr* bindAddress, InetAddrVector* sendAddresses,
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
            readBuffer = new char[MAX_UDP_RECV];
        }

        BlockingUDPTransport::~BlockingUDPTransport() {
            delete receiveBuffer;
            delete sendBuffer;
            delete readBuffer;
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

        void BlockingUDPTransport::processRead() {
            // This function is always called from only one thread - this
            // object's own thread.

            pollfd pfd;
            pfd.fd = channel;
            pfd.events = POLLIN;

            osiSockAddr fromAddress;

            try {

                while(!closed) {
                    // we poll to prevent blocking indefinitely

                    /* From 'accept' man page:
                     * In order to be notified of incoming connections on
                     * a socket, you can use select(2) or poll(2). A readable
                     * event will be delivered when a new connection is
                     * attempted and you may then call accept() to get a
                     * socket for that connection.
                     */

                    int retval = poll(&pfd, 1, 100);
                    if(retval>0) {
                        // activity on SOCKET
                        if(pfd.revents&POLLIN) {
                            // data ready to be read
                            receiveBuffer->clear();

                            socklen_t addrStructSize = sizeof(sockaddr);

                            int bytesRead = recvfrom(channel, readBuffer,
                                    MAX_UDP_RECV, 0, (sockaddr*)&fromAddress,
                                    &addrStructSize);

                            if(bytesRead>0) {
                                // successfully got datagram
                                bool ignore = false;
                                if(ignoredAddresses!=NULL) for(size_t i = 0; i
                                        <ignoredAddresses->size(); i++)
                                    if(ignoredAddresses->at(i)->ia.sin_addr.s_addr
                                            ==fromAddress.ia.sin_addr.s_addr) {
                                        ignore = true;
                                        break;
                                    }

                                if(!ignore) {
                                    receiveBuffer->put(
                                            readBuffer,
                                            0,
                                            bytesRead
                                                    <receiveBuffer->getRemaining() ? bytesRead
                                                    : receiveBuffer->getRemaining());

                                    receiveBuffer->flip();

                                    processBuffer(&fromAddress, receiveBuffer);
                                }
                            }
                            else {
                                // log a 'recvfrom' error
                                if(bytesRead==-1) errlogSevPrintf(errlogMajor,
                                        "Socket recv error: %s", strerror(errno));
                            }
                        }
                        else {
                            // error (POLLERR, POLLHUP, or POLLNVAL)
                            if(pfd.revents&POLLERR) errlogSevPrintf(
                                    errlogMajor, "Socket poll error (POLLERR)");
                            if(pfd.revents&POLLHUP) errlogSevPrintf(
                                    errlogMinor, "Socket poll error (POLLHUP)");
                            if(pfd.revents&POLLNVAL) errlogSevPrintf(
                                    errlogMajor,
                                    "Socket poll error: server socket no longer bound.");
                        }
                    }

                    // retval == 0 : timeout

                    // retval < 0 : error
                    if(retval<0) errlogSevPrintf(errlogMajor,
                            "Socket poll error: %s", strerror(errno));
                }
            } catch(...) {
                // TODO: catch all exceptions, and act accordingly
                close(true);
            }
        }

        bool BlockingUDPTransport::processBuffer(osiSockAddr* fromAddress,
                ByteBuffer* receiveBuffer) {
            // TODO: implement
            return true;
        }

    }
}
