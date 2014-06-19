/* MockClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.18 */


#include <epicsExit.h>
#include <pv/pvAccess.h>
#include <iostream>
#include <pv/lock.h>
#include <pv/standardPVField.h>

using namespace epics::pvData;
using namespace epics::pvAccess;



PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelProcess);

class MockChannelProcess : public ChannelProcess
{
    private:
		ChannelProcessRequester* m_channelProcessRequester;
		PVStructure* m_pvStructure;
		PVScalar* m_valueField;
    
    private:
    ~MockChannelProcess()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelProcess);
    }

    public:
    MockChannelProcess(ChannelProcessRequester* channelProcessRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelProcess);

        PVField* field = pvStructure->getSubField(std::string("value"));
        if (field == 0)
        {
            Status noValueFieldStatus(Status::STATUSTYPE_ERROR, "no 'value' field");
        	m_channelProcessRequester->channelProcessConnect(noValueFieldStatus, this);
        	
        	// NOTE client must destroy this instance...
        	// do not access any fields and return ASAP
        	return;
        }
        
        if (field->getField()->getType() != scalar)
        {
            Status notAScalarStatus(Status::STATUSTYPE_ERROR, "'value' field not scalar type");
        	m_channelProcessRequester->channelProcessConnect(notAScalarStatus, this);
        	
        	// NOTE client must destroy this instance….
        	// do not access any fields and return ASAP
        	return;
        }
        
        m_valueField = static_cast<PVScalar*>(field);
        	
        // TODO pvRequest 
    	m_channelProcessRequester->channelProcessConnect(Status::OK, this);
    }
    
    virtual void process(bool lastRequest)
    {
        switch (m_valueField->getScalar()->getScalarType())
        {
            case pvBoolean:
            {
                // negate
                PVBoolean *pvBoolean = static_cast<PVBoolean*>(m_valueField);
                pvBoolean->put(!pvBoolean->get());
                break;
            }
            case pvByte:
            {
                // increment by one
                PVByte *pvByte = static_cast<PVByte*>(m_valueField);
                pvByte->put(pvByte->get() + 1);
                break;
            }
            case pvShort:
            {
                // increment by one
                PVShort *pvShort = static_cast<PVShort*>(m_valueField);
                pvShort->put(pvShort->get() + 1);
                break;
            }
            case pvInt:
            {
                // increment by one
                PVInt *pvInt = static_cast<PVInt*>(m_valueField);
                pvInt->put(pvInt->get() + 1);
                break;
            }
            case pvLong:
            {
                // increment by one
                PVLong *pvLong = static_cast<PVLong*>(m_valueField);
                pvLong->put(pvLong->get() + 1);
                break;
            }
            case pvFloat:
            {
                // increment by one
                PVFloat *pvFloat = static_cast<PVFloat*>(m_valueField);
                pvFloat->put(pvFloat->get() + 1.0f);
                break;
            }
            case pvDouble:
            {
                // increment by one
                PVDouble *pvDouble = static_cast<PVDouble*>(m_valueField);
                pvDouble->put(pvDouble->get() + 1.0);
                break;
            }
            case pvString:
            {
                // increment by one
                PVString *pvString = static_cast<PVString*>(m_valueField);
                string val = pvString->get();
                if (val.empty())
                    pvString->put("gen0");
                else
                {
                    char c = val[0];
                    c++;
                    pvString->put("gen" + c);
                }
                break;
            }
            default:
                // noop
                break;
            
        } 
    	m_channelProcessRequester->processDone(Status::OK);
    	
    	if (lastRequest)
    	   destroy();
    }
    
    virtual void destroy()
    {
        delete this;
    }
    
};






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelGet);

class MockChannelGet : public ChannelGet
{
    private:
		ChannelGetRequester* m_channelGetRequester;
		PVStructure* m_pvStructure;
		BitSet* m_bitSet;
		bool m_first;
    
    private:
    ~MockChannelGet()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelGet);
    }

    public:
    MockChannelGet(ChannelGetRequester* channelGetRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelGetRequester(channelGetRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelGet);

        // TODO pvRequest 
    	m_channelGetRequester->channelGetConnect(Status::OK, this, m_pvStructure, m_bitSet);
    }
    
    virtual void get(bool lastRequest)
    {
    	m_channelGetRequester->getDone(Status::OK);
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








PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelPut);

class MockChannelPut : public ChannelPut
{
    private:
		ChannelPutRequester* m_channelPutRequester;
		PVStructure* m_pvStructure;
		BitSet* m_bitSet;
		bool m_first;
    
