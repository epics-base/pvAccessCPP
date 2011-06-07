/*
 * testServerContext.cpp
 */

#include <pv/serverContext.h>
#include <pv/CDRMonitor.h>
#include <epicsExit.h>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;

class TestChannelProvider : public ChannelProvider
{
public:
       
    epics::pvData::String getProviderName() { return "local"; };
    

    ChannelFind::shared_pointer channelFind(epics::pvData::String channelName,
                                            ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        ChannelFind::shared_pointer nullCF;
        channelFindRequester->channelFindResult(Status::OK, nullCF, false); 
        return nullCF;  
    }

    Channel::shared_pointer createChannel(
                epics::pvData::String channelName,
                ChannelRequester::shared_pointer const & channelRequester,
                short priority = PRIORITY_DEFAULT)  
    {
        return createChannel(channelName, channelRequester, priority, "");
    }

    Channel::shared_pointer createChannel(
                epics::pvData::String channelName,
                ChannelRequester::shared_pointer const & channelRequester,
                short priority, epics::pvData::String address)
    {
        Channel::shared_pointer nullC;
        channelRequester->channelCreated(Status::OK, nullC);
        return nullC;    
    }
    
    void destroy()
    {
    }
};


class TestChannelAccess : public ChannelAccess {
public:
                
    virtual ~TestChannelAccess() {};
            
    ChannelProvider::shared_pointer getProvider(epics::pvData::String providerName)
    {
        if (providerName == "local")
        {
            return ChannelProvider::shared_pointer(new TestChannelProvider()); 
        }
        else
            return ChannelProvider::shared_pointer(); 
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

int main(int argc, char *argv[])
{
	testServerContext();

	cout << "Done" << endl;

	epicsExitCallAtExits();
    CDRMonitor::get().show(stdout);
    return (0);
}
