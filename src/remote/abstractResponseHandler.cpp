/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <osiSock.h>

#include <pv/byteBuffer.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include <pv/remote.h>
#include <pv/hexDump.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

size_t ResponseHandler::num_instances;

ResponseHandler::ResponseHandler(Context* context, const std::string& description)
    :_description(description)
    ,_debugLevel(context->getConfiguration()->getPropertyAsInteger(PVACCESS_DEBUG, 0)) // actually $EPICS_PVA_DEBUG
{
    REFTRACE_INCREMENT(num_instances);
}

ResponseHandler::~ResponseHandler()
{
    REFTRACE_DECREMENT(num_instances);
}

void ResponseHandler::handleResponse(osiSockAddr* responseFrom,
        Transport::shared_pointer const & transport, int8 version, int8 command,
        size_t payloadSize, ByteBuffer* payloadBuffer) {
    if(_debugLevel >= 3) {   // TODO make a constant of sth (0 - off, 1 - debug, 2 - more/trace, 3 - messages)
        char ipAddrStr[24];
        ipAddrToDottedIP(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

        std::cerr<<"Message [0x"<<std::hex<<(int)command<<", v0x"<<std::hex
                 <<int(version)<<"] received from "<<ipAddrStr<<" on "<<transport->getRemoteName()
                 <<" : "<<_description<<"\n"
                 <<HexDump(*payloadBuffer, payloadSize).limit(0xffff);
    }
}
}
}
