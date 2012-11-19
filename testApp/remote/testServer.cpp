/*
 * testServer.cpp
 */

#include <pv/serverContext.h>
#include <pv/clientContextImpl.h>
#include <epicsExit.h>
#include <pv/standardPVField.h>
#include <pv/pvTimeStamp.h>

#include <stdlib.h>
#include <time.h>
#include <vector>
#include <map>

#include <pv/logger.h>

using namespace epics::pvAccess;
using namespace epics::pvData;
using namespace std;
using std::tr1::static_pointer_cast;


map<String, PVStructure::shared_pointer> structureStore;

class StructureChangedCallback {
public:
	POINTER_DEFINITIONS(StructureChangedCallback);
	// TODO for now no BitSets, etc.
	virtual void structureChanged() = 0;
};

map<String, vector<StructureChangedCallback::shared_pointer> > structureChangedListeners;

static void notifyStructureChanged(String const & name)
{
	// NOTE: not thread-safe
	if (structureChangedListeners.find(name) != structureChangedListeners.end())
	{
		vector<StructureChangedCallback::shared_pointer> & vec = structureChangedListeners[name];
		for (vector<StructureChangedCallback::shared_pointer>::iterator iter = vec.begin();
				iter != vec.end();
				iter++)
		{
			(*iter)->structureChanged();
		}
	}
}

static PVStructure::shared_pointer getRequestedStructure(
		PVStructure::shared_pointer const & pvStructure,
		PVStructure::shared_pointer const & pvRequest,
		String subfieldName = "field")
{
	// if pvRequest is empty, just use pvStructure
	if (pvRequest.get() && pvRequest->getPVFields().size() > 0)
	{
		PVStructure::shared_pointer pvRequestFields;
		if (pvRequest->getSubField(subfieldName))
			pvRequestFields = pvRequest->getStructureField(subfieldName);
		else
			pvRequestFields = pvRequest;

		// if pvRequest is empty, just use pvStructure
		if (pvRequestFields->getPVFields().size() > 0)
		{
			StringArray const & fieldNames = pvRequestFields->getStructure()->getFieldNames();
			PVFieldPtrArray pvFields;
			StringArray actualFieldNames;
			for (StringArray::const_iterator iter = fieldNames.begin();
				 iter != fieldNames.end();
				 iter++)
			{
				PVFieldPtr pvField = pvStructure->getSubField(*iter);
				if (pvField)
				{
					PVStructurePtr pvFieldStructure =
							std::tr1::dynamic_pointer_cast<PVStructure>(pvField);

					PVStructurePtr pvRequestFieldStructure =
							std::tr1::dynamic_pointer_cast<PVStructure>(pvRequestFields->getSubField(*iter));
					if (pvRequestFieldStructure->getPVFields().size() > 0 && pvFieldStructure.get())
					{
						// add subfields only
						actualFieldNames.push_back(*iter);
						pvFields.push_back(getRequestedStructure(pvFieldStructure, pvRequestFieldStructure));
					}
					else
					{
						// add entire field
						actualFieldNames.push_back(*iter);
						pvFields.push_back(pvField);
					}
				}
			}

			return getPVDataCreate()->createPVStructure(actualFieldNames, pvFields);
		}
	}

	return pvStructure;
}


class ProcessAction : public Runnable {
public:
	typedef vector<ChannelProcess::shared_pointer> ChannelProcessVector;
	ChannelProcessVector toProcess;
	AtomicBoolean stopped;
	double period;

	ProcessAction(double periodHz) : period(periodHz) {}

    virtual void run()
    {
    	while (!stopped.get())
    	{

    		for (ChannelProcessVector::iterator iter = toProcess.begin();
    			iter != toProcess.end();
    			iter++)
    		{
    			try {
    				(*iter)->process(false);
    			} catch (std::exception &ex) {
    				std::cerr << "Unhandled exception caught in ProcessAction::run(): " << ex.what() << std::endl;
    			} catch (...) {
    				std::cerr << "Unhandled exception caught in ProcessAction::run()" << std::endl;
    			}

    			epicsThreadSleep(period);
    		}
    	}
    }
};

