/*
 * testBlockingTCPSrv.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: Miha Vitorovic
 */

#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/logger.h>
#include <pv/configuration.h>
#include <pv/serverContext.h>

#include <iostream>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

class ContextImpl : public ServerContextImpl {
public:
    ContextImpl() :
        _tr(new TransportRegistry()),
        _timer(new Timer("server thread", lowPriority)),
        _conf(new SystemConfigurationImpl()) {}
    Timer::shared_pointer getTimer() { return _timer; }
    std::tr1::shared_ptr<TransportRegistry> getTransportRegistry() { return _tr; }
    Channel::shared_pointer getChannel(epics::pvAccess::pvAccessID) { return Channel::shared_pointer(); }
    Transport::shared_pointer getSearchTransport() { return Transport::shared_pointer(); }
	Configuration::shared_pointer getConfiguration() { return _conf; }
    virtual void acquire() {}
    virtual void release() {}

private:
    std::tr1::shared_ptr<TransportRegistry> _tr;
    Timer::shared_pointer _timer;
    Configuration::shared_pointer _conf;
};

class DummyResponseHandler : public ResponseHandler {
public:
    virtual void handleResponse(osiSockAddr* /*responseFrom*/,
    		Transport::shared_pointer const & /*transport*/, int8 /*version*/, int8 /*command*/, std::size_t /*payloadSize*/,
            ByteBuffer* /*payloadBuffer*/) {
    	cout << "DummyResponseHandler::handleResponse" << endl;
    }
};

class DummyResponseHandlerFactory : public ResponseHandlerFactory
 {
     public:
     std::auto_ptr<ResponseHandler> createResponseHandler() {return std::auto_ptr<ResponseHandler>(new DummyResponseHandler());};
 };


void testServerConnections() {
    Context::shared_pointer ctx(new ContextImpl());
    ResponseHandlerFactory::shared_pointer rhf(new DummyResponseHandlerFactory());

    BlockingTCPAcceptor* srv = new BlockingTCPAcceptor(ctx, rhf, PVA_SERVER_PORT, 1024);

    cout<<"Press any key to stop the server...";
    cin.peek();

    delete srv;
}

int main() {

    createFileLogger("testBlockingTCPSrv.log");

    testServerConnections();
}
