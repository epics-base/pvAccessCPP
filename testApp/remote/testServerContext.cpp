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
       
    epics::pvData::String getProviderName() { return "local"; };
    

    ChannelFind::shared_pointer channelFind(epics::pvData::String const & /*channelName*/,
                                            ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        ChannelFind::shared_pointer nullCF;
        channelFindRequester->channelFindResult(Status::Ok, nullCF, false); 
        return nullCF;  
    }

    Channel::shared_pointer createChannel(
                epics::pvData::String const & channelName,
                ChannelRequester::shared_pointer const & channelRequester,
                short priority = PRIORITY_DEFAULT)  
    {
        return createChannel(channelName, channelRequester, priority, "");
    }

    Channel::shared_pointer createChannel(
                epics::pvData::String const & /*channelName*/,
                ChannelRequester::shared_pointer const & channelRequester,
                short /*priority*/, epics::pvData::String const & /*address*/)
    {
        Channel::shared_pointer nullC;
        channelRequester->channelCreated(Status::Ok, nullC);
        return nullC;    
    }
    
    void destroy()
    {
    }
};


class TestChannelAccess : public ChannelAccess {
public:
                
    virtual ~TestChannelAccess() {};
            
    ChannelProvider::shared_pointer getProvider(epics::pvData::String const & providerName)
    {
        if (providerName == "local")
        {
            return ChannelProvider::shared_pointer(new TestChannelProvider()); 
        }
        else
            return ChannelProvider::shared_pointer(); 
    }
            
    ChannelProvider::shared_pointer createProvider(epics::pvData::String const & providerName)
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

	ChannelAccess::shared_pointer ca(new TestChannelAccess());
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
