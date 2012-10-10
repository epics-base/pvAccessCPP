/*
 * testBlockingUDPClnt.cpp
 *
 *  Created on: Dec 28, 2010
 *      Author: Miha Vitorovic
 */

#include <pv/remote.h>
#include <pv/blockingUDP.h>
#include <pv/logger.h>
#include <pv/inetAddressUtil.h>

//#include <pv/CDRMonitor.h>

#include <osiSock.h>
#include <epicsThread.h>

#include <iostream>
#include <cstdio>

#define SRV_IP "127.0.0.1"

using namespace epics::pvAccess;
using namespace epics::pvData;
using std::tr1::static_pointer_cast;
using namespace std;

static osiSockAddr sendTo;

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
    DummyResponseHandler(Context* /*ctx*/)
    { }

    virtual ~DummyResponseHandler() {}

    virtual void handleResponse(osiSockAddr* /*responseFrom*/,
    		Transport::shared_pointer const & /*transport*/, int8 /*version*/, int8 /*command*/, std::size_t /*payloadSize*/,
            ByteBuffer* /*payloadBuffer*/) {
    }
};

class DummyTransportSender : public TransportSender {
public:
	 typedef std::tr1::shared_ptr<DummyTransportSender> shared_pointer;
	 typedef std::tr1::shared_ptr<const DummyTransportSender> const_pointer;

    DummyTransportSender() {
        for(int i = 0; i<20; i++)
            data[i] = (char)(i+1);
        count = 0;
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        control->setRecipient(sendTo);

        // send the packet
        count++;
        control->startMessage((int8)(count+0x10), 0);
        buffer->put(data, 0, count);
        //control->endMessage();
    }

    virtual void lock() {
    }
    virtual void unlock() {
    }
    virtual void acquire() {
    }
    virtual void release() {
    }
private:
    char data[20];
    int count;
};

void testBlockingUDPSender() {
    BlockingUDPConnector connector(false, true);
    ContextImpl ctx;


    auto_ptr<ResponseHandler> drh(new DummyResponseHandler(&ctx));
    TransportSender::shared_pointer dts(new DummyTransportSender());

    osiSockAddr bindAddr;

    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(65001);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    TransportClient::shared_pointer nullPointer;
    Transport::shared_pointer transport(connector.connect(nullPointer, drh, bindAddr, 1, 50));

    // SRV_IP defined at the top of the this file
    if(aToIPAddr(SRV_IP, 65000, &sendTo.ia)<0) {
        cout<<"error in aToIPAddr(...)"<<endl;
        return;
    }

    cout<<"Sending 10 packets..."<<endl;


    for(int i = 0; i<10; i++) {
        cout<<"   Packet: "<<i+1<<endl;
        transport->enqueueSendRequest(dts);
        epicsThreadSleep(1.0);
    }
}

int main() {
//    createFileLogger("testBlockingUDPClnt.log");

    testBlockingUDPSender();

//    std::cout << "-----------------------------------------------------------------------" << std::endl;
//    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return (0);
}
