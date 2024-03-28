// Test channel discovery using name server search

#include <stdlib.h>
#include <sstream>

#include <epicsExit.h>
#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/logger.h>
#include <pv/pvAccess.h>

#include "channelDiscoveryTest.cpp"

#define TESTSERVERNOMAIN
#include "testServer.cpp"

namespace EPVA = epics::pvAccess;


int runAllTests() {

    EPVA::Configuration::shared_pointer baseConfig(ConfigurationBuilder()
        .add("EPICS_PVAS_INTF_ADDR_LIST", "127.0.0.1")
        .add("EPICS_PVA_SERVER_PORT", "0")
        .add("EPICS_PVAS_BROADCAST_PORT", "0")
        .push_map()
        .build());

    EPVA::Configuration::shared_pointer serverConfig(ConfigurationBuilder()
        .push_config(baseConfig)
        .push_map()
        .build());

    TestServer::shared_pointer testServer(new TestServer(serverConfig));
    std::ostringstream portStr;
    portStr << "127.0.0.1:" << testServer->getSearchServerPort();
    EPVA::Configuration::shared_pointer clientConfig(ConfigurationBuilder()
        .push_config(baseConfig)
        .add("EPICS_PVA_NAME_SERVERS", portStr.str())
        .push_map()
        .build());

    ConfigurationFactory::registerConfiguration("pvAccess-client", clientConfig);
    epics::pvAccess::ClientFactory::start();

    ChannelDiscoveryTest cdTest;
    return cdTest.runAllTests();
}

MAIN(testNsChannelDiscovery)
{
    try{
        SET_LOG_LEVEL(logLevelError);
        return runAllTests();
    }
    catch(std::exception& e) {
        PRINT_EXCEPTION(e);
        std::cerr << "Unhandled exception: " << e.what() << "\n";
        return 1;
    }
}
