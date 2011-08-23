/* blockingUDPTransport.cpp
 *
 *  Created on: Dec 20, 2010
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include <pv/blockingUDP.h>

#include <pv/caConstants.h>
#include <pv/inetAddressUtil.h>

/* pvData */
#include <pv/byteBuffer.h>
#include <pv/lock.h>
#include <pv/CDRMonitor.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <errlog.h>
#include <epicsThread.h>

/* standard */
#include <cstdio>
#include <sys/types.h>
#include <sys/socket.h>

using namespace epics::pvData;
using namespace std;

namespace epics {
    namespace pvAccess {

        PVDATA_REFCOUNT_MONITOR_DEFINE(blockingUDPTransport);

        BlockingUDPTransport::BlockingUDPTransport(
                auto_ptr<ResponseHandler>& responseHandler, SOCKET channel,
                osiSockAddr& bindAddress,
                short remoteTransportRevision) :
                    _closed(false),
                    _responseHandler(responseHandler),
                    _channel(channel),
                    _bindAddress(bindAddress),
                    _sendAddresses(0),
                    _ignoredAddresses(0),
                    _sendToEnabled(false),
                    _receiveBuffer(new ByteBuffer(MAX_UDP_RECV, EPICS_ENDIAN_BIG)),
                    _sendBuffer(new ByteBuffer(MAX_UDP_RECV, EPICS_ENDIAN_BIG)),
                    _lastMessageStartPosition(0),
                    _threadId(0)
        {
            PVDATA_REFCOUNT_MONITOR_CONSTRUCT(blockingUDPTransport);

            // set receive timeout so that we do not have problems at shutdown (recvfrom would block)
            struct timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            if (setsockopt (_channel, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout)) < 0)
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                errlogSevPrintf(errlogMajor,
                    "Failed to set SO_RCVTIMEO for UDP socket %s: %s.",
                    inetAddressToString(_bindAddress).c_str(), errStr);
            }


        }

        BlockingUDPTransport::~BlockingUDPTransport() {
            PVDATA_REFCOUNT_MONITOR_DESTRUCT(blockingUDPTransport);

            close(true); // close the socket and stop the thread.
            
            if (_sendAddresses) delete _sendAddresses;
            if (_ignoredAddresses) delete _ignoredAddresses;

            delete _receiveBuffer;
            delete _sendBuffer;
        }

        void BlockingUDPTransport::start() {

            String threadName = "UDP-receive "+inetAddressToString(_bindAddress);
            errlogSevPrintf(errlogInfo, "Starting thread: %s",threadName.c_str());

            _threadId = epicsThreadCreate(threadName.c_str(),
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackMedium),
                    BlockingUDPTransport::threadRunner, this);
        }

        void BlockingUDPTransport::close(bool forced) {
            close(forced, true);
        }

        void BlockingUDPTransport::close(bool forced, bool waitForThreadToComplete) {
            {
                Lock guard(_mutex);
                if(_closed) return;
                _closed = true;
    
                errlogSevPrintf(errlogInfo,
                    "UDP socket %s closed.",
                    inetAddressToString(_bindAddress).c_str());
    
                // TODO should I wait thread to complete first and then destroy
                epicsSocketDestroy(_channel);
            }
            
            
            // wait for send thread to exit cleanly            
            if (waitForThreadToComplete)
            {
                if (!_shutdownEvent.wait(5.0))
                {
                    errlogSevPrintf(errlogMajor,
                        "Receive thread for UDP socket %s has not exited.",
                        inetAddressToString(_bindAddress).c_str());
                }
            }
        }

        void BlockingUDPTransport::enqueueSendRequest(TransportSender::shared_pointer const & sender) {
            Lock lock(_sendMutex);

            _sendToEnabled = false;
            _sendBuffer->clear();
            sender->lock();
            try {
                sender->send(_sendBuffer, this);
                sender->unlock();
                endMessage();
                if(!_sendToEnabled)
                    send(_sendBuffer);
                else
                    send(_sendBuffer, _sendTo);
            } catch(...) {
                sender->unlock();
            }
        }

        void BlockingUDPTransport::startMessage(int8 command, int ensureCapacity) {
            _lastMessageStartPosition = _sendBuffer->getPosition();
            _sendBuffer->putShort(CA_MAGIC_AND_VERSION);
            _sendBuffer->putByte(0); // data
            _sendBuffer->putByte(command); // command
            _sendBuffer->putInt(0); // temporary zero payload
        }

        void BlockingUDPTransport::endMessage() {
            _sendBuffer->putInt(
                    _lastMessageStartPosition+(sizeof(int16)+2),
                    _sendBuffer->getPosition()-_lastMessageStartPosition-CA_MESSAGE_HEADER_SIZE);
        }

        void BlockingUDPTransport::processRead() {
            // This function is always called from only one thread - this
            // object's own thread.

            char _readBuffer[MAX_UDP_RECV];
            osiSockAddr fromAddress;
            Transport::shared_pointer thisTransport = shared_from_this();

            try {

                bool closed;
                while(!_closed)
                {
                    
                    _mutex.lock();
                    closed = _closed;
                    _mutex.unlock();
                    if (closed)
                        break;
                        
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
                        if(_ignoredAddresses!=0)
                        {
                            for(size_t i = 0; i <_ignoredAddresses->size(); i++)
                            {
                                if(_ignoredAddresses->at(i).ia.sin_addr.s_addr==fromAddress.ia.sin_addr.s_addr)
                                {
                                    ignore = true;
                                    break;
                                }
                            }
                        }
                        
                        if(!ignore) {
                            // TODO do not copy.... wrap the buffer!!!
                            _receiveBuffer->put(_readBuffer, 0, 
                                    bytesRead <_receiveBuffer->getRemaining() ?
                                        bytesRead : _receiveBuffer->getRemaining()
                                    );

                            _receiveBuffer->flip();

                            processBuffer(thisTransport, fromAddress, _receiveBuffer);
                        }
                    }
                    else if (bytesRead == -1) {
                        
                        int socketError = SOCKERRNO;
                        
                        // interrupted or timeout
                        if (socketError == EINTR || 
                            socketError == EAGAIN ||
                            socketError == EWOULDBLOCK)
                            continue;
                            
                        if (socketError == SOCK_ECONNREFUSED || // avoid spurious ECONNREFUSED in Linux
                            socketError == SOCK_ECONNRESET)     // or ECONNRESET in Windows
                            continue;
                                                    
                        // log a 'recvfrom' error
                        if(!_closed)
                        {
                            char errStr[64];
                            epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                            errlogSevPrintf(errlogMajor, "Socket recvfrom error: %s", errStr);
                        }
                               
                        close(true, false);
                        break;
                    }

                }
            } catch(...) {
                // TODO: catch all exceptions, and act accordingly
                close(true, false);
            }

            char threadName[40];
            epicsThreadGetName(_threadId, threadName, 40);
            errlogSevPrintf(errlogInfo, "Thread '%s' exiting", threadName);
            
            _shutdownEvent.signal();
        }

        bool BlockingUDPTransport::processBuffer(Transport::shared_pointer const & thisTransport, osiSockAddr& fromAddress, ByteBuffer* receiveBuffer) {

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
                int nextRequestPosition = receiveBuffer->getPosition() + payloadSize;

                // payload size check
                if(nextRequestPosition>receiveBuffer->getLimit()) return false;

                // handle
                _responseHandler->handleResponse(&fromAddress, thisTransport,
                        (int8)(magicAndVersion&0xFF), command, payloadSize,
                        _receiveBuffer);

                // set position (e.g. in case handler did not read all)
                receiveBuffer->setPosition(nextRequestPosition);
            }

            //all ok
            return true;
        }

        bool BlockingUDPTransport::send(ByteBuffer* buffer, const osiSockAddr& address) {

            buffer->flip();
            int retval = sendto(_channel, buffer->getArray(),
                    buffer->getLimit(), 0, &(address.sa), sizeof(sockaddr));
            if(retval<0)
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                errlogSevPrintf(errlogMajor, "Socket sendto error: %s", errStr);
                return false;
            }

            return true;
        }
        
        bool BlockingUDPTransport::send(ByteBuffer* buffer) {
            if(!_sendAddresses) return false;

            for(size_t i = 0; i<_sendAddresses->size(); i++) {
                buffer->flip();
                int retval = sendto(_channel, buffer->getArray(),
                        buffer->getLimit(), 0, &(_sendAddresses->at(i).sa),
                        sizeof(sockaddr));
                {
                    if(retval<0)
                    {
                        char errStr[64];
                        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                        errlogSevPrintf(errlogMajor, "Socket sendto error: %s", errStr);
                    }
                    return false;
                }
            }

            return true;
        }

        int BlockingUDPTransport::getSocketReceiveBufferSize() const {
            // Get value of the SO_RCVBUF option for this DatagramSocket,
            // that is the buffer size used by the platform for input on
            // this DatagramSocket.

            int sockBufSize = -1;
            socklen_t intLen = sizeof(int);

            int retval = getsockopt(_channel, SOL_SOCKET, SO_RCVBUF, &sockBufSize, &intLen);
            if(retval<0) 
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                errlogSevPrintf(errlogMajor, "Socket getsockopt SO_RCVBUF error: %s", errStr);
            }

            return sockBufSize;
        }

        void BlockingUDPTransport::threadRunner(void* param) {
            ((BlockingUDPTransport*)param)->processRead();
        }

    }
}
