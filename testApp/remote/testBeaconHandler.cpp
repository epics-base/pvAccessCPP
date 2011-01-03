/*
 * testBeaconEmitter.cpp
 */

#include "remote.h"
#include "blockingUDP.h"
#include "beaconHandler.h"
#include "inetAddressUtil.h"

#include <osiSock.h>

#include <iostream>
#include <cstdio>

using namespace epics::pvAccess;
using namespace epics::pvData;

class BeaconResponseHandler : public ResponseHandler
{
public:
    virtual void handleResponse(osiSockAddr* responseFrom,
            Transport* transport, int8 version, int8 command, int payloadSize,
            ByteBuffer* payloadBuffer)
    {
    	cout << "DummyResponseHandler::handleResponse" << endl;
    }
};


void testBeaconHandler()
{
    BeacondResponseHandler brh;
    BlockingUDPConnector connector(false, NULL, true);
    DummyClientContext context;

    osiSockAddr bindAddr;
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(5067);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    Transport* transport = connector.connect(NULL, &brh, &bindAddr, 1, 50);

    ((BlockingUDPTransport*)transport)->start();

    while(1) sleep(1);

    delete transport;
}

int main(int argc, char *argv[])
{
	testBeaconHandler();
    return (0);
}
