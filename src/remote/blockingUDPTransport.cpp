/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifdef _WIN32
// needed for ip_mreq
#include <Ws2tcpip.h>
#endif

#include <pv/blockingUDP.h>
#include <pv/pvaConstants.h>
#include <pv/inetAddressUtil.h>
#include <pv/logger.h>
#include <pv/likely.h>

#include <pv/byteBuffer.h>
#include <pv/lock.h>

#include <osdSock.h>
#include <osiSock.h>
#include <epicsThread.h>

#include <cstdio>
#include <sys/types.h>

using namespace epics::pvData;
using namespace std;

namespace epics {
    namespace pvAccess {

#ifdef __vxworks
inline int sendto(int s, const char *buf, size_t len, int flags, const struct sockaddr *to, int tolen)
{
    return ::sendto(s, const_cast<char*>(buf), len, flags, const_cast<struct sockaddr *>(to), tolen);
}
#endif

        PVACCESS_REFCOUNT_MONITOR_DEFINE(blockingUDPTransport);

        BlockingUDPTransport::BlockingUDPTransport(
                bool serverFlag,
                auto_ptr<ResponseHandler>& responseHandler, SOCKET channel,
                osiSockAddr& bindAddress,
                short /*remoteTransportRevision*/) :
                    _closed(),
                    _responseHandler(responseHandler),
                    _channel(channel),
                    _bindAddress(bindAddress),
                    _sendAddresses(0),
                    _ignoredAddresses(0),
                    _sendToEnabled(false),
                    _receiveBuffer(new ByteBuffer(MAX_UDP_RECV)),
                    _sendBuffer(new ByteBuffer(MAX_UDP_RECV)),
                    _lastMessageStartPosition(0),
                    _threadId(0),
                    _clientServerWithEndianFlag(
                        (serverFlag ? 0x40 : 0x00) | ((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG) ? 0x80 : 0x00))
        {
            PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(blockingUDPTransport);

            osiSocklen_t sockLen = sizeof(sockaddr);
            // read the actual socket info
            int retval = ::getsockname(_channel, &_remoteAddress.sa, &sockLen);
            if(retval<0) {
                // error obtaining remote address, fallback to bindAddress
                _remoteAddress = _bindAddress;

                char strBuffer[64];
                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "getsockname error: %s.", strBuffer);
            }
        }

        BlockingUDPTransport::~BlockingUDPTransport() {
            PVACCESS_REFCOUNT_MONITOR_DESTRUCT(blockingUDPTransport);

            close(true); // close the socket and stop the thread.
            
            // TODO use auto_ptr class members
            
            if (_sendAddresses) delete _sendAddresses;
            if (_ignoredAddresses) delete _ignoredAddresses;
        }

        void BlockingUDPTransport::start() {

            string threadName = "UDP-receive " + inetAddressToString(_bindAddress);
            
            if (IS_LOGGABLE(logLevelTrace))
            {
                LOG(logLevelTrace, "Starting thread: %s.", threadName.c_str());
            }
            
            _threadId = epicsThreadCreate(threadName.c_str(),
                    epicsThreadPriorityMedium,
                    epicsThreadGetStackSize(epicsThreadStackSmall),
                    BlockingUDPTransport::threadRunner, this);
        }

        void BlockingUDPTransport::close() {
            close(true);
        }

