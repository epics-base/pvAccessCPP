/* blockingUDPTransport.cpp
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingUDP.h"

#include "caConstants.h"
#include "inetAddressUtil.h"

/* pvData */
#include <byteBuffer.h>
#include <lock.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <errlog.h>
#include <epicsThread.h>

/* standard */
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

namespace epics {
    namespace pvAccess {

        using namespace epics::pvData;

        BlockingUDPTransport::BlockingUDPTransport(
                ResponseHandler* responseHandler, SOCKET channel,
                osiSockAddr& bindAddress, InetAddrVector* sendAddresses,
                short remoteTransportRevision) :
                    _closed(false),
                    _responseHandler(responseHandler),
                    _channel(channel),
                    _sendAddresses(sendAddresses),
                    _ignoredAddresses(NULL),
                    _sendTo(NULL),
                    _receiveBuffer(new ByteBuffer(MAX_UDP_RECV,
                            EPICS_ENDIAN_BIG)),
                    _sendBuffer(new ByteBuffer(MAX_UDP_RECV, EPICS_ENDIAN_BIG)),
                    _lastMessageStartPosition(0), _readBuffer(
                            new char[MAX_UDP_RECV]), _mutex(new Mutex()),
                    _threadId(NULL) {
            _socketAddress = new osiSockAddr;
            memcpy(_socketAddress, &bindAddress, sizeof(osiSockAddr));
            _bindAddress = _socketAddress;

        }

        BlockingUDPTransport::~BlockingUDPTransport() {
            close(true); // close the socket and stop the thread.
            if(_sendTo!=NULL) delete _sendTo;
            delete _socketAddress;
            // _bindAddress equals _socketAddress

            delete _receiveBuffer;
            delete _sendBuffer;
            delete[] _readBuffer;
            delete _mutex;
        }

        void BlockingUDPTransport::start() {
            String threadName = "UDP-receive "+inetAddressToString(
                    _socketAddress);

            errlogSevPrintf(errlogInfo, "Starting thread: %s",
                    threadName.c_str());

            _threadId = epicsThreadCreate(threadName.c_str(),
                    epicsThreadPriorityMedium, epicsThreadGetStackSize(
                            epicsThreadStackMedium),
                    BlockingUDPTransport::threadRunner, this);
        }

        void BlockingUDPTransport::close(bool forced) {
            if(_closed) return;
            _closed = true;

            if(_bindAddress!=NULL) errlogSevPrintf(errlogInfo,
                    "UDP socket %s closed.",
                    inetAddressToString(_bindAddress).c_str());

            epicsSocketDestroy(_channel);
        }

        void BlockingUDPTransport::enqueueSendRequest(TransportSender* sender) {
            Lock lock(_mutex);

            _sendTo = NULL;
            _sendBuffer->clear();
            sender->lock();
            try {
                sender->send(_sendBuffer, this);
                sender->unlock();
                endMessage();
                if(_sendTo==NULL)
                    send(_sendBuffer);
                else
                    send(_sendBuffer, *_sendTo);
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
            _sendBuffer->putInt(_lastMessageStartPosition+(sizeof(int16)+2),
                    _sendBuffer->getPosition()-_lastMessageStartPosition
                            -CA_MESSAGE_HEADER_SIZE);

        }

        void BlockingUDPTransport::processRead() {
            // This function is always called from only one thread - this
            // object's own thread.

            osiSockAddr fromAddress;

            try {

                while(!_closed) {
                    // we poll to prevent blocking indefinitely

                    // data ready to be read
                    _receiveBuffer->clear();

                    socklen_t addrStructSize = sizeof(sockaddr);

                    int bytesRead = recvfrom(_channel, _readBuffer,
                            MAX_UDP_RECV, 0, (sockaddr*)&fromAddress,
                            &addrStructSize);

                    if(bytesRead>0) {
                        // successfully got datagram
                        bool ignore = false;
                        if(_ignoredAddresses!=NULL) for(size_t i = 0; i
                                <_ignoredAddresses->size(); i++)
                            if(_ignoredAddresses->at(i)->ia.sin_addr.s_addr
                                    ==fromAddress.ia.sin_addr.s_addr) {
                                ignore = true;
                                break;
                            }

                        if(!ignore) {
                            _receiveBuffer->put(_readBuffer, 0, bytesRead
                                    <_receiveBuffer->getRemaining() ? bytesRead
                                    : _receiveBuffer->getRemaining());

                            _receiveBuffer->flip();

                            processBuffer(fromAddress, _receiveBuffer);
                        }
                    }
                    else {
                        // 0 == socket close

                        // log a 'recvfrom' error
                        if(bytesRead==-1) errlogSevPrintf(errlogMajor,
                                "Socket recv error: %s", strerror(errno));
                    }

                }
            } catch(...) {
                // TODO: catch all exceptions, and act accordingly
                close(true);
            }

            char threadName[40];
            epicsThreadGetName(_threadId, threadName, 40);
            errlogSevPrintf(errlogInfo, "Thread '%s' exiting", threadName);
        }

        bool BlockingUDPTransport::processBuffer(osiSockAddr& fromAddress,
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
                _responseHandler->handleResponse(&fromAddress, this,
                        (int8)(magicAndVersion&0xFF), command, payloadSize,
                        _receiveBuffer);

                // set position (e.g. in case handler did not read all)
                receiveBuffer->setPosition(nextRequestPosition);
            }

            //all ok
            return true;
        }

        bool BlockingUDPTransport::send(ByteBuffer* buffer,
                const osiSockAddr& address) {

            buffer->flip();
            int retval = sendto(_channel, buffer->getArray(),
                    buffer->getLimit(), 0, &(address.sa), sizeof(sockaddr));
            if(retval<0) {
                errlogSevPrintf(errlogMajor, "Socket sendto error: %s",
                        strerror(errno));
                return false;
            }

            return true;
        }

        bool BlockingUDPTransport::send(ByteBuffer* buffer) {
            if(_sendAddresses==NULL) return false;

            for(size_t i = 0; i<_sendAddresses->size(); i++) {
                buffer->flip();
                int retval = sendto(_channel, buffer->getArray(),
                        buffer->getLimit(), 0, &(_sendAddresses->at(i)->sa),
                        sizeof(sockaddr));
                {
                    if(retval<0) errlogSevPrintf(errlogMajor,
                            "Socket sendto error: %s", strerror(errno));
                    return false;
                }
            }

            return true;
        }

        int BlockingUDPTransport::getSocketReceiveBufferSize() const {
            // Get value of the SO_RCVBUF option for this DatagramSocket,
            // that is the buffer size used by the platform for input on
            // this DatagramSocket.

            int sockBufSize;
            socklen_t intLen = sizeof(int);

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
