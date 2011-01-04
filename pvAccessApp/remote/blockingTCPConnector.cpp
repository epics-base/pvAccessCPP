/*
 * blockingTCPConnector.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

#include "blockingTCP.h"
#include "remote.h"

#include <epicsThread.h>
#include <osiSock.h>
#include <errlog.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sstream>

namespace epics {
    namespace pvAccess {

        BlockingTCPConnector::BlockingTCPConnector(Context* context,
                int receiveBufferSize, float beaconInterval) :
            _context(context), _receiveBufferSize(receiveBufferSize),
                    _beaconInterval(beaconInterval)
        //TODO , _namedLocker(new NamedLockPattern())
        {
        }

        BlockingTCPConnector::~BlockingTCPConnector() {
            // TODO delete _namedLocker;
        }

        SOCKET BlockingTCPConnector::tryConnect(osiSockAddr* address, int tries) {
            for(int tryCount = 0; tryCount<tries; tryCount++) {
                // sleep for a while
                if(tryCount>0) epicsThreadSleep(0.1);

                char strBuffer[64];
                ipAddrToA(&address->ia, strBuffer, sizeof(strBuffer));

                errlogSevPrintf(errlogInfo,
                        "Opening socket to CA server %s, attempt %d.",
                        strBuffer, tryCount+1);

                SOCKET socket = epicsSocketCreate(AF_INET, SOCK_STREAM,
                        IPPROTO_TCP);
                if(socket==INVALID_SOCKET) {
                    epicsSocketConvertErrnoToString(strBuffer,
                            sizeof(strBuffer));
                    errlogSevPrintf(errlogMinor, "Socket create error: %s",
                            strBuffer);
                }
                else {
                    if(::connect(socket, &address->sa, sizeof(sockaddr))==0)
                        return socket;
                    else {
                        epicsSocketConvertErrnoToString(strBuffer,
                                sizeof(strBuffer));
                        errlogSevPrintf(errlogMinor,
                                "Socket connect error: %s", strBuffer);
                    }
                }
            }
            return INVALID_SOCKET;
        }

        Transport* BlockingTCPConnector::connect(TransportClient* client,
                ResponseHandler* responseHandler, osiSockAddr* address,
                short transportRevision, int16 priority) {

            SOCKET socket = INVALID_SOCKET;

            char ipAddrStr[64];
            ipAddrToA(&address->ia, ipAddrStr, sizeof(ipAddrStr));

            // first try to check cache w/o named lock...
            BlockingClientTCPTransport
                    * transport =
                            (BlockingClientTCPTransport*)(_context->getTransportRegistry()->get(
                                    "TCP", address, priority));
            if(transport!=NULL) {
                errlogSevPrintf(errlogInfo,
                        "Reusing existing connection to CA server: %s",
                        ipAddrStr);
                if(transport->acquire(client)) return transport;
            }

            bool lockAcquired = true;
            // TODO comment out
            //bool lockAcquired = _namedLocker->acquireSynchronizationObject(
            //        address, LOCK_TIMEOUT);
            if(lockAcquired) {
                try {
                    // ... transport created during waiting in lock
                    transport
                            = (BlockingClientTCPTransport*)(_context->getTransportRegistry()->get(
                                    "TCP", address, priority));
                    if(transport!=NULL) {
                        errlogSevPrintf(errlogInfo,
                                "Reusing existing connection to CA server: %s",
                                ipAddrStr);
                        if(transport->acquire(client)) return transport;
                    }

                    errlogSevPrintf(errlogInfo, "Connecting to CA server: %s",
                            ipAddrStr);

                    socket = tryConnect(address, 3);

                    // use blocking channel
                    // socket is blocking bya default
                    //socket.configureBlocking(true);

                    // enable TCP_NODELAY (disable Nagle's algorithm)
                    int optval = 1; // true
                    int retval = ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY,
                            &optval, sizeof(optval));
                    if(retval<0) errlogSevPrintf(errlogMajor,
                            "Error setting TCP_NODELAY: %s", strerror(errno));

                    // enable TCP_KEEPALIVE
                    retval = ::setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE,
                            &optval, sizeof(optval));
                    if(retval<0) errlogSevPrintf(errlogMinor,
                            "Error setting SO_KEEPALIVE: %s", strerror(errno));

                    // TODO tune buffer sizes?! Win32 defaults are 8k, which is OK
                    //socket.socket().setReceiveBufferSize();
                    //socket.socket().setSendBufferSize();

                    // create transport
                    transport = new BlockingClientTCPTransport(_context, socket,
                            responseHandler, _receiveBufferSize, client,
                            transportRevision, _beaconInterval, priority);

                    // verify
                    if(!transport->waitUntilVerified(3.0)) {
                        transport->close(true);
                        errlogSevPrintf(
                                errlogInfo,
                                "Connection to CA client %s failed to be validated, closing it.",
                                ipAddrStr);
                        ostringstream temp;
                        temp<<"Failed to verify TCP connection to '"<<ipAddrStr
                                <<"'.";
                        THROW_BASE_EXCEPTION(temp.str().c_str());
                    }

                    // TODO send security token

                    errlogSevPrintf(errlogInfo, "Connected to CA server: %s",
                            ipAddrStr);

                    return transport;
                } catch(...) {
                    // close socket, if open
                    if(socket!=INVALID_SOCKET) epicsSocketDestroy(socket);

                    // TODO namedLocker.releaseSynchronizationObject(address);

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
