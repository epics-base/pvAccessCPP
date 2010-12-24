/* MockClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.18 */


#include <pvAccess.h>
#include <iostream>
#include <showConstructDestruct.h>
#include <lock.h>
#include <standardPVField.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

static volatile int64 mockChannelGet_totalConstruct = 0;
static volatile int64 mockChannelGet_totalDestruct = 0;
static Mutex *mockChannelGet_globalMutex = 0;

static int64 mockChannelGet_getTotalConstruct()
{
    Lock xx(mockChannelGet_globalMutex);
    return mockChannelGet_totalConstruct;
}

static int64 mockChannelGet_getTotalDestruct()
{
    Lock xx(mockChannelGet_globalMutex);
    return mockChannelGet_totalDestruct;
}

static ConstructDestructCallback *mockChannelGet_pConstructDestructCallback;

static void mockChannelGet_init()
{
     static Mutex mutex = Mutex();
     Lock xx(&mutex);
     if(mockChannelGet_globalMutex==0) {
        mockChannelGet_globalMutex = new Mutex();
        mockChannelGet_pConstructDestructCallback = new ConstructDestructCallback(
            String("mockChannelGet"),
            mockChannelGet_getTotalConstruct,mockChannelGet_getTotalDestruct,0);
     }
}

class MockChannelGet : public ChannelGet
{
    private:
		ChannelGetRequester* m_channelGetRequester;
		PVStructure* m_pvStructure;
		BitSet* m_bitSet;
		volatile bool m_first;
    
    private:
    ~MockChannelGet()
    {
        Lock xx(mockChannelGet_globalMutex);
        mockChannelGet_totalDestruct++;
    }

    public:
    MockChannelGet(ChannelGetRequester* channelGetRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelGetRequester(channelGetRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        mockChannelGet_init();   

        Lock xx(mockChannelGet_globalMutex);
        mockChannelGet_totalConstruct++;

        // TODO pvRequest 
    	m_channelGetRequester->channelGetConnect(getStatusCreate()->getStatusOK(), this, m_pvStructure, m_bitSet);
    }
    
    virtual void get(bool lastRequest)
    {
    	m_channelGetRequester->getDone(getStatusCreate()->getStatusOK());
    	if (m_first)
    	{
    		m_first = false;
    		m_bitSet->set(0);  // TODO
    	}
    	
    	if (lastRequest)
    	   destroy();
    }
    
    virtual void destroy()
    {
        delete m_bitSet;
        delete this;
    }
    
};










static volatile int64 mockChannelPut_totalConstruct = 0;
static volatile int64 mockChannelPut_totalDestruct = 0;
static Mutex *mockChannelPut_globalMutex = 0;

static int64 mockChannelPut_getTotalConstruct()
{
    Lock xx(mockChannelPut_globalMutex);
    return mockChannelPut_totalConstruct;
}

static int64 mockChannelPut_getTotalDestruct()
{
    Lock xx(mockChannelPut_globalMutex);
    return mockChannelPut_totalDestruct;
}

static ConstructDestructCallback *mockChannelPut_pConstructDestructCallback;

static void mockChannelPut_init()
{
     static Mutex mutex = Mutex();
     Lock xx(&mutex);
     if(mockChannelPut_globalMutex==0) {
        mockChannelPut_globalMutex = new Mutex();
        mockChannelPut_pConstructDestructCallback = new ConstructDestructCallback(
            String("mockChannelPut"),
            mockChannelPut_getTotalConstruct,mockChannelPut_getTotalDestruct,0);
     }
}

class MockChannelPut : public ChannelPut
{
    private:
		ChannelPutRequester* m_channelPutRequester;
		PVStructure* m_pvStructure;
		BitSet* m_bitSet;
		volatile bool m_first;
    
    private:
    ~MockChannelPut()
    {
        Lock xx(mockChannelPut_globalMutex);
        mockChannelPut_totalDestruct++;
    }

