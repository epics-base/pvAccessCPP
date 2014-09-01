/*
 * testBeaconEmitter.cpp
 */

#include <pv/remote.h>
#include <pv/blockingUDP.h>
#include <pv/beaconHandler.h>
#include <pv/inetAddressUtil.h>
#include <pv/introspectionRegistry.h>

#include <osiSock.h>

#include <iostream>
#include <cstdio>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

class BeaconResponseHandler : public ResponseHandler
{
public:
	BeaconResponseHandler(Context* ctx) : ResponseHandler()
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

    	/*const int32 sequentalID = */ payloadBuffer->getShort();
    	const TimeStamp startupTimestamp(payloadBuffer->getInt(),payloadBuffer->getInt());

    	// 128-bit IPv6 address
    	osiSockAddr address;
      	decodeFromIPv6Address(payloadBuffer, &address);

      	// get port
      	const int32 port = payloadBuffer->getShort();
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


class ContextImpl : public Context {
public:
    ContextImpl() :
        _tr(new TransportRegistry()), _timer(new Timer("server thread",
                lowPriority)), _conf(new SystemConfigurationImpl()) {
    }
    virtual ~ContextImpl() {
        delete _tr;
        delete _timer;
    }
    virtual Timer* getTimer() { return _timer; }
    virtual TransportRegistry* getTransportRegistry() { return _tr; }
    virtual Channel* getChannel(epics::pvAccess::pvAccessID) { return 0; }
    virtual Transport* getSearchTransport() { return 0; }
    virtual Configuration* getConfiguration() { return _conf; }
    virtual void acquire() {}
    virtual void release() {}

private:
    TransportRegistry* _tr;
    Timer* _timer;
    Configuration* _conf;
};


void testBeaconHandler()
{
    ContextImpl ctx;
	BeaconResponseHandler brh(&ctx);
    BlockingUDPConnector connector(false, true);

    osiSockAddr bindAddr;
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(5067);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    Transport* transport = connector.connect(NULL, &brh, bindAddr, 1, 50);
    (static_cast<BlockingUDPTransport*>(transport))->start();

    epicsThreadSleep (60.0);

    delete transport;
}

int main(int argc, char *argv[])
{
	testBeaconHandler();
    return (0);
}
