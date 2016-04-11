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
#include <pv/namedLockPattern.h>
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
    _namedLocker(),
    _receiveBufferSize(receiveBufferSize),
    _heartbeatInterval(heartbeatInterval)
{
}

BlockingTCPConnector::~BlockingTCPConnector() {
}

SOCKET BlockingTCPConnector::tryConnect(osiSockAddr& address, int tries) {

    char strBuffer[64];
    ipAddrToDottedIP(&address.ia, strBuffer, sizeof(strBuffer));

    for(int tryCount = 0; tryCount<tries; tryCount++) {

        LOG(logLevelDebug,
            "Opening socket to PVA server %s, attempt %d.",
            strBuffer, tryCount+1);

        SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET)
        {
            epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
            LOG(logLevelWarn, "Socket create error: %s.", strBuffer);
            return INVALID_SOCKET;
        }
        else {
            if(::connect(socket, &address.sa, sizeof(sockaddr))==0) {
                return socket;
            }
            else {
                epicsSocketDestroy (socket);
                epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                LOG(logLevelDebug, "Socket connect error: %s.", strBuffer);
            }
        }
    }
    return INVALID_SOCKET;
}

Transport::shared_pointer BlockingTCPConnector::connect(TransportClient::shared_pointer const & client,
        ResponseHandler::shared_pointer const & responseHandler, osiSockAddr& address,
        int8 transportRevision, int16 priority) {

    SOCKET socket = INVALID_SOCKET;

    char ipAddrStr[64];
    ipAddrToDottedIP(&address.ia, ipAddrStr, sizeof(ipAddrStr));

    Context::shared_pointer context = _context.lock();

    // first try to check cache w/o named lock...
    Transport::shared_pointer transport = context->getTransportRegistry()->get("TCP", &address, priority);
    if(transport.get()) {
        LOG(logLevelDebug,
            "Reusing existing connection to PVA server: %s.",
            ipAddrStr);
        if (transport->acquire(client))
            return transport;
    }

    bool lockAcquired = _namedLocker.acquireSynchronizationObject(&address, LOCK_TIMEOUT);
    if(lockAcquired) {
        try {
            // ... transport created during waiting in lock
            transport = context->getTransportRegistry()->get("TCP", &address, priority);
            if(transport.get()) {
                LOG(logLevelDebug,
                    "Reusing existing connection to PVA server: %s.",
                    ipAddrStr);
                if (transport->acquire(client))
                    return transport;
            }

            LOG(logLevelDebug, "Connecting to PVA server: %s.", ipAddrStr);

            socket = tryConnect(address, 3);

            // verify
            if(socket==INVALID_SOCKET) {
                LOG(logLevelDebug,
                    "Connection to PVA server %s failed.", ipAddrStr);
                std::ostringstream temp;
                temp<<"Failed to verify TCP connection to '"<<ipAddrStr<<"'.";
                THROW_BASE_EXCEPTION(temp.str().c_str());
            }

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

            _namedLocker.releaseSynchronizationObject(&address);
            return transport;
        } catch(std::exception&) {
            if(transport.get())
                transport->close();
            else if(socket!=INVALID_SOCKET) epicsSocketDestroy(socket);
            _namedLocker.releaseSynchronizationObject(&address);
            throw;
        } catch(...) {
            if(transport.get())
                transport->close();
            else if(socket!=INVALID_SOCKET) epicsSocketDestroy(socket);
            _namedLocker.releaseSynchronizationObject(&address);
            throw;
        }
    }
    else {
        std::ostringstream temp;
        temp<<"Failed to obtain synchronization lock for '"<<ipAddrStr;
        temp<<"', possible deadlock.";
        THROW_BASE_EXCEPTION(temp.str().c_str());
    }
}

}
}
