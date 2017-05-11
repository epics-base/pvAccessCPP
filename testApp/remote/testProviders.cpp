/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsUnitTest.h>
#include <testMain.h>
#include <epicsThread.h>

#include <pv/pvAccess.h>
#include <pv/clientFactory.h>
#include <pv/caProvider.h>

namespace {

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;

struct TestChanReq : public pva::ChannelRequester
{
    POINTER_DEFINITIONS(TestChanReq);

    virtual std::string getRequesterName() { return "TestChanReq"; }

    virtual void channelCreated(const pvd::Status& status, pva::Channel::shared_pointer const & channel)
    {}

    virtual void channelStateChange(pva::Channel::shared_pointer const & channel, pva::Channel::ConnectionState connectionState)
    {}
};

struct TestGetReq : public pva::ChannelGetRequester
{
    POINTER_DEFINITIONS(TestGetReq);

    virtual std::string getRequesterName() { return "TestGetReq"; }

    virtual void channelGetConnect(
        const pvd::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        pvd::Structure::const_shared_pointer const & structure)
    {}

    virtual void getDone(
        const pvd::Status& status,
        pva::ChannelGet::shared_pointer const & channelGet,
        pvd::PVStructure::shared_pointer const & pvStructure,
        pvd::BitSet::shared_pointer const & bitSet)
    {}
};

static
void testChannelCreateDestroy(const char *providerName,
                              const char *badChannel)
{
    testDiag("== testChannelCreateDestroy(\"%s\", \"%s\")", providerName, badChannel);

    pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName));
    pva::ChannelProvider::weak_pointer providerW(provider);

    testOk(provider.unique(), "create provider refs=%u", (unsigned)provider.use_count());

    TestChanReq::shared_pointer req(new TestChanReq);
    pva::Channel::shared_pointer chan(provider->createChannel(badChannel, req));
    pva::Channel::weak_pointer chanW(chan);

    testOk(chan.unique(), "create non-existant channel refs=%u", (unsigned)chan.use_count());
    epicsThreadSleep(1.0); // wait in case some worker thread is lazy...
    testOk(chan.unique(), "2create non-existant channel refs=%u", (unsigned)chan.use_count());

    chan->destroy();

    testOk(chan.unique(), "destroy non-existant channel refs=%u", (unsigned)chan.use_count());
    testOk(req.unique(), "ChannelRequester after Channel::destroy() refs=%u", (unsigned)req.use_count());
    epicsThreadSleep(1.0);
    testOk(chan.unique(), "2destroy non-existant channel refs=%u", (unsigned)chan.use_count());
    testOk(req.unique(), "2ChannelRequester after Channel::destroy() refs=%u", (unsigned)req.use_count());

    chan.reset();
    testOk(req.unique(), "ChannelRequester after Channel last refs=%u", (unsigned)req.use_count());
    req.reset();

    testOk(!chanW.lock(), "Channel leaked from live Provider");

    testOk(provider.unique(), "empty provider refs=%u", (unsigned)provider.use_count());
    epicsThreadSleep(1.0);
    testOk(provider.unique(), "2empty provider refs=%u", (unsigned)provider.use_count());

    provider->destroy();

    testOk(provider.unique(), "empty provider refs=%u", (unsigned)provider.use_count());
    epicsThreadSleep(1.0);
    testOk(provider.unique(), "2empty provider refs=%u", (unsigned)provider.use_count());

    testOk(!chanW.lock(), "Channel leaked from dead Provider");

    provider.reset();

    testOk(!providerW.lock(), "Provider leaks");
}

static
void testChannelCreateDrop(const char *providerName,
                              const char *badChannel)
{
    testDiag("== testChannelCreateDrop(\"%s\", \"%s\")", providerName, badChannel);

    pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName));
    pva::ChannelProvider::weak_pointer providerW(provider);

    testOk(provider.unique(), "create provider refs=%u", (unsigned)provider.use_count());

    TestChanReq::shared_pointer req(new TestChanReq);
    pva::Channel::shared_pointer chan(provider->createChannel(badChannel, req));
    pva::Channel::weak_pointer chanW(chan);

    testOk(chan.unique(), "create non-existant channel refs=%u", (unsigned)chan.use_count());
    epicsThreadSleep(1.0);
    testOk(chan.unique(), "2create non-existant channel refs=%u", (unsigned)chan.use_count());

    testDiag("Drop our ref to Channel w/o destory()");

    chan.reset();
    testOk(req.unique(), "ChannelRequester after Channel last refs=%u", (unsigned)req.use_count());
    req.reset();

    testOk(!chanW.lock(), "Channel leaked from live Provider");

    testOk(provider.unique(), "empty provider refs=%u", (unsigned)provider.use_count());
    epicsThreadSleep(1.0);
    testOk(provider.unique(), "2empty provider refs=%u", (unsigned)provider.use_count());

    provider->destroy();

    testOk(provider.unique(), "empty provider refs=%u", (unsigned)provider.use_count());
    epicsThreadSleep(1.0);
    testOk(provider.unique(), "2empty provider refs=%u", (unsigned)provider.use_count());

    testOk(!chanW.lock(), "Channel leaked from dead Provider");

    provider.reset();

    testOk(!providerW.lock(), "Provider leaks");
}

