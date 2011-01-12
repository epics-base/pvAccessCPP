/*
 * responseHandlers.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

#include "responseHandlers.h"
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

        void BadResponse::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, ByteBuffer* payloadBuffer) {
            AbstractServerResponseHandler::handleResponse(responseFrom,
                    transport, version, command, payloadSize, payloadBuffer);

            char ipAddrStr[48];
            ipAddrToDottedIP(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

            errlogSevPrintf(errlogInfo,
                    "Undecipherable message (bad response type %d) from %s.",
                    command, ipAddrStr);

        }

        ServerResponseHandler::ServerResponseHandler(ServerContextImpl* context) {

            BadResponse* badResponse = new BadResponse(context);

            _handlerTable = new ResponseHandler*[HANDLER_TABLE_LENGTH];
            // TODO add real handlers, as they are developed
            _handlerTable[0] = new NoopResponse(context, "Beacon");
            _handlerTable[1] = new ConnectionValidationHandler(context);
            _handlerTable[2] = new EchoHandler(context);
            _handlerTable[3] = badResponse;
            _handlerTable[4] = badResponse;
            _handlerTable[5] = badResponse;
            _handlerTable[6] = badResponse;
            _handlerTable[7] = badResponse;
            _handlerTable[8] = badResponse;
            _handlerTable[9] = badResponse;
            _handlerTable[10] = badResponse;
            _handlerTable[11] = badResponse;
            _handlerTable[12] = badResponse;
            _handlerTable[13] = badResponse;
            _handlerTable[14] = badResponse;
            _handlerTable[15] = badResponse;
            _handlerTable[16] = badResponse;
            _handlerTable[17] = badResponse;
            _handlerTable[18] = badResponse;
            _handlerTable[19] = badResponse;
            _handlerTable[20] = badResponse;
            _handlerTable[21] = badResponse;
            _handlerTable[22] = badResponse;
            _handlerTable[23] = badResponse;
            _handlerTable[24] = badResponse;
            _handlerTable[25] = badResponse;
            _handlerTable[26] = badResponse;
            _handlerTable[27] = badResponse;
        }

        ServerResponseHandler::~ServerResponseHandler() {
            delete _handlerTable[0];
            delete _handlerTable[1];
            delete _handlerTable[2];
            delete _handlerTable[27];
            delete[] _handlerTable;
        }

        void ServerResponseHandler::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, ByteBuffer* payloadBuffer) {
            if(command<0||command>=HANDLER_TABLE_LENGTH) {
                errlogSevPrintf(errlogMinor,
                        "Invalid (or unsupported) command: %x.", (0xFF&command));
                // TODO remove debug output
                ostringstream name;
                name<<"Invalid CA header "<<hex<<(int)(0xFF&command);
                name<<", its payload buffer";

                hexDump(name.str(), (const int8*)payloadBuffer->getArray(),
                        payloadBuffer->getPosition(), payloadSize);
                return;
            }

            // delegate
            _handlerTable[command]->handleResponse(responseFrom, transport,
                    version, command, payloadSize, payloadBuffer);
        }

        void ConnectionValidationHandler::handleResponse(
                osiSockAddr* responseFrom, Transport* transport, int8 version,
                int8 command, int payloadSize,
                epics::pvData::ByteBuffer* payloadBuffer) {
            AbstractServerResponseHandler::handleResponse(responseFrom,
                    transport, version, command, payloadSize, payloadBuffer);

            transport->ensureData(2*sizeof(int32)+sizeof(int16));
            transport->setRemoteTransportReceiveBufferSize(
                    payloadBuffer->getInt());
            transport->setRemoteTransportSocketReceiveBufferSize(
                    payloadBuffer->getInt());
            transport->setRemoteMinorRevision(version);
            // TODO support priority  !!!
            //transport.setPriority(payloadBuffer.getShort());
        }

        class EchoTransportSender : public TransportSender {
        public:
            EchoTransportSender(osiSockAddr* echoFrom) {
                memcpy(&_echoFrom, echoFrom, sizeof(osiSockAddr));
            }

            virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
                control->startMessage(CMD_ECHO, 0);
                control->setRecipient(_echoFrom);
            }

            virtual void lock() {
            }

            virtual void unlock() {
                delete this;
            }
        private:
            osiSockAddr _echoFrom;

            virtual ~EchoTransportSender() {
            }
        };

        void EchoHandler::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, epics::pvData::ByteBuffer* payloadBuffer) {
            AbstractServerResponseHandler::handleResponse(responseFrom,
                    transport, version, command, payloadSize, payloadBuffer);

            EchoTransportSender* echoReply = new EchoTransportSender(
                    responseFrom);

            // send back
            transport->enqueueSendRequest(echoReply);
        }

    }
}
