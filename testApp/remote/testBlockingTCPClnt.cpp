/*
 * testBlockingTCPClnt.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: Miha Vitorovic
 */

#include "remote.h"
#include "blockingTCP.h"
#include "logger.h"
#include "inetAddressUtil.h"
#include "caConstants.h"

#include <timer.h>

#include <osiSock.h>
#include <errlog.h>

#include <iostream>
#include <cstdio>

using namespace epics::pvAccess;
using namespace epics::pvData;

using std::cout;
using std::endl;
using std::sscanf;

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

private:
    TransportRegistry* _tr;
    Timer* _timer;
    Configuration* _conf;
};

class DummyResponseHandler : public ResponseHandler {
public:
    DummyResponseHandler(Context* ctx) : ResponseHandler(ctx) {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
            Transport* transport, int8 version, int8 command, int payloadSize,
            ByteBuffer* payloadBuffer) {

        if(command==CMD_CONNECTION_VALIDATION) transport->verified();
    }
};

class DummyTransportClient : public TransportClient {
public:
    DummyTransportClient() {
    }
    virtual ~DummyTransportClient() {
    }
    virtual void transportUnresponsive() {
        errlogSevPrintf(errlogInfo, "unresponsive");
    }
    virtual void transportResponsive(Transport* transport) {
        errlogSevPrintf(errlogInfo, "responsive");
    }
    virtual void transportChanged() {
        errlogSevPrintf(errlogInfo, "changed");
    }
    virtual void transportClosed() {
        errlogSevPrintf(errlogInfo, "closed");
    }
};

class DummyTransportSender : public TransportSender {
public:
    DummyTransportSender() {
        for(int i = 0; i<20; i++)
            data[i] = (char)(i+1);
        count = 0;
    }

    virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        // send the packet
        count++;
        control->startMessage(0, count);
        buffer->put(data, 0, count);
        //control->endMessage();
    }

    virtual void lock() {
    }
    virtual void unlock() {
    }
private:
    char data[20];
    int count;
};

void testBlockingTCPSender() {
    ContextImpl ctx;
    BlockingTCPConnector connector(&ctx, 1024, 1.0);

    DummyTransportClient dtc;
    DummyTransportSender dts;
    DummyResponseHandler drh(&ctx);

    osiSockAddr srvAddr;

    //srvAddr.ia.sin_family = AF_INET;
    if(aToIPAddr("192.168.71.132", CA_SERVER_PORT, &srvAddr.ia)<0) {
        cout<<"error in aToIPAddr(...)"<<endl;
        return;
    }

    Transport* transport = connector.connect(&dtc, &drh, srvAddr,
            CA_MAGIC_AND_VERSION, CA_DEFAULT_PRIORITY);

    cout<<"Sending 10 messages..."<<endl;

    for(int i = 0; i<10; i++) {
        cout<<"   Message: "<<i+1<<endl;
        if(!transport->isClosed())
            transport->enqueueSendRequest(&dts);
        else
            break;
        sleep(1);
    }

    delete transport;
}

int main(int argc, char *argv[]) {
    createFileLogger("testBlockingTCPClnt.log");

    testBlockingTCPSender();
    return 0;
}
