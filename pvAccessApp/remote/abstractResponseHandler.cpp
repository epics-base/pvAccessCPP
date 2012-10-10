/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/remote.h>
#include <pv/hexDump.h>

#include <pv/byteBuffer.h>

#include <osiSock.h>

#include <sstream>

using std::ostringstream;
using std::hex;

using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        void AbstractResponseHandler::handleResponse(osiSockAddr* responseFrom,
                Transport::shared_pointer const & /*transport*/, int8 version, int8 command,
                size_t payloadSize, ByteBuffer* payloadBuffer) {
            if(_debug) {
                char ipAddrStr[48];
                ipAddrToDottedIP(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

                ostringstream prologue;
                prologue<<"Message [0x"<<hex<<(int)command<<", v0x"<<hex;
                prologue<<(int)version<<"] received from "<<ipAddrStr;

                hexDump(prologue.str(), _description,
                        (const int8*)payloadBuffer->getArray(),
                        payloadBuffer->getPosition(), payloadSize);
            }
        }
    }
}
