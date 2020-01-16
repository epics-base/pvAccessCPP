/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sstream>
#include <sys/types.h>

#include <osiSock.h>
#include <epicsThread.h>

#define epicsExportSharedSymbols
#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/logger.h>
#include <pv/codec.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

BlockingTCPConnector::BlockingTCPConnector(
    Context::shared_pointer const & context,
    int receiveBufferSize,
    float heartbeatInterval) :
    _context(context),
    _receiveBufferSize(receiveBufferSize),
    _heartbeatInterval(heartbeatInterval)
{
}

SOCKET BlockingTCPConnector::tryConnect(osiSockAddr& address, int tries) {

    char strBuffer[24];
    ipAddrToDottedIP(&address.ia, strBuffer, sizeof(strBuffer));

    for(int tryCount = 0; tryCount<tries; tryCount++) {

        LOG(logLevelDebug,
            "Opening socket to PVA server %s, attempt %d.",
            strBuffer, tryCount+1);

        SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET)
        {
            epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
            std::ostringstream temp;
            temp<<"Socket create error: "<<strBuffer;
            THROW_EXCEPTION2(std::runtime_error, temp.str());
        }
        else {
            // TODO: use non-blocking connect() to have controllable timeout
            if(::connect(socket, &address.sa, sizeof(sockaddr))==0) {
                return socket;
            }
            else {
                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                char saddr[32];
                sockAddrToDottedIP(&address.sa, saddr, sizeof(saddr));
                epicsSocketDestroy (socket);
                std::ostringstream temp;
                temp<<"error connecting to "<<saddr<<" : "<<strBuffer;
                throw std::runtime_error(temp.str());
            }
        }
    }
    return INVALID_SOCKET;
}

Transport::shared_pointer BlockingTCPConnector::connect(std::tr1::shared_ptr<ClientChannelImpl> const & client,
        ResponseHandler::shared_pointer const & responseHandler, osiSockAddr& address,
        int8 transportRevision, int16 priority) {

    SOCKET socket = INVALID_SOCKET;

    char ipAddrStr[24];
    ipAddrToDottedIP(&address.ia, ipAddrStr, sizeof(ipAddrStr));

    Context::shared_pointer context = _context.lock();

    TransportRegistry::Reservation rsvp(context->getTransportRegistry(),
                                        address, priority);
    // we are now blocking any connect() to this destination (address and prio)
    // concurrent connect() to other destination is allowed.
    // This prevents us from opening duplicate connections.

    Transport::shared_pointer transport = context->getTransportRegistry()->get(address, priority);
    if(transport.get()) {
        LOG(logLevelDebug,
            "Reusing existing connection to PVA server: %s.",
            ipAddrStr);
        if (transport->acquire(client))
            return transport;
    }

    try {
        LOG(logLevelDebug, "Connecting to PVA server: %s.", ipAddrStr);

        socket = tryConnect(address, 3);

        LOG(logLevelDebug, "Socket connected to PVA server: %s.", ipAddrStr);

        // enable TCP_NODELAY (disable Nagle's algorithm)
        int optval = 1; // true
        int retval = ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                                  (char *)&optval, sizeof(int));
        if(retval<0) {
            char errStr[64];
            epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
            LOG(logLevelWarn, "Error setting TCP_NODELAY: %s.", errStr);
        }

        // enable TCP_KEEPALIVE
        retval = ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
                              (char *)&optval, sizeof(int));
        if(retval<0)
        {
            char errStr[64];
            epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
            LOG(logLevelWarn, "Error setting SO_KEEPALIVE: %s.", errStr);
        }

        // TODO tune buffer sizes?! Win32 defaults are 8k, which is OK

        // create transport
        // TODO introduce factory
        // get TCP send buffer size
        osiSocklen_t intLen = sizeof(int);
        int _socketSendBufferSize;
        retval = getsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *)&_socketSendBufferSize, &intLen);
        if(retval<0) {
            char strBuffer[64];
            epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
            LOG(logLevelDebug, "Error getting SO_SNDBUF: %s.", strBuffer);
        }

        // create() also adds to context connection pool _context->getTransportRegistry()
        transport = detail::BlockingClientTCPTransportCodec::create(
                    context, socket, responseHandler, _receiveBufferSize, _socketSendBufferSize,
                    client, transportRevision, _heartbeatInterval, priority);

        // verify
        if(!transport->verify(5000)) {
            LOG(
                        logLevelDebug,
                        "Connection to PVA server %s failed to be validated, closing it.",
                        ipAddrStr);

            std::ostringstream temp;
            temp<<"Failed to verify TCP connection to '"<<ipAddrStr<<"'.";
            THROW_BASE_EXCEPTION(temp.str().c_str());
        }

        LOG(logLevelDebug, "Connected to PVA server: %s.", ipAddrStr);

        return transport;
    } catch(std::exception&) {
        if(transport.get())
            transport->close();
        else if(socket!=INVALID_SOCKET)
            epicsSocketDestroy(socket);
        throw;
    }
}

}
}
