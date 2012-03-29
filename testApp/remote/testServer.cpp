/*
 * testServer.cpp
 */

#include <pv/serverContext.h>
#include <pv/clientContextImpl.h>
#include <pv/CDRMonitor.h>
#include <epicsExit.h>
#include <pv/standardPVField.h>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;
using std::tr1::static_pointer_cast;


PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelProcess);

class MockChannelProcess :
	public ChannelProcess,
	public std::tr1::enable_shared_from_this<MockChannelProcess>
{
    private:
		ChannelProcessRequester::shared_pointer m_channelProcessRequester;
		PVStructure::shared_pointer m_pvStructure;
		PVScalar* m_valueField;

    protected:
    MockChannelProcess(ChannelProcessRequester::shared_pointer const & channelProcessRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelProcess);

        ChannelProcess::shared_pointer thisPtr; // we return null  = static_pointer_cast<ChannelProcess>(shared_from_this());
        PVField* field = pvStructure->getSubField(String("value"));
        if (field == 0)
        {
            Status noValueFieldStatus(Status::STATUSTYPE_ERROR, "no 'value' field");
        	m_channelProcessRequester->channelProcessConnect(noValueFieldStatus, thisPtr);
        	// NOTE client must destroy this instance...
        	// do not access any fields and return ASAP
        	return;
        }

        if (field->getField()->getType() != scalar)
        {
            Status notAScalarStatus(Status::STATUSTYPE_ERROR, "'value' field not scalar type");
        	m_channelProcessRequester->channelProcessConnect(notAScalarStatus, thisPtr);
        	// NOTE client must destroy this instance...
        	// do not access any fields and return ASAP
        	return;
        }

        m_valueField = static_cast<PVScalar*>(field);
    }

    public:
    static ChannelProcess::shared_pointer create(ChannelProcessRequester::shared_pointer const & channelProcessRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelProcess::shared_pointer thisPtr(new MockChannelProcess(channelProcessRequester, pvStructure, pvRequest));

        // TODO pvRequest
    	channelProcessRequester->channelProcessConnect(Status::Ok, thisPtr);
    	
    	return thisPtr;
    }

    virtual ~MockChannelProcess()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelProcess);
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
                String val = pvString->get();
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
    	m_channelProcessRequester->processDone(Status::Ok);

    	if (lastRequest)
    	   destroy();
    }

    virtual void destroy()
    {
    }
    
    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};






PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelGet);

class MockChannelGet : public ChannelGet
{
    private:
		ChannelGetRequester::shared_pointer m_channelGetRequester;
		PVStructure::shared_pointer m_pvStructure;
		BitSet::shared_pointer m_bitSet;
		bool m_first;

    protected:
    MockChannelGet(ChannelGetRequester::shared_pointer const & channelGetRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelGetRequester(channelGetRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelGet);
    }

    public:
    static ChannelGet::shared_pointer create(ChannelGetRequester::shared_pointer const & channelGetRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelGet::shared_pointer thisPtr(new MockChannelGet(channelGetRequester, pvStructure, pvRequest));
        // TODO pvRequest
    	channelGetRequester->channelGetConnect(Status::Ok, thisPtr, pvStructure, static_cast<MockChannelGet*>(thisPtr.get())->m_bitSet);
    	
    	return thisPtr;
    }

    virtual ~MockChannelGet()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelGet);
    }

    virtual void get(bool lastRequest)
    {
    	m_channelGetRequester->getDone(Status::Ok);
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
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};




PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelPut);

class MockChannelPut : public ChannelPut
{
    private:
		ChannelPutRequester::shared_pointer m_channelPutRequester;
		PVStructure::shared_pointer m_pvStructure;
		BitSet::shared_pointer m_bitSet;

