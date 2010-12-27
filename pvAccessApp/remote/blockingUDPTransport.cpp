/* blockingUDPTransport.cpp
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
#include <epicsThread.h>

/* standard */
#include <cstdio>
#include <unistd.h>
#include <poll.h>
#include <errno.h>

namespace epics {
    namespace pvAccess {

        using namespace epics::pvData;

        BlockingUDPTransport::BlockingUDPTransport(
                ResponseHandler* responseHandler, SOCKET channel,
                osiSockAddr* bindAddress, InetAddrVector* sendAddresses,
                short remoteTransportRevision) {
            _responseHandler = responseHandler;
            _channel = channel;
            _bindAddress = bindAddress;
            _sendAddresses = sendAddresses;

            _socketAddress = bindAddress;

            // allocate receive buffer
            _receiveBuffer = new ByteBuffer(MAX_UDP_RECV);

            // allocate send buffer and non-reentrant lock
            _sendBuffer = new ByteBuffer(MAX_UDP_SEND);

            _ignoredAddresses = NULL;
            _sendTo = NULL;
            _closed = false;
            _lastMessageStartPosition = 0;
            _readBuffer = new char[MAX_UDP_RECV];
        }

        BlockingUDPTransport::~BlockingUDPTransport() {
            delete _receiveBuffer;
            delete _sendBuffer;
            delete _readBuffer;
        }

        void BlockingUDPTransport::start() {
            String threadName = "UDP-receive "+inetAddressToString(
                    _socketAddress);

            errlogSevPrintf(errlogInfo, "Starting thread: %s",
                    threadName.c_str());

            epicsThreadCreate(threadName.c_str(), epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackMedium),
                    BlockingUDPTransport::threadRunner, this);
        }

        void BlockingUDPTransport::close(bool forced) {
            if(_closed) return;
            _closed = true;

            if(_bindAddress!=NULL) errlogSevPrintf(errlogInfo,
                    "UDP connection to %s closed.", inetAddressToString(
                            _bindAddress).c_str());

            int retval = ::close(_channel);

            if(retval<0) errlogSevPrintf(errlogMajor, "Socket close error: %s",
                    strerror(errno));
        }

        void BlockingUDPTransport::enqueueSendRequest(TransportSender* sender) {
            // TODO: Java version uses synchronized. Why?

            _sendTo = NULL;
            _sendBuffer->clear();
            sender->lock();
            try {
                sender->send(_sendBuffer, this);
                sender->unlock();
                endMessage();
                if(_sendTo!=NULL) send(_sendBuffer, _sendTo);
            } catch(...) {
                sender->unlock();
            }
        }

        void BlockingUDPTransport::startMessage(int8 command,
                int ensureCapacity) {
            _lastMessageStartPosition = _sendBuffer->getPosition();
            _sendBuffer->putShort(CA_MAGIC_AND_VERSION);
            _sendBuffer->putByte(0); // data
            _sendBuffer->putByte(command); // command
            _sendBuffer->putInt(0); // temporary zero payload
        }

        void BlockingUDPTransport::endMessage() {
            int32 data = _lastMessageStartPosition+(16/8+2);
            _sendBuffer->put((char*)&data, _sendBuffer->getPosition()
                    -_lastMessageStartPosition-CA_MESSAGE_HEADER_SIZE,
                    sizeof(int32));
        }

