/*
 * testServerContext.cpp
 */

#include <pv/serverContext.h>
#include <epicsExit.h>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

class TestChannelProvider : public ChannelProvider
{
public:

    std::string getProviderName() {
        return "local";
    };


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


class TestChannelProviderRegistry : public ChannelProviderRegistry {
public:

    virtual ~TestChannelProviderRegistry() {};

    ChannelProvider::shared_pointer getProvider(std::string const & providerName)
    {
        if (providerName == "local")
        {
            return ChannelProvider::shared_pointer(new TestChannelProvider());
        }
        else
            return ChannelProvider::shared_pointer();
    }

    ChannelProvider::shared_pointer createProvider(std::string const & providerName)
    {
        return getProvider(providerName);
    }

    std::auto_ptr<stringVector_t> getProviderNames()
    {
        std::auto_ptr<stringVector_t> pn(new stringVector_t());
        pn->push_back("local");
        return pn;
    }
};

void testServerContext()
{

    ServerContextImpl::shared_pointer ctx = ServerContextImpl::create();

    ChannelProviderRegistry::shared_pointer ca(new TestChannelProviderRegistry());
    ctx->initialize(ca);

    ctx->printInfo();

    ctx->run(1);

    ctx->destroy();
}

int main()
{
    testServerContext();

    cout << "Done" << endl;

    //epicsExitCallAtExits();
    return (0);
}
