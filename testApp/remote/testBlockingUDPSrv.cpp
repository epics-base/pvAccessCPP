/*
 * blockingUDPTest.cpp
 *
 *  Created on: Dec 28, 2010
 *      Author: Miha Vitorovic
 */

#include "remote.h"
#include "blockingUDP.h"
#include "logger.h"
#include "hexDump.h"

#include <osiSock.h>

#include <iostream>
#include <sstream>

using namespace epics::pvAccess;
using std::cout;
using std::endl;
using std::hex;
using std::dec;

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
    virtual Timer* getTimer() {
        return _timer;
    }
    virtual TransportRegistry* getTransportRegistry() {
        return _tr;
    }
    virtual Channel* getChannel(epics::pvAccess::pvAccessID) {
        return 0;
    }
    virtual Transport* getSearchTransport() {
        return 0;
    }
    virtual Configuration* getConfiguration() {
        return _conf;
    }

private:
    TransportRegistry* _tr;
    Timer* _timer;
    Configuration* _conf;
};

class DummyResponseHandler : public ResponseHandler {
public:
    DummyResponseHandler(Context* context) :
        ResponseHandler(context), packets(0) {
    }

    int getPackets() {
        return packets;
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
            Transport* transport, int8 version, int8 command, int payloadSize,
            ByteBuffer* payloadBuffer);
private:
    int packets;
};

void DummyResponseHandler::handleResponse(osiSockAddr* responseFrom,
        Transport* transport, int8 version, int8 command, int payloadSize,
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

    for(int i = 0; i<payloadSize;) {
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
    BlockingUDPConnector connector(false, NULL, true);
    ContextImpl ctx;

    DummyResponseHandler drh(&ctx);

    osiSockAddr bindAddr;

    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(65000);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    Transport* transport = connector.connect(NULL, &drh, bindAddr, 1, 50);

    ((BlockingUDPTransport*)transport)->start();

    cout<<"Waiting for 10 packets..."<<endl;

    while(drh.getPackets()<10) {
        sleep(1);
    }

    delete transport;
}

int main(int argc, char *argv[]) {
    createFileLogger("testBlockingUDPSrv.log");

    testBlockingUDPConnector();
    return (0);
}