    private:
    ~MockChannelPut()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelPut);
    }

    public:
    MockChannelPut(ChannelPutRequester* channelPutRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelPutRequester(channelPutRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPut);

        // TODO pvRequest 
    	m_channelPutRequester->channelPutConnect(Status::OK, this, m_pvStructure, m_bitSet);
    }
    
    virtual void put(bool lastRequest)
    {
    	m_channelPutRequester->putDone(Status::OK);
    	if (lastRequest)
    	   destroy();
    }
    
    virtual void get()
    {
    	m_channelPutRequester->getDone(Status::OK);
    }

    virtual void destroy()
    {
        delete m_bitSet;
        delete this;
    }
    
};







PVACCESS_REFCOUNT_MONITOR_DEFINE(mockMonitor);

class MockMonitor : public Monitor, public MonitorElement
{
    private:
		MonitorRequester* m_monitorRequester;
		PVStructure* m_pvStructure;
		BitSet* m_changedBitSet;
		BitSet* m_overrunBitSet;
		bool m_first;
		Mutex m_lock;
		int m_count;
    
    private:
    ~MockMonitor()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockMonitor);
    }

    public:
    MockMonitor(MonitorRequester* monitorRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_monitorRequester(monitorRequester), m_pvStructure(pvStructure),
        m_changedBitSet(new BitSet(pvStructure->getNumberFields())),
        m_overrunBitSet(new BitSet(pvStructure->getNumberFields())),
        m_first(true),
        m_lock(),
        m_count(0)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockMonitor);

        m_changedBitSet->set(0);
        
        // TODO pvRequest 
        m_monitorRequester->monitorConnect(Status::OK, this, m_pvStructure->getStructure());
    }
    
    virtual Status start()
    {
        // fist monitor
        m_monitorRequester->monitorEvent(this);
        
        return Status::OK;
    }

    virtual Status stop()
    {
        return Status::OK;
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
        stop();
        
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






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannel);

class MockChannel : public Channel {
    private:
        ChannelProvider* m_provider;
        ChannelRequester* m_requester;
        string m_name;
        string m_remoteAddress;
        
        PVStructure* m_pvStructure;
        
