/* testChannelAccessFactory.cpp */
/* Author:  Matej Sekoranja Date: 2010.11.03 */

#include <stddef.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <pv/pvAccess.h>


#include <epicsAssert.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

class DummyChannelProvider : public ChannelProvider {
private:
    String m_name;
public:
    DummyChannelProvider(String name) : m_name(name) {};
    void destroy() {};
    String getProviderName() { return m_name; };

    ChannelFind* channelFind(String channelName,ChannelFindRequester *channelFindRequester)
    { return 0; }

    Channel* createChannel(String channelName,ChannelRequester *channelRequester,short priority)
    { return 0; }

    Channel* createChannel(String channelName,ChannelRequester *channelRequester,short priority,String address)
    { return 0; }
};



void testChannelAccessFactory() {
    printf("testChannelAccessFactory... ");

    ChannelAccess* ca = getChannelAccess();
    assert(ca);

    // empty
    std::vector<String>* providers = ca->getProviderNames();
    assert(providers);
    assert(providers->size() == 0);
    delete providers;

    // register 2
    ChannelProvider* cp1 = new DummyChannelProvider("dummy1");
    registerChannelProvider(cp1);

    ChannelProvider* cp2 = new DummyChannelProvider("dummy2");
    registerChannelProvider(cp2);

    providers = ca->getProviderNames();
    assert(providers);
    assert(providers->size() == 2);
    assert(providers->at(0) == "dummy1");
    assert(providers->at(1) == "dummy2");

    assert(ca->getProvider("dummy1") == cp1);
    assert(ca->getProvider("dummy2") == cp2);

    delete providers;


    // unregister first
    unregisterChannelProvider(cp1);

    providers = ca->getProviderNames();
    assert(providers);
    assert(providers->size() == 1);
    assert(providers->at(0) == "dummy2");
    assert(ca->getProvider("dummy2") == cp2);
    delete providers;

    printf("PASSED\n");

}

int main(int argc,char *argv[])
{
    testChannelAccessFactory();
    return(0);
}


