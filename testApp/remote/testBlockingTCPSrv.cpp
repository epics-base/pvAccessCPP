/*
 * testBlockingTCPSrv.cpp
 *
 *  Created on: Jan 6, 2011
 *      Author: Miha Vitorovic
 */

#include "blockingTCP.h"
#include "remote.h"
#include "logger.h"

#include <iostream>

using namespace epics::pvData;
using namespace epics::pvAccess;

using std::cin;
using std::cout;

class ContextImpl : public Context {
public:
    ContextImpl() :
        _tr(new TransportRegistry()),
        _timer(new Timer("server thread", lowPriority)) {}
    virtual ~ContextImpl() {
        delete _tr;
        delete _timer;
    }
    virtual Timer* getTimer() { return _timer; }
    virtual TransportRegistry* getTransportRegistry() { return _tr; }
	virtual Channel* getChannel(epics::pvAccess::pvAccessID) { return 0; }
	virtual Transport* getSearchTransport() { return 0; }
    
private:
    TransportRegistry* _tr;
    Timer* _timer;
};

void testServerConnections() {
    ContextImpl ctx;

    BlockingTCPAcceptor* srv = new BlockingTCPAcceptor(&ctx, CA_SERVER_PORT,
            1024);

    cout<<"Press any key to stop the server...";
    char c = cin.peek();

    delete srv;
}

int main(int argc, char *argv[]) {

    createFileLogger("testBlockingTCPSrv.log");

    testServerConnections();
}
