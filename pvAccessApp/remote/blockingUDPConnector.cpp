/*
 * blockingUDPConnector.cpp
 *
 *  Created on: Dec 27, 2010
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingUDP.h"
#include "remote.h"

/* EPICSv3 */
#include <errlog.h>
#include <osiSock.h>

/* standard */
#include <sys/types.h>
#include <sys/socket.h>

using namespace std;

namespace epics {
    namespace pvAccess {

    Transport::shared_pointer BlockingUDPConnector::connect(TransportClient::shared_pointer const & client,
                auto_ptr<ResponseHandler>& responseHandler, osiSockAddr& bindAddress,
                short transportRevision, int16 priority) {
                    
            errlogSevPrintf(errlogInfo, "Creating datagram socket to: %s",
                    inetAddressToString(bindAddress).c_str());

            SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if(socket==INVALID_SOCKET) {
                char errStr[64];
                epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
                errlogSevPrintf(errlogMajor, "Error creating socket: %s", errStr);
                return Transport::shared_pointer();
            }

            int optval = _broadcast ? 1 : 0;
            int retval = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &optval, sizeof(optval));
            if(retval<0)
            {
                errlogSevPrintf(errlogMajor, "Error setting SO_BROADCAST: %s", strerror(errno));
                epicsSocketDestroy (socket);
                return Transport::shared_pointer();
            }
            
            // set SO_REUSEADDR or SO_REUSEPORT, OS dependant
            if (_reuseSocket)
                epicsSocketEnableAddressUseForDatagramFanout(socket);

            retval = ::bind(socket, (sockaddr*)&(bindAddress.sa), sizeof(sockaddr));
            if(retval<0) {
                errlogSevPrintf(errlogMajor, "Error binding socket: %s", strerror(errno));
                epicsSocketDestroy (socket);
                return Transport::shared_pointer();
            }

            // sockets are blocking by default
            Transport::shared_pointer transport = BlockingUDPTransport::create(responseHandler, socket, bindAddress, transportRevision);
            return transport;
        }

    }
}
