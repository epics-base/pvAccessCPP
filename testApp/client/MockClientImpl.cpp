/* MockClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.18 */


#include <pvAccess.h>
#include <iostream>

using namespace epics::pvData;
using namespace epics::pvAccess;



class MockChannelProvider : public ChannelProvider {
public:

    virtual epics::pvData::String getProviderName()
    {
        return "MockChannelProvider";
    }
    
    virtual void destroy()
    {
        delete this;
    }
    
    virtual ChannelFind* channelFind(
        epics::pvData::String channelName,
        ChannelFindRequester *channelFindRequester)
    {
        ChannelFind* channelFind = 0;   // TODO
        channelFindRequester->channelFindResult(getStatusCreate()->getStatusOK(), channelFind, true);
        return channelFind;
    }

    virtual Channel* createChannel(
        epics::pvData::String channelName,
        ChannelRequester *channelRequester,
        short priority)
    {
        return createChannel(channelName, channelRequester, priority, "local");
    }

    virtual Channel* createChannel(
        epics::pvData::String channelName,
        ChannelRequester *channelRequester,
        short priority,
        epics::pvData::String address)
    {
        if (address == "local")
        {
            Channel* channel = 0;
            channelRequester->channelCreated(getStatusCreate()->getStatusOK(), channel);
            // TODO state change
            return channel;
        }
        else
        {   
            Status* errorStatus = getStatusCreate()->createStatus(STATUSTYPE_ERROR, "only local supported", 0);
            channelRequester->channelCreated(errorStatus, 0);
            return 0;
        }
    }
    
    private:
    ~MockChannelProvider() {};
    
};




class MockClientContext : public ClientContext
{
    public:
    
    MockClientContext() : m_version(new Version("Mock CA Client", "cpp", 1, 0, 0, 0))
    {
        initialize();
    }
    
    virtual const Version* getVersion() {
        return m_version;
    }

    virtual const ChannelProvider* getProvider() {
        return m_provider;
    }
    
    virtual void initialize() {
        m_provider = new MockChannelProvider();
    }
        
    virtual void printInfo() {
        String info;
        printInfo(&info);
        std::cout << info.c_str() << std::endl;
    }
    
    virtual void printInfo(epics::pvData::StringBuilder out) {
        out->append(m_version->getVersionString());
    }
    
    virtual void destroy()
    {
        m_provider->destroy();
        delete m_version;
        delete this;
    }
    
    virtual void dispose()
    {
        destroy();
    }    
           
    private:
    ~MockClientContext() {};
    
    Version* m_version;
    MockChannelProvider* m_provider;
};


int main(int argc,char *argv[])
{
    MockClientContext* context = new MockClientContext();
    context->printInfo();
    
    context->destroy();
    return(0);
}


