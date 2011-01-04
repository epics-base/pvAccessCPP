/*
 * blockingTCPAcceptor.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingTCP.h"
#include "remote.h"
#include "serverContext.h"

/* pvData */
#include <epicsException.h>

/* EPICSv3 */
#include <errlog.h>
#include <osiSock.h>
#include <epicsThread.h>

/* standard */
#include <sstream>
#include <poll.h>

using std::ostringstream;

namespace epics {
    namespace pvAccess {

        BlockingTCPAcceptor::BlockingTCPAcceptor(Context* context, int port,
                int receiveBufferSize) :
            _context(context), _bindAddress(NULL), _serverSocketChannel(
                    INVALID_SOCKET), _receiveBufferSize(receiveBufferSize),
                    _destroyed(false), _threadId(NULL) {
            initialize(port);
        }

        BlockingTCPAcceptor::~BlockingTCPAcceptor() {
            if(_bindAddress!=NULL) delete _bindAddress;
        }

        int BlockingTCPAcceptor::initialize(in_port_t port) {
            // specified bind address
            _bindAddress = new osiSockAddr;
            _bindAddress->ia.sin_family = AF_INET;
            _bindAddress->ia.sin_port = htons(port);
            _bindAddress->ia.sin_addr.s_addr = htonl(INADDR_ANY);

            char strBuffer[64];
            char ipAddrStr[48];
            ipAddrToA(&_bindAddress->ia, ipAddrStr, sizeof(ipAddrStr));

            int tryCount = 0;
            while(tryCount<2) {

                errlogSevPrintf(errlogInfo, "Creating acceptor to %s.",
                        ipAddrStr);

                _serverSocketChannel = epicsSocketCreate(AF_INET, SOCK_STREAM,
                        IPPROTO_TCP);
                if(_serverSocketChannel==INVALID_SOCKET) {
                    epicsSocketConvertErrnoToString(strBuffer,
                            sizeof(strBuffer));
                    ostringstream temp;
                    temp<<"Socket create error: "<<strBuffer;
                    errlogSevPrintf(errlogMajor, temp.str().c_str());
                    THROW_BASE_EXCEPTION(temp.str().c_str());
                }
                else {
                    // try to bind
                    int retval = ::bind(_serverSocketChannel,
                            &_bindAddress->sa, sizeof(sockaddr));
                    if(retval<0) {
                        epicsSocketConvertErrnoToString(strBuffer,
                                sizeof(strBuffer));
                        errlogSevPrintf(errlogMinor, "Socket bind error: %s",
                                strBuffer);
                        if(_bindAddress->ia.sin_port!=0) {
                            // failed to bind to specified bind address,
                            // try to get port dynamically, but only once
                            errlogSevPrintf(
                                    errlogMinor,
                                    "Configured TCP port %d is unavailable, trying to assign it dynamically.",
                                    port);
                            _bindAddress->ia.sin_port = htons(0);
                        }
                        else {
                            ::close(_serverSocketChannel);
                            break; // exit while loop
                        }
                    }
                    else { // if(retval<0)
                        // bind succeeded

                        // update bind address, if dynamically port selection was used
                        if(ntohs(_bindAddress->ia.sin_port)==0) {
                            socklen_t sockLen = sizeof(sockaddr);
                            // read the actual socket info
                            retval = ::getsockname(_serverSocketChannel,
                                    &_bindAddress->sa, &sockLen);
                            if(retval<0) {
                                // error obtaining port number
                                epicsSocketConvertErrnoToString(strBuffer,
                                        sizeof(strBuffer));
                                errlogSevPrintf(errlogMinor,
                                        "getsockname error: %s", strBuffer);
                            }
                            else {
                                errlogSevPrintf(
                                        errlogInfo,
                                        "Using dynamically assigned TCP port %d.",
                                        ntohs(_bindAddress->ia.sin_port));
                            }
                        }

                        retval = ::listen(_serverSocketChannel, 5);
                        if(retval<0) {
                            epicsSocketConvertErrnoToString(strBuffer,
                                    sizeof(strBuffer));
                            ostringstream temp;
                            temp<<"Socket listen error: "<<strBuffer;
                            errlogSevPrintf(errlogMajor, temp.str().c_str());
                            THROW_BASE_EXCEPTION(temp.str().c_str());
                        }

                        _threadId
                                = epicsThreadCreate(
                                        "TCP-acceptor",
                                        epicsThreadPriorityMedium,
                                        epicsThreadGetStackSize(
                                                epicsThreadStackMedium),
                                        BlockingTCPAcceptor::handleEventsRunner,
                                        this);

                        // all OK, return
                        return ntohs(_bindAddress->ia.sin_port);
                    } // successful bind
                } // successfully obtained socket
                tryCount++;
            } // while

            ostringstream temp;
            temp<<"Failed to create acceptor to "<<ipAddrStr;
            THROW_BASE_EXCEPTION(temp.str().c_str());
        }

