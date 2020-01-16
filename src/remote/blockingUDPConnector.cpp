/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <sys/types.h>

#include <osiSock.h>

#define epicsExportSharedSymbols
#include <pv/blockingUDP.h>
#include <pv/remote.h>
#include <pv/logger.h>

using namespace std;
using namespace epics::pvData;

namespace {
struct closer {
    epics::pvAccess::BlockingUDPTransport::shared_pointer P;
    closer(const epics::pvAccess::BlockingUDPTransport::shared_pointer& P) :P(P) {}
    void operator()(epics::pvAccess::BlockingUDPTransport*) {
        try{
            P->close();
        }catch(...){
            P.reset();
            throw;
        }
        P.reset();
    }
};
}

namespace epics {
namespace pvAccess {

BlockingUDPTransport::shared_pointer BlockingUDPConnector::connect(ResponseHandler::shared_pointer const & responseHandler,
                                                                   osiSockAddr& bindAddress,
                                                                   int8 transportRevision)
{
    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(socket==INVALID_SOCKET) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        LOG(logLevelError, "Error creating socket: %s.", errStr);
        return BlockingUDPTransport::shared_pointer();
    }

    int optval = 1;
    int retval = ::setsockopt(socket, SOL_SOCKET, SO_BROADCAST, (char *)&optval, sizeof(optval));
    if(retval<0)
    {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        LOG(logLevelError, "Error setting SO_BROADCAST: %s.", errStr);
        epicsSocketDestroy (socket);
        return BlockingUDPTransport::shared_pointer();
    }

    /*
    IPv4 multicast addresses are defined by the leading address bits of 1110,
    originating from the classful network design of the early Internet when this
    group of addresses was designated as Class D. The Classless Inter-Domain Routing (CIDR)
    prefix of this group is 224.0.0.0/4.
    The group includes the addresses from 224.0.0.0 to 239.255.255.255.
    Address assignments from within this range are specified in RFC 5771,
    an Internet Engineering Task Force (IETF) Best Current Practice document (BCP 51).*/


    // set SO_REUSEADDR or SO_REUSEPORT, OS dependant
    epicsSocketEnableAddressUseForDatagramFanout(socket);

    retval = ::bind(socket, (sockaddr*)&(bindAddress.sa), sizeof(sockaddr));
    if(retval<0) {
        char ip[24];
        sockAddrToDottedIP((sockaddr*)&(bindAddress.sa), ip, sizeof(ip));
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        LOG(logLevelError, "Error binding socket %s: %s.", ip, errStr);
        epicsSocketDestroy (socket);
        return BlockingUDPTransport::shared_pointer();
    }

    // sockets are blocking by default
    BlockingUDPTransport::shared_pointer transport(new BlockingUDPTransport(_serverFlag, responseHandler,
            socket, bindAddress, transportRevision));
    transport->internal_this = transport;

    // the worker thread holds a strong ref, which is released by transport->close()
    BlockingUDPTransport::shared_pointer ret(transport.get(), closer(transport));

    return ret;
}

}
}
