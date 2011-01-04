/*
 * testBeaconEmitter.cpp
 */

#include "remote.h"
#include "blockingUDP.h"
#include "beaconHandler.h"
#include "inetAddressUtil.h"
#include "introspectionRegistry.h"

#include <osiSock.h>

#include <iostream>
#include <cstdio>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

void decodeFromIPv6Address(ByteBuffer* buffer, osiSockAddr* address)
{
    // IPv4 compatible IPv6 address
    // first 80-bit are 0
    buffer->getLong();
    buffer->getShort();
    // next 16-bits are 1
    buffer->getShort();
    // following IPv4 address in big-endian (network) byte order
    in_addr_t ipv4Addr = 0;
    ipv4Addr |= (uint32)buffer->getByte() << 24;
    ipv4Addr |= (uint32)buffer->getByte() << 16;
    ipv4Addr |= (uint32)buffer->getByte() << 8;
    ipv4Addr |= (uint32)buffer->getByte() << 0;
    address->ia.sin_addr.s_addr = ipv4Addr;
}

class BeaconResponseHandler : public ResponseHandler
{
public:
	BeaconResponseHandler()
	{
		_pvDataCreate = getPVDataCreate();
	}

    virtual void handleResponse(osiSockAddr* responseFrom,
            Transport* transport, int8 version, int8 command, int payloadSize,
            ByteBuffer* payloadBuffer)
    {
    	cout << "BeaconResponseHandler::handleResponse" << endl;

    	// reception timestamp
    	TimeStamp timestamp;
    	timestamp.getCurrent();

    	//TODO
    	//super.handleResponse(responseFrom, transport, version, command, payloadSize, payloadBuffer);

    	transport->ensureData((2*sizeof(int16)+2*sizeof(int32)+128)/sizeof(int8));

    	const int32 sequentalID = payloadBuffer->getShort() & 0x0000FFFF;
    	const TimeStamp startupTimestamp(payloadBuffer->getInt() & 0x00000000FFFFFFFFL,(int32)(payloadBuffer->getInt() & 0x00000000FFFFFFFFL));

    	// 128-bit IPv6 address
    	osiSockAddr address;
      	decodeFromIPv6Address(payloadBuffer, &address);

      	// get port
      	const int32 port = payloadBuffer->getShort() & 0xFFFF;
    	address.ia.sin_port = ntohs(port);

    	// accept given address if explicitly specified by sender
    	if (!ipv4AddressToInt(address))
    	{
    		responseFrom->ia.sin_port = port;
    	}
    	else
    	{
    		responseFrom->ia.sin_port = port;
    		responseFrom->ia.sin_addr.s_addr = address.ia.sin_addr.s_addr;
    	}

    	//org.epics.ca.client.impl.remote.BeaconHandler beaconHandler = context.getBeaconHandler(responseFrom);
    	// currently we care only for servers used by this context
    	//if (beaconHandler == null)
    	//	return;

    	// extra data
    	PVFieldPtr data = NULL;
    	const FieldConstPtr field = IntrospectionRegistry::deserializeFull(payloadBuffer, transport);
    	if (field != NULL)
    	{
    		data = _pvDataCreate->createPVField(NULL, field);
    		data->deserialize(payloadBuffer, transport);
    	}

    	// notify beacon handler
    	//beaconHandler.beaconNotify(responseFrom, version, timestamp, startupTimestamp, sequentalID, data);
    }

private:
    PVDataCreate* _pvDataCreate;
    BeaconHandler* _beaconHandler;
};


void testBeaconHandler()
{
	BeaconResponseHandler brh;
    BlockingUDPConnector connector(false, NULL, true);

    osiSockAddr bindAddr;
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(5067);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    Transport* transport = connector.connect(NULL, &brh, &bindAddr, 1, 50);
    (static_cast<BlockingUDPTransport*>(transport))->start();

    while(1) sleep(1);

    delete transport;
}

int main(int argc, char *argv[])
{
	testBeaconHandler();
    return (0);
}
