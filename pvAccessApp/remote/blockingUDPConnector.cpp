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

/* standard */
#include <sys/types.h>
#include <sys/socket.h>

namespace epics {
    namespace pvAccess {

        Transport* BlockingUDPConnector::connect(TransportClient* client,
                ResponseHandler* responseHandler, osiSockAddr* bindAddress,
                short transportRevision, short priority) {
            errlogSevPrintf(errlogInfo, "Creating datagram socket to: %s",
                    inetAddressToString(bindAddress).c_str());

            SOCKET socket = ::socket(PF_INET, SOCK_DGRAM, 0);

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

            int optval = 1;     // true

            retval = ::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &optval,
                    sizeof(optval));
            if(retval<0) errlogSevPrintf(errlogMajor,
                    "Error binding socket: %s", strerror(errno));

            retval = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &optval,
                    sizeof(optval));

            // sockets are blocking by default

            return new BlockingUDPTransport(responseHandler, socket,
                    bindAddress, _sendAddresses, transportRevision);
        }

    }
}
