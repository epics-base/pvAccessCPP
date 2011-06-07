/*
 * testBeaconEmitter.cpp
 */

#include <pv/remote.h>
#include <pv/blockingUDP.h>
#include <pv/beaconEmitter.h>
#include <pv/inetAddressUtil.h>

#include <osiSock.h>

#include <iostream>
#include <cstdio>

using namespace epics::pvAccess;
using namespace epics::pvData;

class DummyResponseHandler : public ResponseHandler
{
public:
    DummyResponseHandler(Context* ctx) : ResponseHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
            Transport* transport, int8 version, int8 command, int payloadSize,
            ByteBuffer* payloadBuffer)
    {
    	cout << "DummyResponseHandler::handleResponse" << endl;
    }
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


void testBeaconEmitter()
{
    ContextImpl ctx;
    DummyResponseHandler drh(&ctx);

    SOCKET socket = epicsSocketCreate(AF_INET, SOCK_DGRAM, 0);
    auto_ptr<InetAddrVector> broadcastAddresses(getBroadcastAddresses(socket, 5067));
    epicsSocketDestroy (socket);

    BlockingUDPConnector connector(true, true);

    osiSockAddr bindAddr;
    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(5066);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    
    BlockingUDPTransport* transport = (BlockingUDPTransport*)connector.connect(NULL, &drh, bindAddr, 1, 50);
    transport->setBroadcastAddresses(broadcastAddresses.get());

    cout<<"Sending beacons"<<endl;
    BeaconEmitter beaconEmitter(transport, transport->getRemoteAddress());
    beaconEmitter.start();

    epicsThreadSleep (60.0);

    delete transport;
}

int main(int argc, char *argv[])
{
	testBeaconEmitter();
    return (0);
}