static
void testChannelGetDestroy(const char *providerName,
                           const char *badChannel)
{
    testDiag("== testChannelGet(\"%s\", \"%s\")", providerName, badChannel);

    pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName));

    testOk(provider.unique(), "create provider refs=%u", (unsigned)provider.use_count());

    TestChanReq::shared_pointer req(new TestChanReq);
    pva::Channel::shared_pointer chan(provider->createChannel(badChannel, req));

    testOk(chan.unique(), "create non-existant channel refs=%u", (unsigned)chan.use_count());

    TestGetReq::shared_pointer getreq(new TestGetReq);
    pva::ChannelGet::shared_pointer get(chan->createChannelGet(getreq,
                                                               pvd::getPVDataCreate()->createPVStructure(
                                                                   pvd::getFieldCreate()->createFieldBuilder()->createStructure()
                                                                   )));

    testOk(get.unique(), "create get refs=%u", (unsigned)get.use_count());

    get->destroy();

    testOk(get.unique(), "get after ChannelGet::destroy() refs=%u", (unsigned)get.use_count());
    testOk(getreq.unique(), "getreq after ChannelGet::destroy() refs=%u", (unsigned)getreq.use_count());

    chan->destroy();

    testOk(get.unique(), "get after Channel::destroy() refs=%u", (unsigned)get.use_count());
    testOk(chan.unique(), "destroy non-existant channel refs=%u", (unsigned)chan.use_count());
    testOk(req.unique(), "ChannelRequester after Channel::destroy() refs=%u", (unsigned)req.use_count());

    get.reset();
    getreq.reset();
    chan.reset();
    req.reset();

    testOk(provider.unique(), "empty provider refs=%u", (unsigned)provider.use_count());
}

static
void testChannelGetDrop(const char *providerName,
                           const char *badChannel)
{
    testDiag("== testChannelGetDrop(\"%s\", \"%s\")", providerName, badChannel);

    pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName));

    testOk(provider.unique(), "create provider refs=%u", (unsigned)provider.use_count());

    TestChanReq::shared_pointer req(new TestChanReq);
    pva::Channel::shared_pointer chan(provider->createChannel(badChannel, req));

    testOk(chan.unique(), "create non-existant channel refs=%u", (unsigned)chan.use_count());

    TestGetReq::shared_pointer getreq(new TestGetReq);
    pva::ChannelGet::shared_pointer get(chan->createChannelGet(getreq,
                                                               pvd::getPVDataCreate()->createPVStructure(
                                                                   pvd::getFieldCreate()->createFieldBuilder()->createStructure()
                                                                   )));

    testOk(get.unique(), "create get refs=%u", (unsigned)get.use_count());

    testDiag("Drop reference to Channel");
    chan.reset();

    testDiag("Drop reference to ChannelGet");
    get.reset();

    testOk(getreq.unique(), "getreq after ChannelGet::destroy() refs=%u", (unsigned)getreq.use_count());
    epicsThreadSleep(1.0);
    testOk(getreq.unique(), "2getreq after ChannelGet::destroy() refs=%u", (unsigned)getreq.use_count());

    testOk(chan.unique(), "destroy non-existant channel refs=%u", (unsigned)chan.use_count());
    testOk(req.unique(), "ChannelRequester after Channel::destroy() refs=%u", (unsigned)req.use_count());

    get.reset();
    getreq.reset();
    chan.reset();
    req.reset();

    testOk(provider.unique(), "empty provider refs=%u", (unsigned)provider.use_count());
}

static
void testChannelDrop(const char *providerName,
                            const char *badChannel)
{
    testDiag("== testChannelDrop(\"%s\", \"%s\")", providerName, badChannel);

    pva::ChannelProvider::shared_pointer provider(pva::getChannelProviderRegistry()->createProvider(providerName));

    testOk(provider.unique(), "create provider refs=%u", (unsigned)provider.use_count());

    TestChanReq::shared_pointer req(new TestChanReq);
    pva::Channel::shared_pointer chan(provider->createChannel(badChannel, req));
    pva::Channel::weak_pointer chanW(chan);

    testOk(chan.unique(), "create non-existant channel refs=%u", (unsigned)chan.use_count());

    TestGetReq::shared_pointer getreq(new TestGetReq);
    pva::ChannelGet::shared_pointer get(chan->createChannelGet(getreq,
                                                               pvd::getPVDataCreate()->createPVStructure(
                                                                   pvd::getFieldCreate()->createFieldBuilder()->createStructure()
                                                                   )));
    pva::ChannelGet::weak_pointer getW(get);

    testOk(get.unique(), "create get refs=%u", (unsigned)get.use_count());

    testDiag("drop our ref to Channel then ChannelGet");
    chan.reset();
    get.reset();

    testOk(!chanW.lock(), "Channel kept alive NULL==%p", chanW.lock().get());
    testOk(!getW.lock(), "ChannelGet kept alive NULL==%p", getW.lock().get());

    testDiag("drop our ref to Provider");
    provider.reset();

    testOk(!chanW.lock(), "Channel kept alive after Provider NULL==%p", chanW.lock().get());
    testOk(!getW.lock(), "ChannelGet kept alive after Provider NULL==%p", getW.lock().get());
}

} // namespace

MAIN(testProviders) {
    testPlan(0);
    testDiag("==== PVA ====");
    {
        pva::ClientFactory::start();
        testChannelCreateDestroy("pva", "reallyInvalidChannelName");
        testChannelCreateDrop   ("pva", "reallyInvalidChannelName");
        testChannelGetDestroy   ("pva", "reallyInvalidChannelName");
        testChannelDrop         ("pva", "reallyInvalidChannelName");
        pva::ClientFactory::stop();
    }
    testDiag("==== CA ====");
    {
        pva::ca::CAClientFactory::start();
        testChannelCreateDestroy("ca", "reallyInvalidChannelName");
        testChannelCreateDrop   ("ca", "reallyInvalidChannelName");
        testChannelGetDestroy   ("ca", "reallyInvalidChannelName");
        testChannelDrop         ("ca", "reallyInvalidChannelName");
        pva::ca::CAClientFactory::stop();
    }
    return testDone();
}
