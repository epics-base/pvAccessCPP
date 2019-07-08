/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sstream>

#include <epicsThread.h>
#include <osiSock.h>

#include <pv/epicsException.h>

#define epicsExportSharedSymbols
#include <pv/blockingTCP.h>
#include <pv/codec.h>
#include <pv/remote.h>
#include <pv/logger.h>

using std::ostringstream;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

BlockingTCPAcceptor::BlockingTCPAcceptor(
    Context::shared_pointer const & context,
    ResponseHandler::shared_pointer const & responseHandler,
    int port,
    int receiveBufferSize) :
    _context(context),
    _responseHandler(responseHandler),
    _bindAddress(),
    _serverSocketChannel(INVALID_SOCKET),
    _receiveBufferSize(receiveBufferSize),
    _destroyed(false),
    _thread(*this, "TCP-acceptor",
            epicsThreadGetStackSize(
                epicsThreadStackBig),
            epicsThreadPriorityMedium)
{
    memset(&_bindAddress, 0, sizeof(_bindAddress));
    _bindAddress.ia.sin_family = AF_INET;
    _bindAddress.ia.sin_port = htons(port);
    _bindAddress.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    initialize();
}

BlockingTCPAcceptor::BlockingTCPAcceptor(Context::shared_pointer const & context,
        ResponseHandler::shared_pointer const & responseHandler,
        const osiSockAddr& addr, int receiveBufferSize) :
    _context(context),
    _responseHandler(responseHandler),
    _bindAddress(),
    _serverSocketChannel(INVALID_SOCKET),
    _receiveBufferSize(receiveBufferSize),
    _destroyed(false),
    _thread(*this, "TCP-acceptor",
            epicsThreadGetStackSize(
                epicsThreadStackBig),
            epicsThreadPriorityMedium)
{
    _bindAddress = addr;
    initialize();
}

BlockingTCPAcceptor::~BlockingTCPAcceptor() {
    destroy();
}

int BlockingTCPAcceptor::initialize() {

    char ipAddrStr[48];
    ipAddrToDottedIP(&_bindAddress.ia, ipAddrStr, sizeof(ipAddrStr));

    int tryCount = 0;
    while(tryCount<2) {
        char strBuffer[64];

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
                LOG(logLevelDebug, "Socket bind error: %s.", strBuffer);
                if(_bindAddress.ia.sin_port!=0) {
                    // failed to bind to specified bind address,
                    // try to get port dynamically, but only once
                    LOG(
                        logLevelDebug,
                        "Configured TCP port %d is unavailable, trying to assign it dynamically.",
                        ntohs(_bindAddress.ia.sin_port));
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

                retval = ::listen(_serverSocketChannel, 4);
                if(retval<0) {
                    epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                    ostringstream temp;
                    temp<<"Socket listen error: "<<strBuffer;
                    LOG(logLevelError, "%s", temp.str().c_str());
                    THROW_BASE_EXCEPTION(temp.str().c_str());
                }

                _thread.start();

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

void BlockingTCPAcceptor::run() {
    // rise level if port is assigned dynamically
    char ipAddrStr[48];
    ipAddrToDottedIP(&_bindAddress.ia, ipAddrStr, sizeof(ipAddrStr));
    LOG(logLevelDebug, "Accepting connections at %s.", ipAddrStr);

    bool socketOpen = true;
    char strBuffer[64];

    while(socketOpen) {

        SOCKET sock;
        {
            Lock guard(_mutex);
            if (_destroyed)
                break;
            sock = _serverSocketChannel;
        }

        osiSockAddr address;
        osiSocklen_t len = sizeof(sockaddr);

        SOCKET newClient = epicsSocketAccept(sock, &address.sa, &len);
        if(newClient!=INVALID_SOCKET) {
            // accept succeeded
            ipAddrToDottedIP(&address.ia, ipAddrStr, sizeof(ipAddrStr));
            LOG(logLevelDebug, "Accepted connection from PVA client: %s.", ipAddrStr);

            // enable TCP_NODELAY (disable Nagle's algorithm)
            int optval = 1; // true
            int retval = ::setsockopt(newClient, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(int));
            if(retval<0) {
                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "Error setting TCP_NODELAY: %s.", strBuffer);
            }

            // enable TCP_KEEPALIVE
            retval = ::setsockopt(newClient, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(int));
            if(retval<0) {
                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "Error setting SO_KEEPALIVE: %s.", strBuffer);
            }

            // do NOT tune socket buffer sizes, this will disable auto-tunning

            // get TCP send buffer size
            osiSocklen_t intLen = sizeof(int);
            int _socketSendBufferSize;
            retval = getsockopt(newClient, SOL_SOCKET, SO_SNDBUF, (char *)&_socketSendBufferSize, &intLen);
            if(retval<0) {
                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "Error getting SO_SNDBUF: %s.", strBuffer);
            }

            /**
             * Create transport, it registers itself to the registry.
             */
            detail::BlockingServerTCPTransportCodec::shared_pointer transport =
                detail::BlockingServerTCPTransportCodec::create(
                    _context,
                    newClient,
                    _responseHandler,
                    _socketSendBufferSize,
                    _receiveBufferSize);

            // validate connection
            if(!validateConnection(transport, ipAddrStr)) {
                // TODO
                // wait for negative response to be sent back and
                // hold off the client for retrying at very high rate
                epicsThreadSleep(1.0);

                transport->close();
                LOG(
                    logLevelDebug,
                    "Connection to PVA client %s failed to be validated, closing it.",
                    ipAddrStr);
                continue;
            }

            LOG(logLevelDebug, "Serving to PVA client: %s.", ipAddrStr);

        }// accept succeeded
        else
            socketOpen = false;
    } // while
}

bool BlockingTCPAcceptor::validateConnection(Transport::shared_pointer const & transport, const char* address) {
    try {
        return transport->verify(5000);
    } catch(...) {
        LOG(logLevelDebug, "Validation of %s failed.", address);
        return false;
    }
}

void BlockingTCPAcceptor::destroy() {
    SOCKET sock;
    {
        Lock guard(_mutex);
        if(_destroyed) return;
        _destroyed = true;

        sock = _serverSocketChannel;
        _serverSocketChannel = INVALID_SOCKET;
    }

    if(sock!=INVALID_SOCKET) {
        char ipAddrStr[48];
        ipAddrToDottedIP(&_bindAddress.ia, ipAddrStr, sizeof(ipAddrStr));
        LOG(logLevelDebug, "Stopped accepting connections at %s.", ipAddrStr);

        switch(epicsSocketSystemCallInterruptMechanismQuery())
        {
        case esscimqi_socketBothShutdownRequired:
            shutdown(sock, SHUT_RDWR);
            epicsSocketDestroy(sock);
            _thread.exitWait();
            break;
        case esscimqi_socketSigAlarmRequired:
            LOG(logLevelError, "SigAlarm close not implemented for this target\n");
        case esscimqi_socketCloseRequired:
            epicsSocketDestroy(sock);
            _thread.exitWait();
            break;
        }
    }
}

}
}
