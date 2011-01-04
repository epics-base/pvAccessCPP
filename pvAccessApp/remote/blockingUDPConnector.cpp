/*
 * blockingUDPConnector.cpp
 *
 *  Created on: Dec 27, 2010
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingUDP.h"
#include "remote.h"

/* pvData */
#include <epicsException.h>

/* EPICSv3 */
#include <errlog.h>
#include <osiSock.h>

/* standard */
#include <sys/types.h>
#include <sys/socket.h>

namespace epics {
    namespace pvAccess {

        Transport* BlockingUDPConnector::connect(TransportClient* client,
                ResponseHandler* responseHandler, osiSockAddr* bindAddress,
                short transportRevision, int16 priority) {
            errlogSevPrintf(errlogInfo, "Creating datagram socket to: %s",
                    inetAddressToString(bindAddress).c_str());

            SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if(socket==INVALID_SOCKET) {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                errlogSevPrintf(errlogMajor, "Error creating socket: %s",
                        errStr);
            }

            /* from MSDN:
             * Note:  If the setsockopt function is called before the bind
             * function, TCP/IP options will not be checked by using TCP/IP
             * until the bind occurs. In this case, the setsockopt function
             * call will always succeed, but the bind function call can fail
             * because of an early setsockopt call failing.
             */

            int retval = ::bind(socket, (sockaddr*)&(bindAddress->sa),
                    sizeof(sockaddr));
            if(retval<0) {
                errlogSevPrintf(errlogMajor, "Error binding socket: %s",
                        strerror(errno));
                THROW_BASE_EXCEPTION(strerror(errno));
            }

            // set the socket options

            int optval = _reuseSocket ? 1 : 0;
            retval = ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &optval,
                    sizeof(optval));
            if(retval<0) errlogSevPrintf(errlogMajor,
                    "Error setting SO_REUSEADDR: %s", strerror(errno));

            optval = _broadcast ? 1 : 0;
            retval = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &optval,
                    sizeof(optval));
            if(retval<0) errlogSevPrintf(errlogMajor,
                    "Error setting SO_BROADCAST: %s", strerror(errno));

            // sockets are blocking by default

            return new BlockingUDPTransport(responseHandler, socket,
                    bindAddress, _sendAddresses, transportRevision);
        }

    }
}