        void BlockingUDPTransport::processRead() {
            // This function is always called from only one thread - this
            // object's own thread.

            pollfd pfd;
            pfd.fd = _channel;
            pfd.events = POLLIN;

            osiSockAddr fromAddress;

            try {

                while(!_closed) {
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
                            _receiveBuffer->clear();

                            socklen_t addrStructSize = sizeof(sockaddr);

                            int bytesRead = recvfrom(_channel, _readBuffer,
                                    MAX_UDP_RECV, 0, (sockaddr*)&fromAddress,
                                    &addrStructSize);

                            if(bytesRead>0) {
                                // successfully got datagram
                                bool ignore = false;
                                if(_ignoredAddresses!=NULL) for(int i = 0; i
                                        <_ignoredAddresses->size(); i++)
                                    if(_ignoredAddresses->at(i)->ia.sin_addr.s_addr
                                            ==fromAddress.ia.sin_addr.s_addr) {
                                        ignore = true;
                                        break;
                                    }

                                if(!ignore) {
                                    _receiveBuffer->put(
                                            _readBuffer,
                                            0,
                                            bytesRead
                                                    <_receiveBuffer->getRemaining() ? bytesRead
                                                    : _receiveBuffer->getRemaining());

                                    _receiveBuffer->flip();

                                    processBuffer(&fromAddress, _receiveBuffer);
                                }
                            }
                            else {
                                // log a 'recvfrom' error
                                if(bytesRead==-1) errlogSevPrintf(errlogMajor,
                                        "Socket recv error: %s",
                                        strerror(errno));
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

            // handle response(s)
            while(receiveBuffer->getRemaining()>=CA_MESSAGE_HEADER_SIZE) {
                //
                // read header
                //

                // first byte is CA_MAGIC
                // second byte version - major/minor nibble
                // check magic and version at once
                short magicAndVersion = receiveBuffer->getShort();
                if((short)(magicAndVersion&0xFFF0)!=CA_MAGIC_AND_MAJOR_VERSION) return false;

                // only data for UDP
                receiveBuffer->getByte();

                // command ID and paylaod
                int8 command = receiveBuffer->getByte();
                int payloadSize = receiveBuffer->getInt();
                int nextRequestPosition = receiveBuffer->getPosition()
                        +payloadSize;

                // payload size check
                if(nextRequestPosition>receiveBuffer->getLimit()) return false;

                // handle
                _responseHandler->handleResponse(fromAddress, this,
                        (int8)(magicAndVersion&0xFF), command, payloadSize,
                        _receiveBuffer);

                // set position (e.g. in case handler did not read all)
                receiveBuffer->setPosition(nextRequestPosition);
            }

            //all ok
            return true;
        }

        bool BlockingUDPTransport::send(ByteBuffer* buffer,
                const osiSockAddr* address) {
            if(address==NULL||_sendAddresses==NULL) return false;

            if(address!=NULL) {
                buffer->flip();
                int retval =
                        sendto(_channel, buffer->getArray(),
                                buffer->getLimit(), 0, &(address->sa),
                                sizeof(sockaddr));
                if(retval<0) {
                    errlogSevPrintf(errlogMajor, "Socket sendto error: %s",
                            strerror(errno));
                    return false;
                }
            }
            else {
                for(int i = 0; i<_sendAddresses->size(); i++) {
                    buffer->flip();
                    int retval = sendto(_channel, buffer->getArray(),
                            buffer->getLimit(), 0,
                            &(_sendAddresses->at(i)->sa), sizeof(sockaddr));
                    {
                        if(retval<0) errlogSevPrintf(errlogMajor,
                                "Socket sendto error: %s", strerror(errno));
                        return false;
                    }
                }
            }

            return true;
        }

        int BlockingUDPTransport::getSocketReceiveBufferSize() const {
            // Get value of the SO_RCVBUF option for this DatagramSocket,
            // that is the buffer size used by the platform for input on
            // this DatagramSocket.

            int sockBufSize;
            socklen_t intLen;

            intLen = sizeof(int);

            int retval = getsockopt(_channel, SOL_SOCKET, SO_RCVBUF,
                    &sockBufSize, &intLen);
            if(retval<0) errlogSevPrintf(errlogMajor,
                    "Socket getsockopt SO_RCVBUF error: %s", strerror(errno));

            return sockBufSize;
        }

        void BlockingUDPTransport::threadRunner(void* param) {
            ((BlockingUDPTransport*)param)->processRead();
        }

    }
}
