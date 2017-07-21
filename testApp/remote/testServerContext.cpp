/*
 * testServerContext.cpp
 */

#include <pv/serverContext.h>
#include <epicsExit.h>
#include <testMain.h>

#include <epicsUnitTest.h>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

class TestChannelProvider : public ChannelProvider
{
public:

    std::string getProviderName() {
        return "local";
    };

    TestChannelProvider() {}

    ChannelFind::shared_pointer channelFind(std::string const & /*channelName*/,
                                            ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        ChannelFind::shared_pointer nullCF;
        channelFindRequester->channelFindResult(Status::Ok, nullCF, false);
        return nullCF;
    }

    ChannelFind::shared_pointer channelList(ChannelListRequester::shared_pointer const & channelListRequester)
    {
        ChannelFind::shared_pointer nullCF;
        PVStringArray::const_svector none;
        channelListRequester->channelListResult(Status::Ok, nullCF, none, false);
        return nullCF;
    }

    Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority = PRIORITY_DEFAULT)
    {
        return createChannel(channelName, channelRequester, priority, "");
    }

    Channel::shared_pointer createChannel(
        std::string const & /*channelName*/,
        ChannelRequester::shared_pointer const & channelRequester,
        short /*priority*/, std::string const & /*address*/)
    {
        Channel::shared_pointer nullC;
        channelRequester->channelCreated(Status::Ok, nullC);
        return nullC;
    }

    void destroy()
    {
    }
};

void testServerContext()
{
    ChannelProvider::shared_pointer prov(new TestChannelProvider);
    ServerContext::shared_pointer ctx(ServerContext::create(ServerContext::Config()
                                                                .provider(prov)));
    ServerContext::weak_pointer wctx(ctx);

    testOk(ctx.unique(), "# ServerContext::create() returned non-unique instance use_count=%u", (unsigned)ctx.use_count());

    ctx->printInfo();

    ctx->run(1);

    ctx.reset();

    testOk(!wctx.lock(), "# ServerContext cleanup leaves use_count=%u", (unsigned)wctx.use_count());
}

MAIN(testServerContext)
{
    testPlan(0);

    testServerContext();

    return testDone();
}