    protected:
    MockChannelPut(ChannelPutRequester::shared_pointer const & channelPutRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelPutRequester(channelPutRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields()))
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPut);
    }

    public:
    static ChannelPut::shared_pointer create(ChannelPutRequester::shared_pointer const & channelPutRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPut::shared_pointer thisPtr(new MockChannelPut(channelPutRequester, pvStructure, pvRequest));
        // TODO pvRequest
    	channelPutRequester->channelPutConnect(Status::Ok, thisPtr, pvStructure, static_cast<MockChannelPut*>(thisPtr.get())->m_bitSet);
    	
    	return thisPtr;
    }

    virtual ~MockChannelPut()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelPut);
    }


    virtual void put(bool lastRequest)
    {
    	m_channelPutRequester->putDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void get()
    {
    	m_channelPutRequester->getDone(Status::Ok);
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};




PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelPutGet);

class MockChannelPutGet : public ChannelPutGet
{
    private:
		ChannelPutGetRequester::shared_pointer m_channelPutGetRequester;
		PVStructure::shared_pointer m_pvStructure;

    protected:
    MockChannelPutGet(ChannelPutGetRequester::shared_pointer const & channelPutGetRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelPutGetRequester(channelPutGetRequester), m_pvStructure(pvStructure)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPutGet);
    }

    public:
    static ChannelPutGet::shared_pointer create(ChannelPutGetRequester::shared_pointer const & channelPutGetRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPutGet::shared_pointer thisPtr(new MockChannelPutGet(channelPutGetRequester, pvStructure, pvRequest));
        // TODO pvRequest
    	channelPutGetRequester->channelPutGetConnect(Status::Ok, thisPtr, pvStructure, pvStructure);
    	
    	return thisPtr;
    }

    virtual ~MockChannelPutGet()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelPutGet);
    }

    virtual void putGet(bool lastRequest)
    {
    	m_channelPutGetRequester->putGetDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void getGet()
    {
    	m_channelPutGetRequester->getGetDone(Status::Ok);
    }

    virtual void getPut()
    {
    	m_channelPutGetRequester->getPutDone(Status::Ok);
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};






PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelRPC);

class MockChannelRPC : public ChannelRPC
{
    private:
		ChannelRPCRequester::shared_pointer m_channelRPCRequester;
		PVStructure::shared_pointer m_pvStructure;

    protected:
    MockChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelRPCRequester(channelRPCRequester), m_pvStructure(pvStructure)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelRPC);
    }

    public:
    static ChannelRPC::shared_pointer create(ChannelRPCRequester::shared_pointer const & channelRPCRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelRPC::shared_pointer thisPtr(new MockChannelRPC(channelRPCRequester, pvStructure, pvRequest));
        // TODO pvRequest
    	channelRPCRequester->channelRPCConnect(Status::Ok, thisPtr);
    	return thisPtr;
    }

    virtual ~MockChannelRPC()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelRPC);
    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument, bool lastRequest)
    {
        std::string s;
        pvArgument->toString(&s);
        std::cout << "RPC" << std::endl << s << std::endl;
    	m_channelRPCRequester->requestDone(Status::Ok, m_pvStructure);
    	if (lastRequest)
    	   destroy();
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};










PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelArray);

class MockChannelArray : public ChannelArray
{
    private:
		ChannelArrayRequester::shared_pointer m_channelArrayRequester;
		PVArray::shared_pointer m_pvArray;

