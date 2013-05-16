/*
 * testBlockingTCPClnt.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: Miha Vitorovic
 */

#include <pv/remote.h>
#include <pv/blockingTCP.h>
#include <pv/logger.h>
#include <pv/inetAddressUtil.h>
#include <pv/pvaConstants.h>

#include <pv/timer.h>
#include <pv/epicsException.h>
#include <pv/pvType.h>

#include <osiSock.h>
#include <epicsThread.h>
#include <pv/logger.h>

#include <iostream>
#include <cstdio>

using std::tr1::static_pointer_cast;

using namespace epics::pvAccess;
using namespace epics::pvData;

using std::cout;
using std::endl;
using std::sscanf;


class ContextImpl : public Context {
public:
    ContextImpl() :
    		 _tr(new TransportRegistry()), _timer(new Timer("server thread",
    		                lowPriority)), _conf(new SystemConfigurationImpl())
    {}

    virtual ~ContextImpl() {
    }
    virtual Timer::shared_pointer getTimer() {
        return _timer;
    }
    virtual std::tr1::shared_ptr<TransportRegistry> getTransportRegistry() {
        return _tr;
    }
    virtual std::tr1::shared_ptr<Channel> getChannel(epics::pvAccess::pvAccessID) {
        return std::tr1::shared_ptr<Channel>();
    }
    virtual Transport::shared_pointer getSearchTransport() {
        return Transport::shared_pointer();
    }
    virtual Configuration::shared_pointer getConfiguration() {
        return _conf;
    }
    virtual void acquire() {}
    virtual void release() {}
    virtual void newServerDetected() {}

private:
    std::tr1::shared_ptr<TransportRegistry> _tr;
    Timer::shared_pointer _timer;
    Configuration::shared_pointer _conf;
};

class DummyResponseHandler : public ResponseHandler {
public:
    DummyResponseHandler() :
        ResponseHandler() {
    }

    virtual void handleResponse(osiSockAddr* /*responseFrom*/,
    		Transport::shared_pointer const & transport, int8 /*version*/, int8 command, std::size_t /*payloadSize*/,
            ByteBuffer* /*payloadBuffer*/) {

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
        LOG(logLevelInfo, "unresponsive");
    }
    virtual void transportResponsive(Transport::shared_pointer const & /*transport*/) {
        LOG(logLevelInfo, "responsive");
    }
    virtual void transportChanged() {
        LOG(logLevelInfo, "changed");
    }
    virtual void transportClosed() {
        LOG(logLevelInfo, "closed");
    }
    virtual void acquire() {};
    virtual void release() {};
    virtual pvAccessID getID() {return 0;};
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
        // using invalid command to force msg dump
        control->startMessage(0xC0, count);
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

void testBlockingTCPSender() {
	Context::shared_pointer  ctx(new ContextImpl());
	BlockingTCPConnector connector(ctx, 1024, 1.0);

    TransportClient::shared_pointer dtc(new DummyTransportClient());
    TransportSender::shared_pointer dts(new DummyTransportSender());
    std::auto_ptr<ResponseHandler> drh(new DummyResponseHandler());

    osiSockAddr srvAddr;

    //srvAddr.ia.sin_family = AF_INET;
    if(aToIPAddr("localhost", PVA_SERVER_PORT, &srvAddr.ia)<0) {
        cout<<"error in aToIPAddr(...)"<<endl;
        return;
    }

    try {
    	Transport::shared_pointer transport(connector.connect(dtc, drh, srvAddr,
                PVA_VERSION, PVA_DEFAULT_PRIORITY));

        cout<<"Sending 10 messages..."<<endl;

        for(int i = 0; i<10; i++) {
            cout<<"   Message: "<<i+1<<endl;
            if(!transport->isClosed())
                transport->enqueueSendRequest(dts);
            else
                break;
            epicsThreadSleep(1.0);
        }
    } catch(std::exception& e) {
        cout<<e.what()<<endl;
    }


}

int main() {
    createFileLogger("testBlockingTCPClnt.log");

    testBlockingTCPSender();
    return 0;
}