class ChannelFindRequesterImpl : public ChannelFindRequester
{
    virtual void channelFindResult(epics::pvData::Status const & status,
    		ChannelFind::shared_pointer const & /*channelFind*/, bool wasFound)
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

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelCreated(epics::pvData::Status const & /*status*/, Channel::shared_pointer const & /*channel*/)
    {
        //std::cout << "channelCreated(" << status.toString() << ", "
        //          << (channel ? channel->getChannelName() : "(null)") << ")" << std::endl;
    }

    virtual void channelStateChange(Channel::shared_pointer const & /*channel*/, Channel::ConnectionState /*connectionState*/)
    {
        //std::cout << "channelStateChange(" << channel->getChannelName() << ", " << Channel::ConnectionStateNames[connectionState] << ")" << std::endl;
    }
};

class GetFieldRequesterImpl : public GetFieldRequester
{
    virtual String getRequesterName()
    {
        return "GetFieldRequesterImpl";
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
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

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelGetConnect(const epics::pvData::Status& status,
    		 ChannelGet::shared_pointer const & /*channelGet*/,
    		 epics::pvData::PVStructure::shared_pointer const & pvStructure,
    		 epics::pvData::BitSet::shared_pointer const & bitSet)
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

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelPutConnect(const epics::pvData::Status& status,
    		ChannelPut::shared_pointer const & /*channelPut*/,
    		epics::pvData::PVStructure::shared_pointer const & pvStructure,
    		epics::pvData::BitSet::shared_pointer const & bitSet)
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

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void monitorConnect(const Status& status, Monitor::shared_pointer const & /*monitor*/,
    		StructureConstPtr& structure)
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
        element->changedBitSet->toString(&str);
        str += '/';
        element->overrunBitSet->toString(&str);
        str += '\n';
        element->pvStructurePtr->toString(&str);
        std::cout << str << std::endl;

        monitor->release(element);
    }

    virtual void unlisten(Monitor::shared_pointer const & /*monitor*/)
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

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual void channelProcessConnect(const epics::pvData::Status& /*status*/,
    		ChannelProcess::shared_pointer const & /*channelProcess*/)
    {
        //std::cout << "channelProcessConnect(" << status.toString() << ")" << std::endl;

        //m_channelProcess = channelProcess;
    }

    virtual void processDone(const epics::pvData::Status& /*status*/)
    {
        //std::cout << "processDone(" << status.toString() << ")" << std::endl;
    }

};

PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelProcess);