    public:
    MockChannelPut(ChannelPutRequester* channelPutRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelPutRequester(channelPutRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        mockChannelPut_init();   

        Lock xx(mockChannelPut_globalMutex);
        mockChannelPut_totalConstruct++;

        // TODO pvRequest 
    	m_channelPutRequester->channelPutConnect(getStatusCreate()->getStatusOK(), this, m_pvStructure, m_bitSet);
    }
    
    virtual void put(bool lastRequest)
    {
    	m_channelPutRequester->putDone(getStatusCreate()->getStatusOK());
    	if (lastRequest)
    	   destroy();
    }
    
    virtual void get()
    {
    	m_channelPutRequester->getDone(getStatusCreate()->getStatusOK());
    }

    virtual void destroy()
    {
        delete m_bitSet;
        delete this;
    }
    
};








static volatile int64 mockMonitor_totalConstruct = 0;
static volatile int64 mockMonitor_totalDestruct = 0;
static Mutex *mockMonitor_globalMutex = 0;

static int64 mockMonitor_getTotalConstruct()
{
    Lock xx(mockMonitor_globalMutex);
    return mockMonitor_totalConstruct;
}

static int64 mockMonitor_getTotalDestruct()
{
    Lock xx(mockMonitor_globalMutex);
    return mockMonitor_totalDestruct;
}

static ConstructDestructCallback *mockMonitor_pConstructDestructCallback;

static void mockMonitor_init()
{
     static Mutex mutex = Mutex();
     Lock xx(&mutex);
     if(mockMonitor_globalMutex==0) {
        mockMonitor_globalMutex = new Mutex();
        mockMonitor_pConstructDestructCallback = new ConstructDestructCallback(
            String("mockMonitor"),
            mockMonitor_getTotalConstruct,mockMonitor_getTotalDestruct,0);
     }
}





class MockMonitor : public Monitor, public MonitorElement
{
    private:
		MonitorRequester* m_monitorRequester;
		PVStructure* m_pvStructure;
		BitSet* m_changedBitSet;
		BitSet* m_overrunBitSet;
		volatile bool m_first;
		Mutex* m_lock;
		volatile int m_count;
    
    private:
    ~MockMonitor()
    {
        Lock xx(mockMonitor_globalMutex);
        mockMonitor_totalDestruct++;
    }

    public:
    MockMonitor(MonitorRequester* monitorRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_monitorRequester(monitorRequester), m_pvStructure(pvStructure),
        m_changedBitSet(new BitSet(pvStructure->getNumberFields())),
        m_overrunBitSet(new BitSet(pvStructure->getNumberFields())),
        m_first(true),
        m_lock(new Mutex()),
        m_count(0)
    {
        mockMonitor_init();   

        Lock xx(mockMonitor_globalMutex);
        mockMonitor_totalConstruct++;

        m_changedBitSet->set(0);
        
        // TODO pvRequest 
    	m_monitorRequester->monitorConnect(getStatusCreate()->getStatusOK(), this, const_cast<Structure*>(m_pvStructure->getStructure()));
    }
    
    virtual Status* start()
    {
        // fist monitor
        m_monitorRequester->monitorEvent(this);
        
        // client needs to delete status, so passing shared OK instance is not right thing to do
        return getStatusCreate()->createStatus(STATUSTYPE_OK, "Monitor started.");
    }

    virtual Status* stop()
    {
        // client needs to delete status, so passing shared OK instance is not right thing to do
        return getStatusCreate()->createStatus(STATUSTYPE_OK, "Monitor stopped.");
    }

    virtual MonitorElement* poll()
    {
        Lock xx(m_lock);
        if (m_count)
        {
            return 0;
        }
        else
        {
            m_count++;
            return this;
        }
    }

    virtual void release(MonitorElement* monitorElement)
    {
        Lock xx(m_lock);
        if (m_count)
            m_count--;
    }
    
    virtual void destroy()
    {
        delete stop();
        
        delete m_lock;
        delete m_overrunBitSet;
        delete m_changedBitSet;
        delete this;
    }
    
    // ============ MonitorElement ============
    
    virtual PVStructure* getPVStructure()
    {
        return m_pvStructure;
    }
    
    virtual BitSet* getChangedBitSet()
    {
        return m_changedBitSet;
    }

    virtual BitSet* getOverrunBitSet()
    {
        return m_overrunBitSet;
    }
    
    
};







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
        