        void BlockingTCPAcceptor::handleEvents() {
            // rise level if port is assigned dynamically
            char ipAddrStr[48];
            ipAddrToA(&_bindAddress->ia, ipAddrStr, sizeof(ipAddrStr));
            errlogSevPrintf(errlogInfo, "Accepting connections at %s.",
                    ipAddrStr);

            bool socketOpen = true;
            char strBuffer[64];

            pollfd sockets[1];
            sockets[0].fd = _serverSocketChannel;
            sockets[0].events = POLLIN;

            while(!_destroyed&&socketOpen) {
                int retval = ::poll(sockets, 1, 50);
                if(retval<0) {
                    // error in poll
                    epicsSocketConvertErrnoToString(strBuffer,
                            sizeof(strBuffer));
                    errlogSevPrintf(errlogMajor, "socket poll error: %s",
                            strBuffer);
                    socketOpen = false;
                }
                else if(retval>0) {
                    // some event on a socket
                    if(sockets[0].revents&POLLIN!=0) {
                        // connection waiting

                        osiSockAddr address;
                        osiSocklen_t len = sizeof(sockaddr);

                        SOCKET newClient = epicsSocketAccept(
                                _serverSocketChannel, &address.sa, &len);
                        if(newClient!=INVALID_SOCKET) {
                            // accept succeeded
                            ipAddrToA(&address.ia, ipAddrStr, sizeof(ipAddrStr));
                            errlogSevPrintf(errlogInfo,
                                    "Accepted connection from CA client: %s",
                                    ipAddrStr);

                            // enable TCP_NODELAY (disable Nagle's algorithm)
                            int optval = 1; // true
                            retval = ::setsockopt(newClient, IPPROTO_TCP,
                                    TCP_NODELAY, &optval, sizeof(int));
                            if(retval<0) {
                                epicsSocketConvertErrnoToString(strBuffer,
                                        sizeof(strBuffer));
                                errlogSevPrintf(errlogMinor,
                                        "Error setting TCP_NODELAY: %s",
                                        strBuffer);
                            }

                            // enable TCP_KEEPALIVE
                            retval = ::setsockopt(newClient, SOL_SOCKET,
                                    SO_KEEPALIVE, &optval, sizeof(int));
                            if(retval<0) {
                                epicsSocketConvertErrnoToString(strBuffer,
                                        sizeof(strBuffer));
                                errlogSevPrintf(errlogMinor,
                                        "Error setting SO_KEEPALIVE: %s",
                                        strBuffer);
                            }

                            // TODO tune buffer sizes?!
                            //socket.socket().setReceiveBufferSize();
                            //socket.socket().setSendBufferSize();

                            // create transport
                            // each transport should have its own response handler since it is not "shareable"
                            BlockingServerTCPTransport
                                    * transport =
                                            new BlockingServerTCPTransport(
                                                    _context,
                                                    newClient,
                                                    new ServerResponseHandler(
                                                            (ServerContextImpl*)_context),
                                                    _receiveBufferSize);

                            // validate connection
                            if(!validateConnection(transport, ipAddrStr)) {
                                transport->close(true);
                                errlogSevPrintf(
                                        errlogInfo,
                                        "Connection to CA client %s failed to be validated, closing it.",
                                        ipAddrStr);
                                return;
                            }

                            errlogSevPrintf(errlogInfo,
                                    "Serving to CA client: %s", ipAddrStr);

                        }// accept succeeded
                    } // connection waiting
                    if(sockets[0].revents&(POLLERR|POLLHUP|POLLNVAL)!=0) {
                        errlogSevPrintf(errlogMajor,
                                "error on a socket: POLLERR|POLLHUP|POLLNVAL");
                        socketOpen = false;
                    }
                } // some event on a socket
            } // while
        }

        bool BlockingTCPAcceptor::validateConnection(
                BlockingServerTCPTransport* transport, const char* address) {
            try {
                transport->verify();
                return true;
            } catch(...) {
                errlogSevPrintf(errlogInfo, "Validation of %s failed.", address);
                return false;
            }
        }

        void BlockingTCPAcceptor::handleEventsRunner(void* param) {
            ((BlockingTCPAcceptor*)param)->handleEvents();
        }

        void BlockingTCPAcceptor::destroy() {
            if(_destroyed) return;
            _destroyed = true;

            if(_serverSocketChannel!=INVALID_SOCKET) {
                char ipAddrStr[48];
                ipAddrToA(&_bindAddress->ia, ipAddrStr, sizeof(ipAddrStr));
                errlogSevPrintf(errlogInfo,
                        "Stopped accepting connections at %s.", ipAddrStr);

                epicsSocketDestroy(_serverSocketChannel);
            }
        }

    }
}
