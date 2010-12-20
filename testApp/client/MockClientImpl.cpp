/* MockClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.18 */


#include <pvAccess.h>
#include <iostream>
#include <showConstructDestruct.h>
#include <lock.h>

using namespace epics::pvData;
using namespace epics::pvAccess;





static volatile int64 mockChannel_totalConstruct = 0;
static volatile int64 mockChannel_totalDestruct = 0;
static Mutex *mockChannel_globalMutex = 0;

static int64 mockChannel_getTotalConstruct()
{
    Lock xx(mockChannel_globalMutex);
    return mockChannel_totalConstruct;
}

static int64 mockChannel_getTotalDestruct()
{
    Lock xx(mockChannel_globalMutex);
    return mockChannel_totalDestruct;
}

static ConstructDestructCallback *mockChannel_pConstructDestructCallback;

static void mockChannel_init()
{
     static Mutex mutex = Mutex();
     Lock xx(&mutex);
     if(mockChannel_globalMutex==0) {
        mockChannel_globalMutex = new Mutex();
        mockChannel_pConstructDestructCallback = new ConstructDestructCallback(
            String("mockChannel"),
            mockChannel_getTotalConstruct,mockChannel_getTotalDestruct,0);
     }
}


class MockChannel : public Channel {
    private:
        ChannelProvider* m_provider;
        ChannelRequester* m_requester;
        String m_name;
        String m_remoteAddress;
        
    private:
    ~MockChannel()
    {
        Lock xx(mockChannel_globalMutex);
        mockChannel_totalDestruct++;
    }
    
    public:
    
    MockChannel(
        ChannelProvider* provider,
        ChannelRequester* requester,
        String name,
        String remoteAddress) :
        m_provider(provider),
        m_requester(requester),
        m_name(name),
        m_remoteAddress(remoteAddress)
    {
        mockChannel_init();   

        Lock xx(mockChannel_globalMutex);
        mockChannel_totalConstruct++;
        
        // already connected, report state
        m_requester->channelStateChange(this, CONNECTED);
    }
    
    virtual void destroy()
    {
        delete this;
    };

    virtual String getRequesterName()
    {
        return getChannelName();
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual ChannelProvider* getProvider() 
    {
        return m_provider;
    }

    virtual epics::pvData::String getRemoteAddress()
    {
        return m_remoteAddress;
    }

    virtual epics::pvData::String getChannelName()
    {
        return m_name;
    }

    virtual ChannelRequester* getChannelRequester()
    {
        return m_requester;
    }

    virtual ConnectionState getConnectionState()
    {
        return CONNECTED;
    }

    virtual bool isConnected()
    {
        return getConnectionState() == CONNECTED;
    }

    virtual AccessRights getAccessRights(epics::pvData::PVField *pvField)
    {
        return readWrite;
    }

    virtual void getField(GetFieldRequester *requester,epics::pvData::String subField)
    {
        // TODO 
    }

    virtual ChannelProcess* createChannelProcess(
            ChannelProcessRequester *channelProcessRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }

    virtual ChannelGet* createChannelGet(
            ChannelGetRequester *channelGetRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }

    virtual ChannelPut* createChannelPut(
            ChannelPutRequester *channelPutRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }

    virtual ChannelPutGet* createChannelPutGet(
            ChannelPutGetRequester *channelPutGetRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }
    
    virtual ChannelRPC* createChannelRPC(ChannelRPCRequester *channelRPCRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }

    virtual epics::pvData::Monitor* createMonitor(
            epics::pvData::MonitorRequester *monitorRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }

    virtual ChannelArray* createChannelArray(
            ChannelArrayRequester *channelArrayRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }
};


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
            Channel* channel = new MockChannel(this, channelRequester, channelName, address);
            channelRequester->channelCreated(getStatusCreate()->getStatusOK(), channel);
            return channel;
        }
        else
        {   
            Status* errorStatus = getStatusCreate()->createStatus(STATUSTYPE_ERROR, "only local supported", 0);
            channelRequester->channelCreated(errorStatus, 0);
            delete errorStatus; // TODO guard from CB
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
    
    virtual Version* getVersion() {
        return m_version;
    }

    virtual ChannelProvider* getProvider() {
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


class ChannelRequesterImpl : public ChannelRequester
{
    virtual String getRequesterName()
    {
        return "ChannelRequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelCreated(epics::pvData::Status* status, Channel *channel)
    {
        std::cout << "channelCreated(";
        String str;
        status->toString(&str);
        std::cout << str << ", " << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }
    
    virtual void channelStateChange(Channel *c, ConnectionState connectionState)
    {
        std::cout << "channelStateChange(" << c->getChannelName() << ", " << ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

int main(int argc,char *argv[])
{
    MockClientContext* context = new MockClientContext();
    context->printInfo();
    
    ChannelRequesterImpl requester;
    
    /*Channel* noChannel =*/ context->getProvider()->createChannel("test", &requester, ChannelProvider::PRIORITY_DEFAULT, "over the rainbow");

    Channel* channel = context->getProvider()->createChannel("test", &requester);
    std::cout << channel->getChannelName() << std::endl;
    channel->destroy();
    
    
    context->destroy();
    
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return(0);
}