        PVStructure* m_pvStructure;
        
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
     
     
        ScalarType stype = pvDouble;
        String allProperties("alarm,timeStamp,display,control,valueAlarm");

        m_pvStructure = getStandardPVField()->scalar(
            0,name,stype,allProperties);
        PVDouble *pvField = m_pvStructure->getDoubleField(String("value"));
        pvField->put(1.123e35);

        
        // already connected, report state
        m_requester->channelStateChange(this, CONNECTED);
    }
    
    virtual void destroy()
    {
        delete m_pvStructure;
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
        requester->getDone(getStatusCreate()->getStatusOK(),m_pvStructure->getSubField(subField)->getField());
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
    	return new MockChannelGet(channelGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPut* createChannelPut(
            ChannelPutRequester *channelPutRequester,
            epics::pvData::PVStructure *pvRequest)
    {
    	return new MockChannelPut(channelPutRequester, m_pvStructure, pvRequest);
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
    	return new MockMonitor(monitorRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelArray* createChannelArray(
            ChannelArrayRequester *channelArrayRequester,
            epics::pvData::PVStructure *pvRequest)
    {
        // TODO
        return 0;
    }
};

class MockChannelProvider;

class MockChannelFind : public ChannelFind
{
    public:
    MockChannelFind(ChannelProvider* provider) : m_provider(provider)
    {
    }

    virtual void destroy()
    {
        // one instance for all, do not delete at all
    }
   
    virtual ChannelProvider* getChannelProvider()
    {
        return m_provider;
    };
    
    virtual void cancelChannelFind()
    {
        throw std::runtime_error("not supported");
    }
    
    private:
    
    // only to be destroyed by it
    friend class MockChannelProvider;
    virtual ~MockChannelFind() {}
    
    ChannelProvider* m_provider;  
};

class MockChannelProvider : public ChannelProvider {
    public:

    MockChannelProvider() : m_mockChannelFind(new MockChannelFind(this)) {
    }

    virtual epics::pvData::String getProviderName()
    {
        return "MockChannelProvider";
    }
    
    virtual void destroy()
    {
        delete m_mockChannelFind;
        delete this;
    }
    
    virtual ChannelFind* channelFind(
        epics::pvData::String channelName,
        ChannelFindRequester *channelFindRequester)
    {
        channelFindRequester->channelFindResult(getStatusCreate()->getStatusOK(), m_mockChannelFind, true);
        return m_mockChannelFind;
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
    
    MockChannelFind* m_mockChannelFind;
    
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


class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(epics::pvData::Status *status,ChannelFind *channelFind,bool wasFound)
    {
        std::cout << "[ChannelFindRequesterImpl] channelFindResult("
                  << status->toString() << ", ..., " << wasFound << ")" << std::endl; 
    }
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
        std::cout << "channelCreated(" << status->toString() << ", "
                  << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }
    
    virtual void channelStateChange(Channel *c, ConnectionState connectionState)
    {
        std::cout << "channelStateChange(" << c->getChannelName() << ", " << ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

class GetFieldRequesterImpl : public GetFieldRequester
{
    virtual String getRequesterName()
    {
        return "GetFieldRequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void getDone(epics::pvData::Status *status,epics::pvData::FieldConstPtr field)
    {
        std::cout << "getDone(" << status->toString() << ", ";
        if (field)
        {
            String str;
            field->toString(&str);
            std::cout << str;
        }
        else
            std::cout << "(null)";
        std::cout << ")" << std::endl;
    }
};

class ChannelGetRequesterImpl : public ChannelGetRequester
{
    ChannelGet *m_channelGet;
    epics::pvData::PVStructure *m_pvStructure;
    epics::pvData::BitSet *m_bitSet;
                
    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelGetConnect(epics::pvData::Status *status,ChannelGet *channelGet,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelGetConnect(" << status->toString() << ")" << std::endl;
        
        // TODO sync
        m_channelGet = channelGet;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(epics::pvData::Status *status)
    {
        std::cout << "getDone(" << status->toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
    ChannelPut *m_channelPut;
    epics::pvData::PVStructure *m_pvStructure;
    epics::pvData::BitSet *m_bitSet;
                
    virtual String getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelPutConnect(epics::pvData::Status *status,ChannelPut *channelPut,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelPutConnect(" << status->toString() << ")" << std::endl;
        
        // TODO sync
        m_channelPut = channelPut;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(epics::pvData::Status *status)
    {
        std::cout << "getDone(" << status->toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }

    virtual void putDone(epics::pvData::Status *status)
    {
        std::cout << "putDone(" << status->toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }

};
 
 
class MonitorRequesterImpl : public MonitorRequester
{
    virtual String getRequesterName()
    {
        return "MonitorRequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }
    
    virtual void monitorConnect(Status* status, Monitor* monitor, Structure* structure)
    {
        std::cout << "monitorConnect(" << status->toString() << ")" << std::endl;
        if (structure)
        {
            String str;
            structure->toString(&str);
            std::cout << str << std::endl;
        }
    }
    
    virtual void monitorEvent(Monitor* monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement* element = monitor->poll();
        
        String str("changed/overrun ");
        element->getChangedBitSet()->toString(&str);
        str += '/';
        element->getOverrunBitSet()->toString(&str);
        str += '\n';
        element->getPVStructure()->toString(&str);
        std::cout << str << std::endl;
        
        monitor->release(element);
    }
    
    virtual void unlisten(Monitor* monitor)
    {
        std::cout << "unlisten" << std::endl;
    }
}; 


int main(int argc,char *argv[])
{
    MockClientContext* context = new MockClientContext();
    context->printInfo();
    
    
    ChannelFindRequesterImpl findRequester;
    context->getProvider()->channelFind("something", &findRequester);
    
    ChannelRequesterImpl channelRequester;
    /*Channel* noChannel =*/ context->getProvider()->createChannel("test", &channelRequester, ChannelProvider::PRIORITY_DEFAULT, "over the rainbow");

    Channel* channel = context->getProvider()->createChannel("test", &channelRequester);
    std::cout << channel->getChannelName() << std::endl;
    
    GetFieldRequesterImpl getFieldRequesterImpl;
    channel->getField(&getFieldRequesterImpl, "timeStamp.secondsPastEpoch");
    
    ChannelGetRequesterImpl channelGetRequesterImpl;
    ChannelGet* channelGet = channel->createChannelGet(&channelGetRequesterImpl, 0);
    channelGet->get(false);
    channelGet->destroy();
    
    ChannelPutRequesterImpl channelPutRequesterImpl;
    ChannelPut* channelPut = channel->createChannelPut(&channelPutRequesterImpl, 0);
    channelPut->get();
    channelPut->put(false);
    channelPut->destroy();
    
    
    MonitorRequesterImpl monitorRequesterImpl;
    Monitor* monitor = channel->createMonitor(&monitorRequesterImpl, 0);

    Status* status = monitor->start();
    std::cout << "monitor->start() = " << status->toString() << std::endl;
    delete status;

    status = monitor->stop();
    std::cout << "monitor->stop() = " << status->toString() << std::endl;
    delete status;
    
    monitor->destroy();

    channel->destroy();
    
    context->destroy();
    
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return(0);
}


