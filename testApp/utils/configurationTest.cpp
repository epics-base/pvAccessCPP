/*
 * configurationTest.cpp
 *
 */

#include <pv/configuration.h>

#include <iostream>
#include <string>
#include <memory>

#include <stdlib.h>

#include <epicsAssert.h>
#include <epicsExit.h>
#include <envDefs.h>
#include <epicsString.h>
#include <osiSock.h>

#include <epicsUnitTest.h>
#include <testMain.h>

using namespace epics::pvAccess;
using namespace epics::pvData;

static void showEnv(const char *name)
{
    testDiag("%s = \"%s\"", name, getenv(name));
}

static void setEnv(const char *name, const char *val)
{
    epicsEnvSet(name, val);
    testDiag("%s = \"%s\"", name, getenv(name));
}

static void testBuilder()
{
    Configuration::shared_pointer C(ConfigurationBuilder()
                                    .add("TESTKEY","value1")
                                    .push_map()
                                    .push_env()
                                    .add("OTHERKEY","value3")
                                    .push_map()
                                    .build());

    testOk1(C->getPropertyAsString("key", "X")=="X");
    testOk1(C->getPropertyAsString("TESTKEY", "X")=="value1");
    testOk1(C->getPropertyAsString("OTHERKEY", "X")=="value3");
    setEnv("TESTKEY", "value2");
    setEnv("OTHERKEY","value2");
    testOk1(C->getPropertyAsString("TESTKEY", "X")=="value2");
    testOk1(C->getPropertyAsString("OTHERKEY", "X")=="value3");
}

static void showAddr(const osiSockAddr& addr)
{
    char buf[40];
    sockAddrToDottedIP(&addr.sa, buf, sizeof(buf));
    testDiag("%s", buf);
}

#define TESTVAL(TYPE, VAL1, VAL2, VAL1S) do {\
    showEnv(#TYPE "Property"); \
    testOk1(configuration->getPropertyAs##TYPE(#TYPE "Property", VAL1) == VAL1); \
    testOk1(configuration->getPropertyAs##TYPE(#TYPE "Property", VAL2) == VAL2); \
    setEnv(#TYPE "Property", VAL1S); \
    testOk1(configuration->getPropertyAs##TYPE(#TYPE "Property", VAL1) == VAL1); \
    testOk1(configuration->getPropertyAs##TYPE(#TYPE "Property", VAL2) == VAL1); \
    } while(0)


static
void testConfig()
{
    testDiag("Default configuration");
    Configuration::shared_pointer configuration(new SystemConfigurationImpl());

    TESTVAL(String, "one", "two", "one");
    TESTVAL(Boolean, true, false, "true");
    TESTVAL(Integer, 100, 321, "100");
    TESTVAL(Float, 42.0e3, 44.0e3, "42.0e3");
    TESTVAL(Double, 42.0e3, 44.0e3, "42.0e3");

    testDiag("IP Address w/o default or explicit port");

    showEnv("AddressProperty");
    osiSockAddr addr;
    memset(&addr, 0, sizeof(addr));
    addr.ia.sin_family = AF_INET+1; // something not IPv4
    addr.ia.sin_port = htons(42);

    testOk1(configuration->getPropertyAsAddress("AddressProperty", &addr)==false);
    setEnv("AddressProperty", "127.0.0.1"); // no port
    testOk1(configuration->getPropertyAsAddress("AddressProperty", &addr)==true);
    showAddr(addr);

    testOk1(addr.ia.sin_family==AF_INET);
    testOk1(ntohl(addr.ia.sin_addr.s_addr)==INADDR_LOOPBACK);
    testOk1(ntohs(addr.ia.sin_port)==0);

    testDiag("IP Address w/ default port");

    memset(&addr, 0, sizeof(addr));
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_port = htons(42);

    testOk1(configuration->getPropertyAsAddress("AddressProperty", &addr)==true);
    showAddr(addr);

    testOk1(addr.ia.sin_family==AF_INET);
    testOk1(ntohl(addr.ia.sin_addr.s_addr)==INADDR_LOOPBACK);
    testOk1(ntohs(addr.ia.sin_port)==42);

    testDiag("IP Address w/ default and explicit port");

    setEnv("AddressProperty", "127.0.0.1:43"); // no port
    testOk1(configuration->getPropertyAsAddress("AddressProperty", &addr)==true);
    showAddr(addr);

    memset(&addr, 0, sizeof(addr));
    addr.ia.sin_family = AF_INET;
    addr.ia.sin_port = htons(42);

    testOk1(configuration->getPropertyAsAddress("AddressProperty", &addr)==true);
    showAddr(addr);

    testOk1(addr.ia.sin_family==AF_INET);
    testOk1(ntohl(addr.ia.sin_addr.s_addr)==INADDR_LOOPBACK);
    testOk1(ntohs(addr.ia.sin_port)==43);

    testDiag("register with global configuration listings");

    ConfigurationProvider::shared_pointer configProvider(ConfigurationFactory::getProvider());
    configProvider->registerConfiguration("conf1", configuration);

    Configuration::shared_pointer configurationOut(configProvider->getConfiguration("conf1"));
    testOk1(configurationOut.get() == configuration.get());
}

MAIN(configurationTest)
{
    testPlan(40);
    testBuilder();
    testConfig();
    return testDone();
}
