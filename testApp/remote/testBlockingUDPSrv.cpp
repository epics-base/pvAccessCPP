/*
 * blockingUDPTest.cpp
 *
 *  Created on: Dec 28, 2010
 *      Author: Miha Vitorovic
 */

#include <pv/remote.h>
#include <pv/blockingUDP.h>
#include <pv/logger.h>
#include <pv/hexDump.h>

#include <osiSock.h>
#include <epicsThread.h>

#include <iostream>
#include <sstream>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;
using std::tr1::static_pointer_cast;

class ContextImpl : public Context {
public:
    ContextImpl() {}

    virtual ~ContextImpl() {
    }
    virtual Timer::shared_pointer getTimer() {
        return Timer::shared_pointer();
    }
    virtual std::tr1::shared_ptr<TransportRegistry> getTransportRegistry() {
        return std::tr1::shared_ptr<TransportRegistry>();
    }
    virtual std::tr1::shared_ptr<Channel> getChannel(epics::pvAccess::pvAccessID) {
        return std::tr1::shared_ptr<Channel>();
    }
    virtual Transport::shared_pointer getSearchTransport() {
        return Transport::shared_pointer();
    }
    virtual Configuration::shared_pointer getConfiguration() {
        return Configuration::shared_pointer();
    }
    virtual void acquire() {}
    virtual void release() {}
    virtual void newServerDetected() {}
};

class DummyResponseHandler : public ResponseHandler {
public:
    DummyResponseHandler(Context* /*context*/)
       : packets(0) {
    }
    
    virtual ~DummyResponseHandler() {}

    int getPackets() {
        return packets;
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
    		Transport::shared_pointer const &, int8 version, int8 command, std::size_t payloadSize,
            ByteBuffer* payloadBuffer);
private:
    int packets;
};

void DummyResponseHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const &, int8 version, int8 command, std::size_t payloadSize,
        ByteBuffer* payloadBuffer) {
    std::ostringstream os;

    cout<<"Received new UDP datagram["<<packets+1<<"]..."<<endl;

    char ipAddressStr[24];

    ipAddrToDottedIP(&responseFrom->ia, ipAddressStr, sizeof(ipAddressStr));

    cout<<"From: "<<ipAddressStr<<endl;
    cout<<"Version: 0x"<<hex<<(int)version<<endl;
    cout<<"Command: 0x"<<hex<<(int)command<<endl;
    cout<<"Payload size: "<<dec<<payloadSize<<endl;

    char payload[50];

    for(std::size_t i = 0; i<payloadSize;) {
        int dataCount = payloadSize-i<50 ? payloadSize-i : 50;
        payloadBuffer->get(payload, 0, dataCount);
        os<<"Payload ("<<i<<"-"<<(dataCount-1)<<")";
        hexDump(os.str(), (int8*)payload, dataCount);
        i += dataCount;
    }

    cout<<endl<<endl;

    packets++;
}

void testBlockingUDPConnector() {
    BlockingUDPConnector connector(false, true);
    ContextImpl ctx;

    DummyResponseHandler* drh = new DummyResponseHandler(&ctx);
    auto_ptr<ResponseHandler> rh(static_cast<ResponseHandler*>(drh));

    osiSockAddr bindAddr;

    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(65000);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    TransportClient::shared_pointer nullPointer;
    Transport::shared_pointer transport(connector.connect(nullPointer,rh, bindAddr, 1, 50));

    static_pointer_cast<BlockingUDPTransport>(transport)->start();

    cout<<"Waiting for 10 packets..."<<endl;

    //TODO drh can be deleted in connector!
    while(drh->getPackets()<10) {
        epicsThreadSleep(1.0);
    }
}

int main() {
//    createFileLogger("testBlockingUDPSrv.log");

    testBlockingUDPConnector();
    return (0);
}
