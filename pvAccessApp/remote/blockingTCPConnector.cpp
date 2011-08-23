/*
 * blockingTCPConnector.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/namedLockPattern.h>

#include <epicsThread.h>
#include <osiSock.h>
#include <errlog.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>

namespace epics {
    namespace pvAccess {

        BlockingTCPConnector::BlockingTCPConnector(
                Context::shared_pointer const & context,
                int receiveBufferSize,
                float beaconInterval) :
            _context(context),
            _namedLocker(),
            _receiveBufferSize(receiveBufferSize),
            _beaconInterval(beaconInterval)
        {
        }

        BlockingTCPConnector::~BlockingTCPConnector() {
        }

        SOCKET BlockingTCPConnector::tryConnect(osiSockAddr& address, int tries) {
            
            char strBuffer[64];
            ipAddrToDottedIP(&address.ia, strBuffer, sizeof(strBuffer));

            for(int tryCount = 0; tryCount<tries; tryCount++) {

                errlogSevPrintf(errlogInfo,
                        "Opening socket to CA server %s, attempt %d.",
                        strBuffer, tryCount+1);

                SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                if (socket == INVALID_SOCKET)
                {
                    epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                    errlogSevPrintf(errlogMinor, "Socket create error: %s", strBuffer);
                    return INVALID_SOCKET;
                }
                else {
                    if(::connect(socket, &address.sa, sizeof(sockaddr))==0) {
                        return socket;
                    }
                    else {
                        epicsSocketDestroy (socket);
                        epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
                        errlogSevPrintf(errlogMinor, "Socket connect error: %s", strBuffer);
                    }
                }
            }
            return INVALID_SOCKET;
        }

        Transport::shared_pointer BlockingTCPConnector::connect(TransportClient::shared_pointer const & client,
                auto_ptr<ResponseHandler>& responseHandler, osiSockAddr& address,
                short transportRevision, int16 priority) {

            SOCKET socket = INVALID_SOCKET;

            char ipAddrStr[64];
            ipAddrToDottedIP(&address.ia, ipAddrStr, sizeof(ipAddrStr));
            
            Context::shared_pointer context = _context.lock();

            // first try to check cache w/o named lock...
            Transport::shared_pointer tt = context->getTransportRegistry()->get("TCP", &address, priority);
            BlockingClientTCPTransport::shared_pointer transport = std::tr1::static_pointer_cast<BlockingClientTCPTransport>(tt);
            if(transport.get()) {
                errlogSevPrintf(errlogInfo,
                        "Reusing existing connection to CA server: %s",
                        ipAddrStr);
                if (transport->acquire(client))
                    return transport;
            }

            bool lockAcquired = _namedLocker.acquireSynchronizationObject(&address, LOCK_TIMEOUT);
            if(lockAcquired) {
                try {
                    // ... transport created during waiting in lock
                    tt = context->getTransportRegistry()->get("TCP", &address, priority);
                    transport = std::tr1::static_pointer_cast<BlockingClientTCPTransport>(tt);
                    if(transport.get()) {
                        errlogSevPrintf(errlogInfo,
                                        "Reusing existing connection to CA server: %s",
                                        ipAddrStr);
                        if (transport->acquire(client))
                            return transport;
                    }
                    
                    errlogSevPrintf(errlogInfo, "Connecting to CA server: %s", ipAddrStr);

                    socket = tryConnect(address, 3);
                    
                    // verify
                    if(socket==INVALID_SOCKET) {
                        errlogSevPrintf(errlogMajor,
                                "Connection to CA server %s failed.", ipAddrStr);
                        ostringstream temp;
                        temp<<"Failed to verify TCP connection to '"<<ipAddrStr<<"'.";
                        THROW_BASE_EXCEPTION(temp.str().c_str());
                    }

                    errlogSevPrintf(errlogInfo, "Socket connected to CA server: %s.", ipAddrStr);

                    // enable TCP_NODELAY (disable Nagle's algorithm)
                    int optval = 1; // true
                    int retval = ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                            &optval, sizeof(int));
                    if(retval<0) {
                        char errStr[64];
                        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                        errlogSevPrintf(errlogMajor, "Error setting TCP_NODELAY: %s", errStr);
                    }
                    
                    // enable TCP_KEEPALIVE
                    retval = ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
                            &optval, sizeof(int));
                    if(retval<0) 
                    {
                        char errStr[64];
                        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                        errlogSevPrintf(errlogMinor, "Error setting SO_KEEPALIVE: %s", errStr);
                    }
                    
                    // TODO tune buffer sizes?! Win32 defaults are 8k, which is OK

                    // create transport
                    transport = BlockingClientTCPTransport::create(
                                            context, socket, responseHandler, _receiveBufferSize,
                                            client, transportRevision, _beaconInterval, priority);

                    // verify
                    if(!transport->waitUntilVerified(3.0)) {
                        errlogSevPrintf(
                                errlogMinor,
                                "Connection to CA server %s failed to be validated, closing it.",
                                ipAddrStr);
                        ostringstream temp;
                        temp<<"Failed to verify TCP connection to '"<<ipAddrStr<<"'.";
                        THROW_BASE_EXCEPTION(temp.str().c_str());
                    }

                    // TODO send security token

                    errlogSevPrintf(errlogInfo, "Connected to CA server: %s", ipAddrStr);

                    _namedLocker.releaseSynchronizationObject(&address);
                    return transport;
                } catch(std::exception& ex) {
                    // TODO
                    printf("ex %s\n", ex.what());
                    if(transport.get())
                        transport->close(true);
                    else if(socket!=INVALID_SOCKET) epicsSocketDestroy(socket);
                    _namedLocker.releaseSynchronizationObject(&address);
                    throw;
                } catch(...) {
                    if(transport.get())
                        transport->close(true);
                    else if(socket!=INVALID_SOCKET) epicsSocketDestroy(socket);
                    _namedLocker.releaseSynchronizationObject(&address);
                    throw;
                }
            }
            else {
                ostringstream temp;
                temp<<"Failed to obtain synchronization lock for '"<<ipAddrStr;
                temp<<"', possible deadlock.";
                THROW_BASE_EXCEPTION(temp.str().c_str());
            }
        }

    }
}
