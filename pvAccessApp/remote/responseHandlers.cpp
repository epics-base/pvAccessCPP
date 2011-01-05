/*
 * responseHandlers.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

#include "remote.h"
#include "hexDump.h"

#include <byteBuffer.h>

#include <osiSock.h>
#include <errlog.h>

#include <sstream>

using std::ostringstream;
using std::hex;

using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

        void AbstractResponseHandler::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, ByteBuffer* payloadBuffer) {
            if(_debug) {
                char ipAddrStr[48];
                ipAddrToA(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

                ostringstream prologue;
                prologue<<"Message ["<<command<<", v"<<hex<<version;
                prologue<<"] received from "<<ipAddrStr;

                hexDump(prologue.str(), _description,
                        (const int8*)payloadBuffer->getArray(),
                        payloadBuffer->getPosition(), payloadSize);
            }
        }

        void BadResponse::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, ByteBuffer* payloadBuffer) {
            AbstractServerResponseHandler::handleResponse(responseFrom,
                    transport, version, command, payloadSize, payloadBuffer);

            char ipAddrStr[48];
            ipAddrToA(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

            errlogSevPrintf(errlogInfo,
                    "Undecipherable message (bad response type %d) from %s.",
                    command, ipAddrStr);

        }

        ServerResponseHandler::ServerResponseHandler(ServerContextImpl* context) :
            _context(context) {

            BadResponse* badResponse = new BadResponse(context);

            _handlerTable = new ResponseHandler*[HANDLER_TABLE_LENGTH];
            _handlerTable[0] = badResponse;
        }

        ServerResponseHandler::~ServerResponseHandler() {
            delete[] _handlerTable;
        }

        void ServerResponseHandler::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, ByteBuffer* payloadBuffer) {
            if(command<0||command>=HANDLER_TABLE_LENGTH) {
                errlogSevPrintf(errlogMinor,
                        "Invalid (or unsupported) command: %d.", command);
                // TODO remove debug output
                ostringstream name;
                name<<"Invalid CA header "<<command;
                name<<" + , its payload buffer";

                hexDump(name.str(), (const int8*)payloadBuffer->getArray(),
                        payloadBuffer->getPosition(), payloadSize);
                return;
            }

            // delegate
            _handlerTable[command]->handleResponse(responseFrom, transport,
                    version, command, payloadSize, payloadBuffer);
        }

    }
}
