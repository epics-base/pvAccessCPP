/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/logger.h>

#include <pv/epicsException.h>

#include <osiSock.h>
#include <epicsThread.h>

#include <sstream>

using std::ostringstream;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

        BlockingTCPAcceptor::BlockingTCPAcceptor(
                Context::shared_pointer const & context,
                ResponseHandlerFactory::shared_pointer const & responseHandlerFactory,
                int port,
                int receiveBufferSize) :
            _context(context),
            _responseHandlerFactory(responseHandlerFactory),
            _bindAddress(),
            _serverSocketChannel(INVALID_SOCKET),
            _receiveBufferSize(receiveBufferSize),
            _destroyed(false),
            _threadId(0)
        {
            initialize(port);
        }

        BlockingTCPAcceptor::~BlockingTCPAcceptor() {
            destroy();
        }

        int BlockingTCPAcceptor::initialize(unsigned short port) {
            // specified bind address
            _bindAddress.ia.sin_family = AF_INET;
            _bindAddress.ia.sin_port = htons(port);
            _bindAddress.ia.sin_addr.s_addr = htonl(INADDR_ANY);

            char strBuffer[64];
            char ipAddrStr[48];
            ipAddrToDottedIP(&_bindAddress.ia, ipAddrStr, sizeof(ipAddrStr));

            int tryCount = 0;
            while(tryCount<2) {

                LOG(logLevelDebug, "Creating acceptor to %s.", ipAddrStr);

                _serverSocketChannel = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if(_serverSocketChannel==INVALID_SOCKET) {
                    epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                    ostringstream temp;
                    temp<<"Socket create error: "<<strBuffer;
                    LOG(logLevelError, "%s", temp.str().c_str());
                    THROW_BASE_EXCEPTION(temp.str().c_str());
                }
                else {
                    
                    //epicsSocketEnableAddressReuseDuringTimeWaitState(_serverSocketChannel);

                    // try to bind
                    int retval = ::bind(_serverSocketChannel, &_bindAddress.sa, sizeof(sockaddr));
                    if(retval<0) {
                        epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                        LOG(logLevelDebug, "Socket bind error: %s", strBuffer);
                        if(_bindAddress.ia.sin_port!=0) {
                            // failed to bind to specified bind address,
                            // try to get port dynamically, but only once
                            LOG(
                                    logLevelDebug,
                                    "Configured TCP port %d is unavailable, trying to assign it dynamically.",
                                    port);
                            _bindAddress.ia.sin_port = htons(0);
                        }
                        else {
                            epicsSocketDestroy(_serverSocketChannel);
                            break; // exit while loop
                        }
                    }
                    else { // if(retval<0)
                        // bind succeeded

                        // update bind address, if dynamically port selection was used
                        if(ntohs(_bindAddress.ia.sin_port)==0) {
                            osiSocklen_t sockLen = sizeof(sockaddr);
                            // read the actual socket info
                            retval = ::getsockname(_serverSocketChannel, &_bindAddress.sa, &sockLen);
                            if(retval<0) {
                                // error obtaining port number
                                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                                LOG(logLevelDebug, "getsockname error: %s", strBuffer);
                            }
                            else {
                                LOG(
                                        logLevelInfo,
                                        "Using dynamically assigned TCP port %d.",
                                        ntohs(_bindAddress.ia.sin_port));
                            }
                        }

                        retval = ::listen(_serverSocketChannel, 1024);
                        if(retval<0) {
                            epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                            ostringstream temp;
                            temp<<"Socket listen error: "<<strBuffer;
                            LOG(logLevelError, "%s", temp.str().c_str());
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
                        return ntohs(_bindAddress.ia.sin_port);
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
            ipAddrToDottedIP(&_bindAddress.ia, ipAddrStr, sizeof(ipAddrStr));
            LOG(logLevelDebug, "Accepting connections at %s.", ipAddrStr);

            bool socketOpen = true;
            char strBuffer[64];

            while(socketOpen) {
                
                {
                    Lock guard(_mutex);
                    if (_destroyed)
                        break;
                }
                
                osiSockAddr address;
                osiSocklen_t len = sizeof(sockaddr);

                SOCKET newClient = epicsSocketAccept(_serverSocketChannel, &address.sa, &len);
                if(newClient!=INVALID_SOCKET) {
                    // accept succeeded
                    ipAddrToDottedIP(&address.ia, ipAddrStr, sizeof(ipAddrStr));
                    LOG(logLevelDebug, "Accepted connection from PVA client: %s", ipAddrStr);

                    // enable TCP_NODELAY (disable Nagle's algorithm)
                    int optval = 1; // true
                    int retval = ::setsockopt(newClient, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(int));
                    if(retval<0) {
                        epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                        LOG(logLevelDebug, "Error setting TCP_NODELAY: %s", strBuffer);
                    }

                    // enable TCP_KEEPALIVE
                    retval = ::setsockopt(newClient, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(int));
                    if(retval<0) {
                        epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                        LOG(logLevelDebug, "Error setting SO_KEEPALIVE: %s", strBuffer);
                    }

                    // TODO tune buffer sizes?!

                    /**
                     * Create transport, it registers itself to the registry.
                     * Each transport should have its own response handler since it is not "shareable"
                     */
                    std::auto_ptr<ResponseHandler> responseHandler = _responseHandlerFactory->createResponseHandler();
                    BlockingServerTCPTransport::shared_pointer transport = 
                                    BlockingServerTCPTransport::create(
                                            _context,
                                            newClient,
                                            responseHandler,
                                            _receiveBufferSize);

                    // validate connection
                    if(!validateConnection(transport, ipAddrStr)) {
                        transport->close();
                        LOG(
                                logLevelDebug,
                                "Connection to PVA client %s failed to be validated, closing it.",
                                ipAddrStr);
                        return;
                    }

                    LOG(logLevelDebug, "Serving to PVA client: %s", ipAddrStr);

                }// accept succeeded
                else
                    socketOpen = false;
            } // while
        }

        bool BlockingTCPAcceptor::validateConnection(Transport::shared_pointer const & transport, const char* address) {
            try {
                transport->verify(0);
                return true;
            } catch(...) {
                LOG(logLevelDebug, "Validation of %s failed.", address);
                return false;
            }
        }

        void BlockingTCPAcceptor::handleEventsRunner(void* param) {
            ((BlockingTCPAcceptor*)param)->handleEvents();
        }

        void BlockingTCPAcceptor::destroy() {
            Lock guard(_mutex);
            if(_destroyed) return;
            _destroyed = true;

            if(_serverSocketChannel!=INVALID_SOCKET) {
                char ipAddrStr[48];
                ipAddrToDottedIP(&_bindAddress.ia, ipAddrStr, sizeof(ipAddrStr));
                LOG(logLevelDebug, "Stopped accepting connections at %s.", ipAddrStr);

                epicsSocketDestroy(_serverSocketChannel);
            }
        }

    }
}
