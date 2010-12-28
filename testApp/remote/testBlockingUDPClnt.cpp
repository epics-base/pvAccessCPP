/*
 * testBlockingUDPClnt.cpp
 *
 *  Created on: Dec 28, 2010
 *      Author: Miha Vitorovic
 */

#include "remote.h"
#include "blockingUDP.h"
#include "logger.h"
#include "inetAddressUtil.h"

#include <osiSock.h>

#include <iostream>
#include <cstdio>

using namespace epics::pvAccess;
using namespace epics::pvData;

using std::cout;
using std::endl;
using std::sscanf;

static osiSockAddr sendTo;

class DummyResponseHandler : public ResponseHandler {
public:
    virtual void handleResponse(osiSockAddr* responseFrom,
            Transport* transport, int8 version, int8 command, int payloadSize,
            ByteBuffer* payloadBuffer) {
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
        // set recipient
        sendTo.ia.sin_family = AF_INET;
        sendTo.ia.sin_port = htons(65000);
        if(inet_aton("192.168.71.129",&sendTo.ia.sin_addr)==0) {
            cout<<"error in inet_aton()"<<endl;
            return;
        }
        control->setRecipient(&sendTo);

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
private:
    char data[20];
    int count;
};

void testBlockingUDPSender() {
    BlockingUDPConnector connector(false, NULL, true);

    DummyTransportSender dts;
    DummyResponseHandler drh;

    osiSockAddr bindAddr;

    bindAddr.ia.sin_family = AF_INET;
    bindAddr.ia.sin_port = htons(65001);
    bindAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);

    Transport* transport = connector.connect(NULL, &drh, &bindAddr, 1, 50);

    cout<<"Sending 10 packets..."<<endl;

    for(int i = 0; i<10; i++) {
        cout<<"   Packet: "<<i+1<<endl;
        transport->enqueueSendRequest(&dts);
        sleep(1);
    }

}

int main(int argc, char *argv[]) {
    createFileLogger("testBlockingUDPClnt.log");

    testBlockingUDPSender();
    return (0);
}