        void BlockingUDPTransport::close(bool waitForThreadToComplete) {
            {
                Lock guard(_mutex);
                if(_closed.get()) return;
                _closed.set();
            }

            if (IS_LOGGABLE(logLevelDebug))
            {
                LOG(logLevelDebug,
                    "UDP socket %s closed.",
                    inetAddressToString(_bindAddress).c_str());
            }
            
            epicsSocketSystemCallInterruptMechanismQueryInfo info  =
                epicsSocketSystemCallInterruptMechanismQuery ();
            switch ( info )
            {
                case esscimqi_socketCloseRequired:
                    epicsSocketDestroy ( _channel );
                    break;
                case esscimqi_socketBothShutdownRequired:
                    {
                        /*int status =*/ ::shutdown ( _channel, SHUT_RDWR );
                        /*
                        if ( status ) {
                            char sockErrBuf[64];
                            epicsSocketConvertErrnoToString (
                                sockErrBuf, sizeof ( sockErrBuf ) );
                        LOG(logLevelDebug,
                            "UDP socket %s failed to shutdown: %s.",
                            inetAddressToString(_bindAddress).c_str(), sockErrBuf);
                        }
                        */
                        epicsSocketDestroy ( _channel );
                    }
                    break;
                case esscimqi_socketSigAlarmRequired:
                    // not supported anymore anyway
                default:
                    epicsSocketDestroy(_channel);
            }
            
            
            // wait for send thread to exit cleanly            
            if (waitForThreadToComplete)
            {
                if (!_shutdownEvent.wait(5.0))
                {
                    LOG(logLevelError,
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
                sender->send(_sendBuffer.get(), this);
                sender->unlock();
                endMessage();
                if(!_sendToEnabled)
                    send(_sendBuffer.get());
                else
                    send(_sendBuffer.get(), _sendTo);
            } catch(...) {
                sender->unlock();
            }
        }


        void BlockingUDPTransport::flushSendQueue()
        {
            // noop (note different sent addresses are possible)
        }

        void BlockingUDPTransport::startMessage(int8 command, size_t /*ensureCapacity*/, int32 payloadSize) {
            _lastMessageStartPosition = _sendBuffer->getPosition();
            _sendBuffer->putByte(PVA_MAGIC);
            _sendBuffer->putByte(PVA_VERSION);
            _sendBuffer->putByte(_clientServerWithEndianFlag);
            _sendBuffer->putByte(command); // command
            _sendBuffer->putInt(payloadSize);
        }

        void BlockingUDPTransport::endMessage() {
    		//we always (for now) send by packet, so no need for this here...
    		//alignBuffer(PVA_ALIGNMENT);
            _sendBuffer->putInt(
                    _lastMessageStartPosition+(sizeof(int16)+2),
                    _sendBuffer->getPosition()-_lastMessageStartPosition-PVA_MESSAGE_HEADER_SIZE);
        }

        void BlockingUDPTransport::processRead() {
            // This function is always called from only one thread - this
            // object's own thread.

            osiSockAddr fromAddress;
            osiSocklen_t addrStructSize = sizeof(sockaddr);
            Transport::shared_pointer thisTransport = shared_from_this();

            try {

                while(!_closed.get())
                {
                    // we poll to prevent blocking indefinitely

                    // data ready to be read
                    _receiveBuffer->clear();

                    int bytesRead = recvfrom(_channel, (char*)_receiveBuffer->getArray(),
                            _receiveBuffer->getRemaining(), 0, (sockaddr*)&fromAddress,
                            &addrStructSize);

                    if(likely(bytesRead>0)) {
                        // successfully got datagram
                        bool ignore = false;
                        if(likely(_ignoredAddresses!=0))
                        {
                            for(size_t i = 0; i <_ignoredAddresses->size(); i++)
                            {
                                if((*_ignoredAddresses)[i].ia.sin_addr.s_addr==fromAddress.ia.sin_addr.s_addr)
                                {
                                    ignore = true;
                                    break;
                                }
                            }
                        }
                        
                        if(likely(!ignore)) {
                            _receiveBuffer->setPosition(bytesRead);

                            _receiveBuffer->flip();

                            processBuffer(thisTransport, fromAddress, _receiveBuffer.get());
                        }
                    }
                    else if (unlikely(bytesRead == -1)) {
                        
                        int socketError = SOCKERRNO;
                        
                        // interrupted or timeout
                        if (socketError == SOCK_EINTR || 
                            socketError == EAGAIN ||        // no alias in libCom
                            // windows times out with this
                            socketError == SOCK_ETIMEDOUT || 
                            socketError == SOCK_EWOULDBLOCK)
                            continue;
                            
                        if (socketError == SOCK_ECONNREFUSED || // avoid spurious ECONNREFUSED in Linux
                            socketError == SOCK_ECONNRESET)     // or ECONNRESET in Windows
                            continue;
                                                    
                        // log a 'recvfrom' error
                        if(!_closed.get())
                        {
                            char errStr[64];
                            epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                            LOG(logLevelError, "Socket recvfrom error: %s.", errStr);
                        }
                               
                        close(false);
                        break;
                    }

                }
            } catch(...) {
                // TODO: catch all exceptions, and act accordingly
                close(false);
            }

            if (IS_LOGGABLE(logLevelTrace))
            {
                string threadName = "UDP-receive "+inetAddressToString(_bindAddress);
                LOG(logLevelTrace, "Thread '%s' exiting.", threadName.c_str());
            }
            
            _shutdownEvent.signal();
        }

        bool BlockingUDPTransport::processBuffer(Transport::shared_pointer const & thisTransport, osiSockAddr& fromAddress, ByteBuffer* receiveBuffer) {

            // handle response(s)
            while(likely((int)receiveBuffer->getRemaining()>=PVA_MESSAGE_HEADER_SIZE)) {
                //
                // read header
                //

                // first byte is PVA_MAGIC
                int8 magic = receiveBuffer->getByte();
                if(unlikely(magic != PVA_MAGIC))
                    return false;

                // second byte version
                int8 version = receiveBuffer->getByte();

                // only data for UDP
                int8 flags = receiveBuffer->getByte();
                if (flags < 0)
                {
                    // 7-bit set
                    receiveBuffer->setEndianess(EPICS_ENDIAN_BIG);
                }
                else
                {
                    receiveBuffer->setEndianess(EPICS_ENDIAN_LITTLE);
                }

                // command ID and paylaod
                int8 command = receiveBuffer->getByte();
                // TODO check this cast (size_t must be 32-bit)
                size_t payloadSize = receiveBuffer->getInt();
                size_t nextRequestPosition = receiveBuffer->getPosition() + payloadSize;

                // payload size check
                if(unlikely(nextRequestPosition>receiveBuffer->getLimit())) return false;

                // handle
                _responseHandler->handleResponse(&fromAddress, thisTransport,
                        version, command, payloadSize,
                        _receiveBuffer.get());

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
            if(unlikely(retval<0))
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                LOG(logLevelDebug, "Socket sendto error: %s.", errStr);
                return false;
            }

            // all sent
            buffer->setPosition(buffer->getLimit());

            return true;
        }
        
        bool BlockingUDPTransport::send(ByteBuffer* buffer, InetAddressType target) {
            if(!_sendAddresses) return false;

            buffer->flip();

            bool allOK = true;
            for(size_t i = 0; i<_sendAddresses->size(); i++) {

                // filter
                if (target != inetAddressType_all)
                    if ((target == inetAddressType_unicast && !_isSendAddressUnicast[i]) ||
                        (target == inetAddressType_broadcast_multicast && _isSendAddressUnicast[i]))
                        continue;

                int retval = sendto(_channel, buffer->getArray(),
                        buffer->getLimit(), 0, &((*_sendAddresses)[i].sa),
                        sizeof(sockaddr));
                if(unlikely(retval<0))
                {
                    char errStr[64];
                    epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                    LOG(logLevelDebug, "Socket sendto error: %s.", errStr);
                    allOK = false;
                }
            }

            // all sent
            buffer->setPosition(buffer->getLimit());

            return allOK;
        }

        size_t BlockingUDPTransport::getSocketReceiveBufferSize() const {
            // Get value of the SO_RCVBUF option for this DatagramSocket,
            // that is the buffer size used by the platform for input on
            // this DatagramSocket.

            int sockBufSize = -1;
            osiSocklen_t intLen = sizeof(int);

            int retval = getsockopt(_channel, SOL_SOCKET, SO_RCVBUF, (char *)&sockBufSize, &intLen);
            if(unlikely(retval<0)) 
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                LOG(logLevelError, "Socket getsockopt SO_RCVBUF error: %s.", errStr);
            }

            return (size_t)sockBufSize;
        }

        void BlockingUDPTransport::threadRunner(void* param) {
            ((BlockingUDPTransport*)param)->processRead();
        }


        void BlockingUDPTransport::join(const osiSockAddr & mcastAddr, const osiSockAddr & nifAddr)
        {
            struct ip_mreq imreq;
            memset(&imreq, 0, sizeof(struct ip_mreq));

            imreq.imr_multiaddr.s_addr = mcastAddr.ia.sin_addr.s_addr;
            imreq.imr_interface.s_addr = nifAddr.ia.sin_addr.s_addr;

            // join multicast group on default interface
            int status = ::setsockopt(_channel, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                            (char*)&imreq, sizeof(struct ip_mreq));
            if (status)
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                throw std::runtime_error(
                            string("Failed to join to the multicast group '") +
                            inetAddressToString(mcastAddr) + "' on network interface '" +
                            inetAddressToString(nifAddr, false) + "': " + errStr);
            }
        }

        void BlockingUDPTransport::setMutlicastNIF(const osiSockAddr & nifAddr, bool loopback)
        {
            // set the multicast outgoing interface
            int status = ::setsockopt(_channel, IPPROTO_IP, IP_MULTICAST_IF,
                                  (char*)&nifAddr.ia.sin_addr, sizeof(struct in_addr));
            if (status)
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                throw std::runtime_error(
                            string("Failed to set multicast network inteface '") +
                            inetAddressToString(nifAddr, false) + "': " + errStr);
            }

            // send multicast traffic to myself too
            unsigned char mcast_loop = (loopback ? 1 : 0);
            status = ::setsockopt(_channel, IPPROTO_IP, IP_MULTICAST_LOOP,
                                (char*)&mcast_loop, sizeof(unsigned char));
            if (status)
            {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                throw std::runtime_error(
                            string("Failed to enable multicast loopback on network inteface '") +
                            inetAddressToString(nifAddr, false) + "': " + errStr);
            }

        }


    }
}