    protected:
    MockChannelArray(ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelArrayRequester(channelArrayRequester),
        m_pvArray(getPVDataCreate()->createPVScalarArray(0, "", pvDouble))
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelArray);
    }

    public:
    static ChannelArray::shared_pointer create(ChannelArrayRequester::shared_pointer const & channelArrayRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelArray::shared_pointer thisPtr(new MockChannelArray(channelArrayRequester, pvStructure, pvRequest));

        // TODO pvRequest
    	channelArrayRequester->channelArrayConnect(Status::Ok, thisPtr, static_cast<MockChannelArray*>(thisPtr.get())->m_pvArray);
    	
    	return thisPtr;
    }

    virtual ~MockChannelArray()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelArray);
    }

    virtual void putArray(bool lastRequest, int offset, int count)
    {
        // TODO offset, count
    	m_channelArrayRequester->putArrayDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void getArray(bool lastRequest, int offset, int count)
    {
        // TODO offset, count
    	m_channelArrayRequester->getArrayDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void setLength(bool lastRequest, int length, int capacity)
    {
        // TODO offset, capacity
    	m_channelArrayRequester->setLengthDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void destroy()
    {
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }
};






PVDATA_REFCOUNT_MONITOR_DEFINE(mockMonitor);

class MockMonitor : public Monitor, public MonitorElement, public std::tr1::enable_shared_from_this<MockMonitor>
{
    private:
		MonitorRequester::shared_pointer m_monitorRequester;
		PVStructure::shared_pointer m_pvStructure;
		BitSet::shared_pointer m_changedBitSet;
		BitSet::shared_pointer m_overrunBitSet;
		bool m_first;
		Mutex m_lock;
		int m_count;
		
		MonitorElement::shared_pointer m_thisPtr;
		MonitorElement::shared_pointer m_nullMonitor;

    protected:
    MockMonitor(MonitorRequester::shared_pointer const & monitorRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_monitorRequester(monitorRequester), m_pvStructure(pvStructure),
        m_changedBitSet(new BitSet(pvStructure->getNumberFields())),
        m_overrunBitSet(new BitSet(pvStructure->getNumberFields())),
        m_first(true),
        m_lock(),
        m_count(0)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockMonitor);

        m_changedBitSet->set(0);
    }

    public:
    static Monitor::shared_pointer create(MonitorRequester::shared_pointer const & monitorRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        Monitor::shared_pointer thisPtr(new MockMonitor(monitorRequester, pvStructure, pvRequest));

        // TODO pvRequest
        StructureConstPtr structurePtr = static_cast<MockMonitor*>(thisPtr.get())->m_pvStructure->getStructure();
        monitorRequester->monitorConnect(Status::Ok, thisPtr, structurePtr);
        
        return thisPtr;
    }

    virtual ~MockMonitor()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockMonitor);
    }

    virtual Status start()
    {
        // first monitor
    	Monitor::shared_pointer thisPtr = shared_from_this();
        m_monitorRequester->monitorEvent(thisPtr);

        return Status::Ok;
    }

    virtual Status stop()
    {
        return Status::Ok;
    }

    virtual MonitorElement::shared_pointer const & poll()
    {
        Lock xx(m_lock);
        if (m_count)
        {
            return m_nullMonitor;
        }
        else
        {
            m_count++;
            m_thisPtr = shared_from_this();
            return m_thisPtr;
        }
    }

    virtual void release(MonitorElement::shared_pointer const & monitorElement)
    {
        Lock xx(m_lock);
        if (m_count)
        {
            m_thisPtr.reset();
            m_count--;
        }
    }

    virtual void destroy()
    {
        m_thisPtr.reset();
        stop();
    }

    virtual void lock()
    {
        // TODO !!!
    }

    virtual void unlock()
    {
        // TODO !!!
    }

    // ============ MonitorElement ============

    virtual PVStructure::shared_pointer const & getPVStructure()
    {
        return m_pvStructure;
    }

    virtual BitSet::shared_pointer const & getChangedBitSet()
    {
        return m_changedBitSet;
    }

    virtual BitSet::shared_pointer const & getOverrunBitSet()
    {
        return m_overrunBitSet;
    }


};


PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannel);

class MockChannel : public Channel {
    private:
        ChannelProvider::shared_pointer  m_provider;
        ChannelRequester::shared_pointer m_requester;
        String m_name;
        String m_remoteAddress;
        PVStructure::shared_pointer m_pvStructure;

    protected:

    MockChannel(
    	ChannelProvider::shared_pointer provider,
    	ChannelRequester::shared_pointer requester,
        String name,
        String remoteAddress) :
        m_provider(provider),
        m_requester(requester),
        m_name(name),
        m_remoteAddress(remoteAddress),
        m_pvStructure()
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannel);



        if (m_name.find("array") == 0)
        {
            String allProperties("alarm,timeStamp,display,control");
            m_pvStructure.reset(getStandardPVField()->scalarArray(0,name,pvDouble,allProperties));
            PVDoubleArray *pvField = static_cast<PVDoubleArray*>(m_pvStructure->getScalarArrayField(String("value"), pvDouble));
            int v = 0;
            int ix = 0;
            const int COUNT = 1000;
            
            pvField->setCapacity(1000*COUNT);
            for (int n = 0; n < 1000; n++)
            {
            
                double array[COUNT];
                for (int i = 0; i < COUNT; i++)
                {
                    array[i] = v; v+=1.1;
                }
                pvField->put(ix, COUNT, array, 0);
                ix += COUNT;
            }
            /*
            printf("array prepared------------------------------------!!!\n");
            String str;
            pvField->toString(&str);
            printf("%s\n", str.c_str());
            printf("=============------------------------------------!!!\n");
            */   
        }
        else
        {
            String allProperties("alarm,timeStamp,display,control,valueAlarm");
            m_pvStructure.reset(getStandardPVField()->scalar(0,name,pvDouble,allProperties));
            PVDouble *pvField = m_pvStructure->getDoubleField(String("value"));
            pvField->put(1.123);
        }
    }
    
    public:

    static Channel::shared_pointer create(
    	ChannelProvider::shared_pointer provider,
    	ChannelRequester::shared_pointer requester,
        String name,
        String remoteAddress)
    {
        Channel::shared_pointer channelPtr(new MockChannel(provider, requester, name, remoteAddress));

        // already connected, report state
        requester->channelStateChange(channelPtr, CONNECTED);
        
        return channelPtr;
    }    

    virtual ~MockChannel()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannel);
    }

    virtual void destroy()
    {
    };

    virtual String getRequesterName()
    {
        return getChannelName();
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual ChannelProvider::shared_pointer const & getProvider()
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

    virtual  std::tr1::shared_ptr<ChannelRequester> const & getChannelRequester()
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

    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & pvField)
    {
        return readWrite;
    }

    virtual void getField(GetFieldRequester::shared_pointer const & requester,epics::pvData::String subField)
    {
    	PVFieldPtr pvField;
    	if(subField == "")
    	{
    		pvField = m_pvStructure.get();
    	}
    	else
    	{
    		pvField = m_pvStructure->getSubField(subField);
    	}

    	if(pvField == NULL)
    	{
    		string errMsg = "field '" + subField + "' not found";
    		FieldConstPtr nullPtr;
    		Status errorStatus(Status::STATUSTYPE_ERROR, errMsg);
                requester->getDone(errorStatus,nullPtr);
    		return;
    	}
    	FieldConstPtr fieldPtr = pvField->getField();
    	requester->getDone(Status::Ok, fieldPtr);
    }

    virtual ChannelProcess::shared_pointer createChannelProcess(
    		ChannelProcessRequester::shared_pointer const & channelProcessRequester,
    		epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelProcess::create(channelProcessRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelGet::shared_pointer createChannelGet(
    		ChannelGetRequester::shared_pointer const & channelGetRequester,
    		epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelGet::create(channelGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPut::shared_pointer createChannelPut(
    		ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelPut::create(channelPutRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
    		ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelPutGet::create(channelPutGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelRPC::shared_pointer createChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
    		epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelRPC::create(channelRPCRequester, m_pvStructure, pvRequest);
    }

    virtual epics::pvData::Monitor::shared_pointer createMonitor(
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockMonitor::create(monitorRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelArray::shared_pointer createChannelArray(
            ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelArray::create(channelArrayRequester, m_pvStructure, pvRequest);
    }

    virtual void printInfo() {
        String info;
        printInfo(&info);
        std::cout << info.c_str() << std::endl;
    }

    virtual void printInfo(epics::pvData::StringBuilder out) {
        //std::ostringstream ostr;
        //static String emptyString;

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


class MockServerChannelProvider;

class MockChannelFind : public ChannelFind
{
    public:
    typedef std::tr1::shared_ptr<MockChannelFind> shared_pointer;
    typedef std::tr1::shared_ptr<const MockChannelFind> const_shared_pointer;

    MockChannelFind(ChannelProvider::shared_pointer &provider) : m_provider(provider)
    {
    }

    virtual ~MockChannelFind() {}

    virtual void destroy()
    {
        // one instance for all, do not delete at all
    }

    virtual ChannelProvider::shared_pointer getChannelProvider()
    {
        return m_provider.lock();
    };

    virtual void cancelChannelFind()
    {
        throw std::runtime_error("not supported");
    }

    private:
       ChannelProvider::weak_pointer m_provider;
};


class MockServerChannelProvider : 	public ChannelProvider,
									public std::tr1::enable_shared_from_this<MockServerChannelProvider>
{
    public:
    typedef std::tr1::shared_ptr<MockServerChannelProvider> shared_pointer;
    typedef std::tr1::shared_ptr<const MockServerChannelProvider> const_shared_pointer;

	MockServerChannelProvider() : m_mockChannelFind()
	{
    }

	void initialize()
	{
		ChannelProvider::shared_pointer chProviderPtr = shared_from_this();
		m_mockChannelFind.reset(new MockChannelFind(chProviderPtr));
	}

    virtual epics::pvData::String getProviderName()
    {
        return "local";
    }

    virtual void destroy()
    {
    }

    virtual ChannelFind::shared_pointer channelFind(
        epics::pvData::String channelName,
        ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        // channel always exists
        channelFindRequester->channelFindResult(Status::Ok, m_mockChannelFind, true);
        return m_mockChannelFind;
    }

    virtual Channel::shared_pointer createChannel(
        epics::pvData::String channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority)
    {
        return createChannel(channelName, channelRequester, priority, "local");
    }

    virtual Channel::shared_pointer createChannel(
        epics::pvData::String channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority,
        epics::pvData::String address)
    {
        if (address == "local")
        {
        	ChannelProvider::shared_pointer chProviderPtr = shared_from_this();
        	Channel::shared_pointer channel = MockChannel::create(chProviderPtr, channelRequester, channelName, address);
            channelRequester->channelCreated(Status::Ok, channel);
            return channel;
        }
        else
        {
        	Channel::shared_pointer nullPtr;
            Status errorStatus(Status::STATUSTYPE_ERROR, "only local supported");
            channelRequester->channelCreated(errorStatus, nullPtr);
            return nullPtr;
        }
    }
    private:

    ChannelFind::shared_pointer m_mockChannelFind;
};


class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(const epics::pvData::Status& status,ChannelFind::shared_pointer &channelFind,bool wasFound)
    {
        std::cout << "[ChannelFindRequesterImpl] channelFindResult("
                  << status.toString() << ", ..., " << wasFound << ")" << std::endl;
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

    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer &channel)
    {
        std::cout << "channelCreated(" << status.toString() << ", "
                  << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel::shared_pointer &channel, Channel::ConnectionState connectionState)
    {
        std::cout << "channelStateChange(" << channel->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
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

    virtual void getDone(const epics::pvData::Status& status,epics::pvData::FieldConstPtr field)
    {
        std::cout << "getDone(" << status.toString() << ", ";
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
    //TODO weak ChannelGet::shared_pointer m_channelGet;
    epics::pvData::PVStructure::shared_pointer m_pvStructure;
    epics::pvData::BitSet::shared_pointer m_bitSet;

    virtual String getRequesterName()
    {
        return "ChannelGetRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status, ChannelGet::shared_pointer const & channelGet,
    		 epics::pvData::PVStructure::shared_pointer const & pvStructure, epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        std::cout << "channelGetConnect(" << status.toString() << ")" << std::endl;

        //m_channelGet = channelGet;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }
};

class ChannelPutRequesterImpl : public ChannelPutRequester
{
    //TODO weak ChannelPut::shared_pointer m_channelPut;
    epics::pvData::PVStructure::shared_pointer m_pvStructure;
    epics::pvData::BitSet::shared_pointer m_bitSet;

    virtual String getRequesterName()
    {
        return "ChannelPutRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,ChannelPut::shared_pointer const & channelPut,
    		epics::pvData::PVStructure::shared_pointer const & pvStructure, epics::pvData::BitSet::shared_pointer const & bitSet)
    {
        std::cout << "channelPutConnect(" << status.toString() << ")" << std::endl;

        //m_channelPut = channelPut;
        m_pvStructure = pvStructure;
        m_bitSet = bitSet;
    }

    virtual void getDone(const epics::pvData::Status& status)
    {
        std::cout << "getDone(" << status.toString() << ")" << std::endl;
        String str;
        m_pvStructure->toString(&str);
        std::cout << str;
        std::cout << std::endl;
    }

    virtual void putDone(const epics::pvData::Status& status)
    {
        std::cout << "putDone(" << status.toString() << ")" << std::endl;
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

    virtual void monitorConnect(const Status& status, Monitor::shared_pointer const & monitor, StructureConstPtr& structure)
    {
        std::cout << "monitorConnect(" << status.toString() << ")" << std::endl;
        if (structure)
        {
            String str;
            structure->toString(&str);
            std::cout << str << std::endl;
        }
    }

    virtual void monitorEvent(Monitor::shared_pointer const & monitor)
    {
        std::cout << "monitorEvent" << std::endl;

        MonitorElement::shared_pointer  element = monitor->poll();

        String str("changed/overrun ");
        element->getChangedBitSet()->toString(&str);
        str += '/';
        element->getOverrunBitSet()->toString(&str);
        str += '\n';
        element->getPVStructure()->toString(&str);
        std::cout << str << std::endl;

        monitor->release(element);
    }

    virtual void unlisten(Monitor::shared_pointer const & monitor)
    {
        std::cout << "unlisten" << std::endl;
    }
};


class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    //TODO weak ChannelProcess::shared_pointer m_channelProcess;

    virtual String getRequesterName()
    {
        return "ProcessRequesterImpl";
    };

    virtual void message(String message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl;
    }

    virtual void channelProcessConnect(const epics::pvData::Status& status,ChannelProcess::shared_pointer const & channelProcess)
    {
        std::cout << "channelProcessConnect(" << status.toString() << ")" << std::endl;

        //m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& status)
    {
        std::cout << "processDone(" << status.toString() << ")" << std::endl;
    }

};


void testServer()
{
	
	MockServerChannelProvider::shared_pointer channelProvider(new MockServerChannelProvider());
	channelProvider->initialize();
	
	ChannelProvider::shared_pointer ptr = channelProvider;
	registerChannelProvider(ptr);

	ServerContextImpl::shared_pointer ctx = ServerContextImpl::create();
	ChannelAccess::shared_pointer channelAccess = getChannelAccess();
	ctx->initialize(channelAccess);

	ctx->printInfo();

	ctx->run(100);

	ctx->destroy();

	unregisterChannelProvider(ptr);

}

int main(int argc, char *argv[])
{
	testServer();

	cout << "Done" << endl;

    epicsThreadSleep ( 3.0 ); 
    std::cout << "-----------------------------------------------------------------------" << std::endl;
	epicsExitCallAtExits();
    CDRMonitor::get().show(stdout, true);
    return (0);
}