class MockChannelProcess :
	public ChannelProcess,
	public std::tr1::enable_shared_from_this<MockChannelProcess>
{
    private:
		String m_channelName;
		ChannelProcessRequester::shared_pointer m_channelProcessRequester;
		PVStructure::shared_pointer m_pvStructure;
		PVScalarPtr m_valueField;
		PVTimeStamp m_timeStamp;

    protected:
    MockChannelProcess(String const & channelName, ChannelProcessRequester::shared_pointer const & channelProcessRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelName(channelName), m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelProcess);

        ChannelProcess::shared_pointer thisPtr; // we return null  = static_pointer_cast<ChannelProcess>(shared_from_this());
        PVFieldPtr field = pvStructure->getSubField("value");
        if (field.get() == 0)
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

        m_valueField = static_pointer_cast<PVScalar>(field);

        PVFieldPtr ts = pvStructure->getSubField("timeStamp");
        if (ts) m_timeStamp.attach(ts);
    }

    public:
    static ChannelProcess::shared_pointer create(
    		String const & channelName,
    		ChannelProcessRequester::shared_pointer const & channelProcessRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelProcess::shared_pointer thisPtr(new MockChannelProcess(channelName, channelProcessRequester, pvStructure, pvRequest));

        // TODO pvRequest
    	channelProcessRequester->channelProcessConnect(Status::Ok, thisPtr);
    	
    	return thisPtr;
    }

    virtual ~MockChannelProcess()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelProcess);
    }


    virtual void process(bool lastRequest)
    {
        switch (m_valueField->getScalar()->getScalarType())
        {
            case pvBoolean:
            {
                // negate
                PVBooleanPtr pvBoolean = static_pointer_cast<PVBoolean>(m_valueField);
                pvBoolean->put(!pvBoolean->get());
                break;
            }
            case pvByte:
            {
                // increment by one
                PVBytePtr pvByte = static_pointer_cast<PVByte>(m_valueField);
                pvByte->put(pvByte->get() + 1);
                break;
            }
            case pvShort:
            {
                // increment by one
                PVShortPtr pvShort = static_pointer_cast<PVShort>(m_valueField);
                pvShort->put(pvShort->get() + 1);
                break;
            }
            case pvInt:
            {
                // increment by one
                PVIntPtr pvInt = static_pointer_cast<PVInt>(m_valueField);
                pvInt->put(pvInt->get() + 1);
                break;
            }
            case pvLong:
            {
                // increment by one
                PVLongPtr pvLong = static_pointer_cast<PVLong>(m_valueField);
                pvLong->put(pvLong->get() + 1);
                break;
            }
            case pvFloat:
            {
                // increment by one
                PVFloatPtr pvFloat = static_pointer_cast<PVFloat>(m_valueField);
                pvFloat->put(pvFloat->get() + 1.0f);
                break;
            }
            case pvDouble:
            {
                // increment by one
                PVDoublePtr pvDouble = static_pointer_cast<PVDouble>(m_valueField);
                pvDouble->put(pvDouble->get() + 1.0);
                break;
            }
            case pvString:
            {
                // increment by one
                PVStringPtr pvString = static_pointer_cast<PVString>(m_valueField);
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

        if (m_timeStamp.isAttached())
        {
        	TimeStamp current;
        	current.getCurrent();
        	m_timeStamp.set(current);
        }

    	m_channelProcessRequester->processDone(Status::Ok);

    	notifyStructureChanged(m_channelName);

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






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelGet);

class MockChannelGet : public ChannelGet
{
    private:
		ChannelGetRequester::shared_pointer m_channelGetRequester;
		PVStructure::shared_pointer m_pvStructure;
		BitSet::shared_pointer m_bitSet;
		bool m_first;

    protected:
    MockChannelGet(ChannelGetRequester::shared_pointer const & channelGetRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelGetRequester(channelGetRequester), m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields())), m_first(true)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelGet);
    }

    public:
    static ChannelGet::shared_pointer create(ChannelGetRequester::shared_pointer const & channelGetRequester, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
    	ChannelGet::shared_pointer thisPtr(new MockChannelGet(channelGetRequester, pvStructure, pvRequest));
    	channelGetRequester->channelGetConnect(Status::Ok, thisPtr,
    			static_cast<MockChannelGet*>(thisPtr.get())->m_pvStructure,
    			static_cast<MockChannelGet*>(thisPtr.get())->m_bitSet);
    	return thisPtr;
    }

    virtual ~MockChannelGet()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelGet);
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




PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelPut);

class MockChannelPut : public ChannelPut
{
    private:
		String m_channelName;
		ChannelPutRequester::shared_pointer m_channelPutRequester;
		PVStructure::shared_pointer m_pvStructure;
		BitSet::shared_pointer m_bitSet;

    protected:
    MockChannelPut(String const & channelName, ChannelPutRequester::shared_pointer const & channelPutRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
        m_channelName(channelName),
        m_channelPutRequester(channelPutRequester), m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_bitSet(new BitSet(m_pvStructure->getNumberFields()))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPut);
    }

    public:
    static ChannelPut::shared_pointer create(
    		String const & channelName,
    		ChannelPutRequester::shared_pointer const & channelPutRequester,
    		PVStructure::shared_pointer const & pvStructure,
    		PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPut::shared_pointer thisPtr(new MockChannelPut(channelName, channelPutRequester, pvStructure, pvRequest));
    	channelPutRequester->channelPutConnect(Status::Ok, thisPtr,
    			static_cast<MockChannelPut*>(thisPtr.get())->m_pvStructure,
    			static_cast<MockChannelPut*>(thisPtr.get())->m_bitSet);
    	
    	return thisPtr;
    }

    virtual ~MockChannelPut()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelPut);
    }


    virtual void put(bool lastRequest)
    {
    	m_channelPutRequester->putDone(Status::Ok);

    	notifyStructureChanged(m_channelName);

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




PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelPutGet);

class MockChannelPutGet : public ChannelPutGet
{
    private:
		String m_channelName;
		ChannelPutGetRequester::shared_pointer m_channelPutGetRequester;
		PVStructure::shared_pointer m_getStructure;
		PVStructure::shared_pointer m_putStructure;