    private:
    ~MockChannel()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannel);
    }
    
    public:
    
    MockChannel(
        ChannelProvider* provider,
        ChannelRequester* requester,
        string name,
        string remoteAddress) :
        m_provider(provider),
        m_requester(requester),
        m_name(name),
        m_remoteAddress(remoteAddress)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannel);
     
     
        ScalarType stype = pvDouble;
        string allProperties("alarm,timeStamp,display,control,valueAlarm");

        m_pvStructure = getStandardPVField()->scalar(
            0,name,stype,allProperties);
        PVDouble *pvField = m_pvStructure->getDoubleField(std::string("value"));
        pvField->put(1.123);

        
        // already connected, report state
        m_requester->channelStateChange(this, CONNECTED);
    }
    
    virtual void destroy()
    {
        delete m_pvStructure;
        delete this;
    };

    virtual string getRequesterName()
    {
        return getChannelName();
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual ChannelProvider* getProvider() 
    {
        return m_provider;
    }

    virtual std::string getRemoteAddress()
    {
        return m_remoteAddress;
    }

    virtual std::string getChannelName()
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

    virtual void getField(GetFieldRequester *requester,std::string subField)
    {
        requester->getDone(Status::OK,m_pvStructure->getSubField(subField)->getField());
    }

    virtual ChannelProcess* createChannelProcess(
            ChannelProcessRequester *channelProcessRequester,
            epics::pvData::PVStructure *pvRequest)
    {
    	return new MockChannelProcess(channelProcessRequester, m_pvStructure, pvRequest);
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

    virtual void printInfo() {
        string info;
        printInfo(&info);
        std::cout << info.c_str() << std::endl;
    }
    
    virtual void printInfo(epics::pvData::StringBuilder out) {
        //std::ostringstream ostr;
        //static string emptyString;
        
		out->append(  "CHANNEL  : "); out->append(m_name);
		out->append("\nSTATE    : "); out->append(ConnectionStateNames[getConnectionState()]);
		if (isConnected())
		{
       		out->append("\nADDRESS  : "); out->append(getRemoteAddress());
			//out->append("\nRIGHTS   : "); out->append(getAccessRights());
		}
		out->append("\n");
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
    
    virtual void cancel()
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

    virtual std::string getProviderName()
    {
        return "MockChannelProvider";
    }
    
    virtual void destroy()
    {
        delete m_mockChannelFind;
        delete this;
    }
    
    virtual ChannelFind* channelFind(
        std::string channelName,
        ChannelFindRequester *channelFindRequester)
    {
        channelFindRequester->channelFindResult(Status::OK, m_mockChannelFind, true);
        return m_mockChannelFind;
    }

    virtual Channel* createChannel(
        std::string channelName,
        ChannelRequester *channelRequester,
        short priority)
    {
        return createChannel(channelName, channelRequester, priority, "local");
    }

    virtual Channel* createChannel(
        std::string channelName,
        ChannelRequester *channelRequester,
        short priority,
        std::string address)
    {
        if (address == "local")
        {
            Channel* channel = new MockChannel(this, channelRequester, channelName, address);
            channelRequester->channelCreated(Status::OK, channel);
            return channel;
        }
        else
        {   
            Status errorStatus(Status::STATUSTYPE_ERROR, "only local supported");
            channelRequester->channelCreated(errorStatus, 0);
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
    
    MockClientContext() : m_version(new Version("Mock PVA Client", "cpp", 1, 0, 0, 0))
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
        string info;
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
    virtual void channelFindResult(const epics::pvData::Status& status,ChannelFind *channelFind,bool wasFound)
    {
        std::cout << "[ChannelFindRequesterImpl] channelFindResult("
                  << status.toString() << ", ..., " << wasFound << ")" << std::endl; 
    }
};

class ChannelRequesterImpl : public ChannelRequester
{
    virtual string getRequesterName()
    {
        return "ChannelRequesterImpl";
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelCreated(const epics::pvData::Status& status, Channel *channel)
    {
        std::cout << "channelCreated(" << status.toString() << ", "
                  << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }
    
    virtual void channelStateChange(Channel *c, Channel::ConnectionState connectionState)
    {
        std::cout << "channelStateChange(" << c->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

class GetFieldRequesterImpl : public GetFieldRequester
{
    virtual string getRequesterName()
    {
        return "GetFieldRequesterImpl";
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void getDone(const epics::pvData::Status& status,epics::pvData::FieldConstPtr field)
    {
        std::cout << "getDone(" << status.toString() << ", ";
        if (field)
        {
            string str;
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
                
    virtual string getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,ChannelGet *channelGet,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelGetConnect(" << status.toString() << ")" << std::endl;
        
        // TODO sync
        m_channelGet = channelGet;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;
        string str;
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
                
    virtual string getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,ChannelPut *channelPut,
            epics::pvData::PVStructure *pvStructure,epics::pvData::BitSet *bitSet)
    {
        std::cout << "channelPutConnect(" << status.toString() << ")" << std::endl;
        
        // TODO sync
        m_channelPut = channelPut;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;
        string str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }

    virtual void putDone(const epics::pvData::Status& status)
    {
        std::cout << "putDone(" << status.toString() << ")" << std::endl;
        string str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }

};
 
 
class MonitorRequesterImpl : public MonitorRequester
{
    virtual string getRequesterName()
    {
        return "MonitorRequesterImpl";
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }
    
    virtual void monitorConnect(const Status& status, Monitor* monitor, StructureConstPtr structure)
    {
        std::cout << "monitorConnect(" << status.toString() << ")" << std::endl;
        if (structure)
        {
            string str;
            structure->toString(&str);
            std::cout << str << std::endl;
        }
    }
    
    virtual void monitorEvent(Monitor* monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement* element = monitor->poll();
        
        string str("changed/overrun ");
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


class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    ChannelProcess *m_channelProcess;
                
    virtual string getRequesterName()
    {
        return "ProcessRequesterImpl";
    };
    
    virtual void message(std::string const & message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelProcessConnect(const epics::pvData::Status& status,ChannelProcess *channelProcess)
    {
        std::cout << "channelProcessConnect(" << status.toString() << ")" << std::endl;
        
        // TODO sync
        m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& status)
    {
        std::cout << "processDone(" << status.toString() << ")" << std::endl;
    }

};

int main(int argc,char *argv[])
{
    MockClientContext* context = new MockClientContext();
    context->printInfo();
    
    
    ChannelFindRequesterImpl findRequester;
    context->getProvider()->channelFind("something", &findRequester);
    
    ChannelRequesterImpl channelRequester;
    //Channel* noChannel 
    context->getProvider()->createChannel("test", &channelRequester, ChannelProvider::PRIORITY_DEFAULT, "over the rainbow");

    Channel* channel = context->getProvider()->createChannel("test", &channelRequester);
    channel->printInfo();
    
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

    Status status = monitor->start();
    std::cout << "monitor->start() = " << status.toString() << std::endl;


    ChannelProcessRequesterImpl channelProcessRequester;
    ChannelProcess* channelProcess = channel->createChannelProcess(&channelProcessRequester, 0);
    channelProcess->process(false);
    channelProcess->destroy();
    

    status = monitor->stop();
    std::cout << "monitor->stop() = " << status.toString() << std::endl;
    
    
    monitor->destroy();

    channel->destroy();
    
    context->destroy();
    
    //std::cout << "-----------------------------------------------------------------------" << std::endl;
    //epicsExitCallAtExits();
    return(0);
}