    protected:
    MockChannelPutGet(String const & channelName, ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
    	m_channelName(channelName),
        m_channelPutGetRequester(channelPutGetRequester),
        m_getStructure(getRequestedStructure(pvStructure, pvRequest, "getField")),
        m_putStructure(getRequestedStructure(pvStructure, pvRequest, "putField"))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPutGet);
    }

    public:
    static ChannelPutGet::shared_pointer create(
    		String const & channelName,
    		ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
    		PVStructure::shared_pointer const & pvStructure,
    		PVStructure::shared_pointer const & pvRequest)
    {
        ChannelPutGet::shared_pointer thisPtr(new MockChannelPutGet(channelName, channelPutGetRequester, pvStructure, pvRequest));

    	channelPutGetRequester->channelPutGetConnect(Status::Ok, thisPtr,
    			static_cast<MockChannelPutGet*>(thisPtr.get())->m_putStructure,
    			static_cast<MockChannelPutGet*>(thisPtr.get())->m_getStructure);
    	
    	return thisPtr;
    }

    virtual ~MockChannelPutGet()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelPutGet);
    }

    virtual void putGet(bool lastRequest)
    {
    	m_channelPutGetRequester->putGetDone(Status::Ok);

    	notifyStructureChanged(m_channelName);

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






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelRPC);

class MockChannelRPC : public ChannelRPC
{
    private:
		ChannelRPCRequester::shared_pointer m_channelRPCRequester;
		String m_channelName;
		PVStructure::shared_pointer m_pvStructure;

    protected:
    MockChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
    		String const & channelName, PVStructure::shared_pointer const & pvStructure,
    		PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelRPCRequester(channelRPCRequester), m_channelName(channelName), m_pvStructure(pvStructure)
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelRPC);
    }

    public:
    static ChannelRPC::shared_pointer create(ChannelRPCRequester::shared_pointer const & channelRPCRequester, String const & channelName, PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        ChannelRPC::shared_pointer thisPtr(new MockChannelRPC(channelRPCRequester, channelName, pvStructure, pvRequest));
        // TODO pvRequest
    	channelRPCRequester->channelRPCConnect(Status::Ok, thisPtr);
    	return thisPtr;
    }

    virtual ~MockChannelRPC()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelRPC);
    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument, bool lastRequest)
    {
		if (m_channelName == "testNTTable")
    	{
			PVStructure::shared_pointer args(
					(pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
							pvArgument->getStructureField("query") :
							pvArgument
				);

	        // TODO type check, getStringField is verbose
	        PVStringPtr columns = static_pointer_cast<PVString>(args->getSubField("columns"));
			if (columns.get() == 0)
			{
	    		PVStructure::shared_pointer nullPtr;
	    		Status errorStatus(Status::STATUSTYPE_ERROR, "no columns specified");
	    		m_channelRPCRequester->requestDone(errorStatus, nullPtr);
			}
			else
			{

		        int columnsCount = atoi(columns->get().c_str());
		        StringArray fieldNames(columnsCount);
		        FieldConstPtrArray fields(columnsCount);
		        char sbuf[16];
		        vector<String> labels;
		        for (int i = 0; i < columnsCount; i++)
		        {
		        	sprintf(sbuf, "column%d", i);
		        	fieldNames[i] = sbuf;
		        	fields[i] = getFieldCreate()->createScalarArray(pvDouble);
		            labels.push_back(sbuf);
		        }

		        Structure::const_shared_pointer valueStructure(
		        			getFieldCreate()->createStructure(fieldNames, fields)
		        	);

		        StringArray tableFieldNames(2);
		        FieldConstPtrArray tableFields(2);
		        tableFieldNames[0] = "labels";
		        tableFields[0] = getFieldCreate()->createScalarArray(pvString);
		        tableFieldNames[1] = "value";
		        tableFields[1] = valueStructure;

		        PVStructure::shared_pointer result(
		        		getPVDataCreate()->createPVStructure(
		        				getFieldCreate()->createStructure(
		        						"uri:ev4:nt/2012/pwd:NTTable", tableFieldNames, tableFields)
		        			)
		        		);
		        static_pointer_cast<PVStringArray>(result->getScalarArrayField("labels", pvString))->put(0, labels.size(), &labels[0], 0);

		        PVStructure::shared_pointer resultValue = result->getStructureField("value");

		        srand ( time(NULL) );

		        #define ROWS 10
		        double values[ROWS];
                #define FILL_VALUES(OFFSET) \
		        for (int r = 0; r < ROWS; r++) \
		        	values[r] = rand()/((double)RAND_MAX+1) + OFFSET;

		        int offset = 0;
		        for (vector<String>::iterator iter = labels.begin();
		        		iter != labels.end();
		        		iter++, offset++)
		        {
		        	FILL_VALUES(offset);
		        	static_pointer_cast<PVDoubleArray>(resultValue->getScalarArrayField(*iter, pvDouble))->put(0, ROWS, values, 0);
		        }
				m_channelRPCRequester->requestDone(Status::Ok, result);
			}
    	}
		else if (m_channelName == "testNTMatrix")
    	{
			PVStructure::shared_pointer args(
					(pvArgument->getStructure()->getID() == "uri:ev4:nt/2012/pwd:NTURI") ?
							pvArgument->getStructureField("query") :
							pvArgument
				);

	        PVStringPtr rows = static_pointer_cast<PVString>(args->getSubField("rows"));
	        PVStringPtr columns = static_pointer_cast<PVString>(args->getSubField("columns"));
			if (rows.get() == 0 || columns.get() == 0)
			{
	    		PVStructure::shared_pointer nullPtr;
	    		Status errorStatus(Status::STATUSTYPE_ERROR, "no rows and columns specified");
	    		m_channelRPCRequester->requestDone(errorStatus, nullPtr);
			}
			else
			{
				int i = 0;
		        int totalFields = 2;  // value[], dim
		        StringArray fieldNames(totalFields);
		        FieldConstPtrArray fields(totalFields);
		        fieldNames[i] = "value";
		        fields[i++] = getFieldCreate()->createScalarArray(pvDouble);
		        fieldNames[i] = "dim";
		        fields[i++] = getFieldCreate()->createScalarArray(pvInt);

		        PVStructure::shared_pointer result(
		        		getPVDataCreate()->createPVStructure(
		        				getFieldCreate()->createStructure("uri:ev4:nt/2012/pwd:NTMatrix", fieldNames, fields)
		        			)
		        		);

		        int32 rowsVal = atoi(rows->get().c_str());
		        int32 colsVal = atoi(columns->get().c_str());
		        int32 dimValues[] = { rowsVal, colsVal };
		        static_pointer_cast<PVIntArray>(result->getScalarArrayField("dim", pvInt))->put(0, 2, &dimValues[0], 0);

		        srand ( time(NULL) );

		        PVStringPtr byColumns = static_pointer_cast<PVString>(args->getSubField("bycolumns"));
		        bool bycolumns = (byColumns.get() && byColumns->get() == "1");

		        int32 len = rowsVal * colsVal;
		        vector<double> mv(len);
		        for (int r = 0; r < len; r++)
		        	if (bycolumns)
		        		mv[r] = rand()/((double)RAND_MAX+1) + (int)(r%rowsVal);
		        	else
		        		mv[r] = rand()/((double)RAND_MAX+1) + (int)(r/colsVal);
		        static_pointer_cast<PVDoubleArray>(result->getScalarArrayField("value", pvDouble))->put(0, len, &mv[0], 0);

	    		m_channelRPCRequester->requestDone(Status::Ok, result);
			}
    	}
		else
    	{
    		/*
    		std::string s;
    		pvArgument->toString(&s);
    		std::cout << "RPC" << std::endl << s << std::endl;
			*/
    		m_channelRPCRequester->requestDone(Status::Ok, m_pvStructure);
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










PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannelArray);

class MockChannelArray : public ChannelArray
{
    private:
		ChannelArrayRequester::shared_pointer m_channelArrayRequester;
		PVArray::shared_pointer m_pvArray;

    protected:
    MockChannelArray(ChannelArrayRequester::shared_pointer const & channelArrayRequester,
    		PVStructure::shared_pointer const & /*pvStructure*/, PVStructure::shared_pointer const & /*pvRequest*/) :
        m_channelArrayRequester(channelArrayRequester),
        m_pvArray(getPVDataCreate()->createPVScalarArray(pvDouble))
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannelArray);
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
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannelArray);
    }

    virtual void putArray(bool lastRequest, int /*offset*/, int /*count*/)
    {
        // TODO offset, count
    	m_channelArrayRequester->putArrayDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void getArray(bool lastRequest, int /*offset*/, int /*count*/)
    {
        // TODO offset, count
    	m_channelArrayRequester->getArrayDone(Status::Ok);
    	if (lastRequest)
    	   destroy();
    }

    virtual void setLength(bool lastRequest, int /*length*/, int /*capacity*/)
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






PVACCESS_REFCOUNT_MONITOR_DEFINE(mockMonitor);

class MockMonitor : public Monitor, public StructureChangedCallback, public std::tr1::enable_shared_from_this<MockMonitor>
{
    private:
		String m_channelName;
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
    MockMonitor(String const & channelName, MonitorRequester::shared_pointer const & monitorRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest) :
    	m_channelName(channelName),
        m_monitorRequester(monitorRequester), m_pvStructure(getRequestedStructure(pvStructure, pvRequest)),
        m_changedBitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_overrunBitSet(new BitSet(m_pvStructure->getNumberFields())),
        m_first(true),
        m_lock(),
        m_count(0),
        m_thisPtr(new MonitorElement())
    {
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockMonitor);

        m_changedBitSet->set(0);

        m_thisPtr->pvStructurePtr = m_pvStructure;
        m_thisPtr->changedBitSet = m_changedBitSet;
        m_thisPtr->overrunBitSet = m_overrunBitSet;
   }

    public:
    static Monitor::shared_pointer create(String const & channelName,
    		MonitorRequester::shared_pointer const & monitorRequester,
    		PVStructure::shared_pointer const & pvStructure, PVStructure::shared_pointer const & pvRequest)
    {
        Monitor::shared_pointer thisPtr(new MockMonitor(channelName, monitorRequester, pvStructure, pvRequest));
        StructureConstPtr structurePtr = static_cast<MockMonitor*>(thisPtr.get())->m_pvStructure->getStructure();
        monitorRequester->monitorConnect(Status::Ok, thisPtr, structurePtr);

        structureChangedListeners[channelName].push_back(std::tr1::dynamic_pointer_cast<StructureChangedCallback>(thisPtr));
        return thisPtr;
    }

    virtual ~MockMonitor()
    {
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockMonitor);
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

	virtual void structureChanged()
	{
		m_count = 0;
    	Monitor::shared_pointer thisPtr = shared_from_this();
        m_monitorRequester->monitorEvent(thisPtr);
	}

    virtual MonitorElement::shared_pointer poll()
    {
        Lock xx(m_lock);
        if (m_count)
        {
            return m_nullMonitor;
        }
        else
        {
            m_count++;
            return m_thisPtr;
        }
    }

    virtual void release(MonitorElement::shared_pointer const & /*monitorElement*/)
    {
        Lock xx(m_lock);
        if (m_count)
        {
            m_count--;
        }
    }

    virtual void destroy()
    {
        stop();

        // remove itself from listeners table
        vector<StructureChangedCallback::shared_pointer> &vec = structureChangedListeners[m_channelName];
        vec.erase(find(vec.begin(), vec.end(), std::tr1::dynamic_pointer_cast<StructureChangedCallback>(shared_from_this())));
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


PVACCESS_REFCOUNT_MONITOR_DEFINE(mockChannel);

class MockChannel : public Channel {
    private:
        ChannelProvider::weak_pointer  m_provider;
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
        PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(mockChannel);

        if (structureStore.find(m_name) != structureStore.end())
        	m_pvStructure = structureStore[m_name];

        else
        {
        	// create structure

			if (m_name.find("testArray") == 0)
			{
				String allProperties("");
	//            String allProperties("alarm,timeStamp,display,control");
				m_pvStructure = getStandardPVField()->scalarArray(pvDouble,allProperties);
				PVDoubleArrayPtr pvField = static_pointer_cast<PVDoubleArray>(m_pvStructure->getScalarArrayField(String("value"), pvDouble));

				int specCount = 0; char postfix[64];
				int done = sscanf(m_name.c_str(), "testArray%d%s", &specCount, postfix);

				if (done && specCount > 0)
				{
					pvField->setCapacity(specCount);
					pvField->setLength(specCount);

					double v = 0;
					int ix = 0;
					const int COUNT = 1024;

					int n = 0;
					while (n < specCount)
					{

						double array[COUNT];
						int i = 0;
						for (; i < COUNT && n < specCount; i++)
						{
							array[i] = v; v+=1; n++;
						}
						pvField->put(ix, i, array, 0);
						ix += i;
					}
				}
				else
				{
					double v = 0;
					int ix = 0;
					const int COUNT = 1024;

					pvField->setCapacity(1024*COUNT);
					for (int n = 0; n < 1024; n++)
					{

						double array[COUNT];
						for (int i = 0; i < COUNT; i++)
						{
							array[i] = v; v+=1.1;
						}
						pvField->put(ix, COUNT, array, 0);
						ix += COUNT;
					}
				}
				/*
				printf("array prepared------------------------------------!!!\n");
				String str;
				pvField->toString(&str);
				printf("%s\n", str.c_str());
				printf("=============------------------------------------!!!\n");
				*/
			}
			else if (m_name.find("testImage") == 0)
			{
				String allProperties("alarm,timeStamp,display,control");
				m_pvStructure = getStandardPVField()->scalarArray(pvByte,allProperties);
				PVByteArrayPtr pvField = static_pointer_cast<PVByteArray>(m_pvStructure->getScalarArrayField(String("value"), pvByte));
				int ix = 0;
				const int COUNT = 1024;

				pvField->setCapacity(1024*COUNT);
				for (int n = 0; n < 1024; n++)
				{

					int8 array[COUNT];
					for (int i = 0; i < COUNT; i++)
					{
						array[i] = ix;
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
			else if (m_name.find("testRPC") == 0 || m_name == "testNTTable" || m_name == "testNTMatrix")
			{
				StringArray fieldNames;
				PVFieldPtrArray fields;
				m_pvStructure = getPVDataCreate()->createPVStructure(fieldNames, fields);
			}
			else if (m_name.find("testValueOnly") == 0)
			{
				String allProperties("");
				m_pvStructure = getStandardPVField()->scalar(pvDouble,allProperties);
			}
			else if (m_name == "testCounter")
			{
				String allProperties("timeStamp");
				m_pvStructure = getStandardPVField()->scalar(pvInt,allProperties);
			}
			else
			{
				String allProperties("alarm,timeStamp,display,control,valueAlarm");
				m_pvStructure = getStandardPVField()->scalar(pvDouble,allProperties);
				//PVDoublePtr pvField = m_pvStructure->getDoubleField(String("value"));
				//pvField->put(1.123);
			}

			structureStore[m_name] = m_pvStructure;
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
        PVACCESS_REFCOUNT_MONITOR_DESTRUCT(mockChannel);
    }

    virtual void destroy()
    {
    };

    virtual String getRequesterName()
    {
        return getChannelName();
    };

    virtual void message(String const & message,MessageType messageType)
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
    }

    virtual ChannelProvider::shared_pointer getProvider()
    {
        return m_provider.lock();
    }

    virtual epics::pvData::String getRemoteAddress()
    {
        return m_remoteAddress;
    }

    virtual epics::pvData::String getChannelName()
    {
        return m_name;
    }

    virtual  std::tr1::shared_ptr<ChannelRequester> getChannelRequester()
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

    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & /*pvField*/)
    {
        return readWrite;
    }

    virtual void getField(GetFieldRequester::shared_pointer const & requester,epics::pvData::String const & subField)
    {
    	PVFieldPtr pvField;
    	if(subField == "")
    	{
    		pvField = m_pvStructure;
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
    	return MockChannelProcess::create(m_name, channelProcessRequester, m_pvStructure, pvRequest);
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
    	return MockChannelPut::create(m_name, channelPutRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
    		ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelPutGet::create(m_name, channelPutGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelRPC::shared_pointer createChannelRPC(ChannelRPCRequester::shared_pointer const & channelRPCRequester,
    		epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockChannelRPC::create(channelRPCRequester, m_name, m_pvStructure, pvRequest);
    }

    virtual epics::pvData::Monitor::shared_pointer createMonitor(
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
    	return MockMonitor::create(m_name, monitorRequester, m_pvStructure, pvRequest);
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


	MockServerChannelProvider() :
		m_mockChannelFind(),
		m_counterChannel(),
		m_scan1Hz(1.0),
		m_scan1HzThread()
	{
    }

	virtual ~MockServerChannelProvider()
	{
		m_scan1Hz.stopped.set();
	}

	void initialize()
	{
		ChannelProvider::shared_pointer chProviderPtr = shared_from_this();
		m_mockChannelFind.reset(new MockChannelFind(chProviderPtr));


		std::tr1::shared_ptr<ChannelRequesterImpl> cr(new ChannelRequesterImpl());
		m_counterChannel = MockChannel::create(chProviderPtr, cr, "testCounter", "local");
		std::tr1::shared_ptr<ChannelProcessRequesterImpl> cpr(new ChannelProcessRequesterImpl());
		ChannelProcess::shared_pointer process = m_counterChannel->createChannelProcess(cpr, PVStructure::shared_pointer());
		//process->process(false);

		m_scan1Hz.toProcess.push_back(process);
	    m_scan1HzThread.reset(new Thread("process1hz", highPriority, &m_scan1Hz));
	}

    virtual epics::pvData::String getProviderName()
    {
        return "local";
    }

    virtual void destroy()
    {
    }

    virtual ChannelFind::shared_pointer channelFind(
        epics::pvData::String const & channelName,
        ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        // channel that starts with "test" always exists
    	bool exists = (channelName.find("test") == 0);
        channelFindRequester->channelFindResult(Status::Ok, m_mockChannelFind, exists);
        return m_mockChannelFind;
    }

    virtual Channel::shared_pointer createChannel(
        epics::pvData::String const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority)
    {
        return createChannel(channelName, channelRequester, priority, "local");
    }

    virtual Channel::shared_pointer createChannel(
        epics::pvData::String const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short /*priority*/,
        epics::pvData::String const & address)
    {
        if (address == "local")
        {
        	if (channelName == "testCounter")
        	{
        		channelRequester->channelCreated(Status::Ok, m_counterChannel);
        		return m_counterChannel;
        	}
        	else
        	{
        		ChannelProvider::shared_pointer chProviderPtr = shared_from_this();
        		Channel::shared_pointer channel = MockChannel::create(chProviderPtr, channelRequester, channelName, address);
        		channelRequester->channelCreated(Status::Ok, channel);
        		return channel;
        	}
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
    Channel::shared_pointer m_counterChannel;

	ProcessAction m_scan1Hz;
	auto_ptr<Thread> m_scan1HzThread;
};




void testServer(int timeToRun)
{
	
	MockServerChannelProvider::shared_pointer channelProvider(new MockServerChannelProvider());
	channelProvider->initialize();
	
	ChannelProvider::shared_pointer ptr = channelProvider;
	registerChannelProvider(ptr);

	ServerContextImpl::shared_pointer ctx = ServerContextImpl::create();
	ChannelAccess::shared_pointer channelAccess = getChannelAccess();
	ctx->initialize(channelAccess);

	ctx->printInfo();

	ctx->run(timeToRun);

	ctx->destroy();

	unregisterChannelProvider(ptr);

	structureChangedListeners.clear();
	structureStore.clear();
}

#include <epicsGetopt.h>

void usage (char *argv[])
{
    fprintf (stderr, "\nUsage: %s [options]\n\n"
    "  -h: Help: Print this message\n"
    "\noptions:\n"
    "  -t <seconds>:   Time to run in seconds, 0 for forever\n"
    "  -d:             Enable debug output\n"
    "  -c:             Wait for clean shutdown and report used instance count (for expert users)"
    "\n\n",
    argv[0]);
}



int main(int argc, char *argv[])
{
    int opt;                    /* getopt() current option */
    bool debug = false;
    bool cleanupAndReport = false;
    int timeToRun = 0;

    setvbuf(stdout,NULL,_IOLBF,BUFSIZ);    /* Set stdout to line buffering */

    while ((opt = getopt(argc, argv, ":ht:dc")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage(argv);
            return 0;
        case 't':               /* Print usage */
            timeToRun = atoi(optarg);
            break;
        case 'd':               /* Debug log level */
            debug = true;
            break;
        case 'c':               /* Clean-up and report used instance count */
            cleanupAndReport = true;
            break;
        case '?':
            fprintf(stderr,
                    "Unrecognized option: '-%c'. ('%s -h' for help.)\n",
                    optopt, argv[0]);
            return 1;
        case ':':
            fprintf(stderr,
                    "Option '-%c' requires an argument. ('%s -h' for help.)\n",
                    optopt, argv[0]);
            return 1;
        default :
            usage(argv);
            return 1;
        }
    }

    SET_LOG_LEVEL(debug ? logLevelDebug : logLevelError);

    testServer(timeToRun);

	cout << "Done" << endl;

    if (cleanupAndReport)
    {
        // TODO implement wait on context
		epicsThreadSleep ( 3.0 );
		//std::cout << "-----------------------------------------------------------------------" << std::endl;
		//epicsExitCallAtExits();
    }

    return (0);
}
