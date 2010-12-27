/* MockClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2010.12.18 */


#include <pvAccess.h>
#include <iostream>
#include <showConstructDestruct.h>
#include <lock.h>
#include <standardPVField.h>

#include <sstream>

using namespace epics::pvData;
using namespace epics::pvAccess;

using std::string;

class CreateRequestFactory {
    private:
    
        static void trim(std::string& str)
        {
          std::string::size_type pos = str.find_last_not_of(' ');
          if(pos != std::string::npos) {
            str.erase(pos + 1);
            pos = str.find_first_not_of(' ');
            if(pos != std::string::npos) str.erase(0, pos);
          }
          else str.erase(str.begin(), str.end());
        }
    
    	static int findMatchingBrace(string& request, int index, int numOpen) {
    		size_t openBrace = request.find('{', index+1);
    		size_t closeBrace = request.find('}', index+1);
    		if(openBrace == std::string::npos && closeBrace == std::string::npos) return std::string::npos;
    		if (openBrace != std::string::npos) {
    			if(openBrace<closeBrace) return findMatchingBrace(request,openBrace,numOpen+1);
    			if(numOpen==1) return closeBrace;
    			return findMatchingBrace(request,closeBrace,numOpen-1);
    		}
    		if(numOpen==1) return closeBrace;
    		return findMatchingBrace(request,closeBrace,numOpen-1);
    	}
    	
        static bool createFieldRequest(PVStructure* pvParent,std::string request,bool fieldListOK,Requester* requester) {
        	trim(request);
        	if(request.length()<=0) return true;
        	size_t comma = request.find(',');
        	size_t openBrace = request.find('{');
        	size_t openBracket = request.find('[');
        	if(openBrace != std::string::npos || openBracket != std::string::npos) fieldListOK = false;
        	if(openBrace != std::string::npos && (comma==std::string::npos || comma>openBrace)) {
        		//find matching brace
        		size_t closeBrace = findMatchingBrace(request,openBrace+1,1);
        		if(closeBrace==std::string::npos) {
        			requester->message(request + "mismatched { }", errorMessage);
        			return false;
        		}
        		String fieldName = request.substr(0,openBrace);
        		PVStructure* pvStructure = getPVDataCreate()->createPVStructure(pvParent, fieldName, 0);
        		createFieldRequest(pvStructure,request.substr(openBrace+1,closeBrace-openBrace-1),false,requester);
        		pvParent->appendPVField(pvStructure);
        		if(request.length()>closeBrace+1) {
        			if(request.at(closeBrace+1) != ',') {
        				requester->message(request + "misssing , after }", errorMessage);
        				return false;
        			}
        			if(!createFieldRequest(pvParent,request.substr(closeBrace+2),false,requester)) return false;
        		}
        		return true;
        	}
        	if(openBracket==std::string::npos && fieldListOK) {
        			PVString* pvStringField = static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvParent, "fieldList", pvString));
        			pvStringField->put(request);
        			pvParent->appendPVField(pvStringField);
        			return true;
        	}
        	if(openBracket!=std::string::npos && (comma==std::string::npos || comma>openBracket)) {
        		size_t closeBracket = request.find(']');
    			if(closeBracket==std::string::npos) {
        		    requester->message(request + "option does not have matching []", errorMessage);
        			return false;
    			}
    			if(!createLeafFieldRequest(pvParent,request.substr(0, closeBracket+1),requester)) return false;
    			if(request.rfind(',')>closeBracket) {
    				int nextComma = request.find(',', closeBracket);
    				if(!createFieldRequest(pvParent,request.substr(nextComma+1),false,requester)) return false;
    			} 
    			return true;
        	}
        	if(comma!=std::string::npos) {
        		if(!createLeafFieldRequest(pvParent,request.substr(0, comma),requester)) return false;
        		return createFieldRequest(pvParent,request.substr(comma+1),false,requester);
        	}
        	return createLeafFieldRequest(pvParent,request,requester);
        }
        
        static bool createLeafFieldRequest(PVStructure* pvParent,String request,Requester* requester) {
        	size_t openBracket = request.find('[');
        	String fullName = request;
        	if(openBracket != std::string::npos) fullName = request.substr(0,openBracket);
        	size_t indLast = fullName.rfind('.');
    		String fieldName = fullName;
    		if(indLast>1 && indLast != std::string::npos) fieldName = fullName.substr(indLast+1);
        	PVStructure* pvStructure = getPVDataCreate()->createPVStructure(pvParent, fieldName, 0);
    		PVStructure* pvLeaf = getPVDataCreate()->createPVStructure(pvStructure,"leaf", 0);
    		PVString* pvStringField = static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvLeaf, "source", pvString));
    		pvStringField->put(fullName);
    		pvLeaf->appendPVField(pvStringField);
    		if(openBracket != std::string::npos) {
    			size_t closeBracket = request.find(']');
    			if(closeBracket==std::string::npos) {
    				requester->message("option does not have matching []", errorMessage);
    				return false;
    			}
    			if(!createRequestOptions(pvLeaf,request.substr(openBracket+1, closeBracket-openBracket-1),requester)) return false;
    		}
    		pvStructure->appendPVField(pvLeaf);
    		pvParent->appendPVField(pvStructure);
    		return true;
        }
        
        static bool createRequestOptions(PVStructure* pvParent,std::string request,Requester* requester) {
    		trim(request);
    		if(request.length()<=1) return true;
    		
            std::string token;
            std::istringstream iss(request);
            while (getline(iss, token, ','))
            {
                size_t equalsPos = token.find('=');
                size_t equalsRPos = token.rfind('=');
                if (equalsPos != equalsRPos)
                {
        			requester->message("illegal option ", errorMessage);
        			return false;
                }
        		
        		PVString* pvStringField = static_cast<PVString*>(getPVDataCreate()->createPVScalar(pvParent, token.substr(0, equalsPos), pvString));
        		pvStringField->put(token.substr(equalsPos+1));
        		pvParent->appendPVField(pvStringField);
            }
        	return true;
        }
    
    public:
        
        /**
         * Create a request structure for the create calls in Channel.
         * See the package overview documentation for details.
         * @param request The field request. See the package overview documentation for details.
         * @param requester The requester;
         * @return The request structure if an invalid request was given. 
         */
    	static PVStructure* createRequest(String request, Requester* requester)
    	{
        	static String emptyString;
    		if (!request.empty()) trim(request);
        	if (request.empty())
        	{
        	   return getPVDataCreate()->createPVStructure(0, emptyString, 0);
        	}
        	
    		size_t offsetRecord = request.find("record[");
    		size_t offsetField = request.find("field(");
    		size_t offsetPutField = request.find("putField(");
    		size_t offsetGetField = request.find("getField(");

            PVStructure* pvStructure = getPVDataCreate()->createPVStructure(0, emptyString, 0);
    		if (offsetRecord != std::string::npos) {
    			size_t offsetBegin = request.find('[', offsetRecord);
    			size_t offsetEnd = request.find(']', offsetBegin);
    			if(offsetEnd == std::string::npos) {
                    delete pvStructure;
    				requester->message("record[ does not have matching ]", errorMessage);
    				return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "record", 0);
    			if(!createRequestOptions(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),requester)) 
    			{
    			     // TODO is this ok? 
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (offsetField != std::string::npos) {
    			size_t offsetBegin = request.find('(', offsetField);
    			size_t offsetEnd = request.find(')', offsetBegin);
    			if(offsetEnd == std::string::npos) {
                    delete pvStructure;
    				requester->message("field( does not have matching )", errorMessage);
    				return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "field", 0);
    			if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true,requester))
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (offsetPutField != std::string::npos) {
    			size_t offsetBegin = request.find('(', offsetPutField);
    			size_t offsetEnd = request.find(')', offsetBegin);
    			if(offsetEnd == std::string::npos) {
    			    delete pvStructure;
    				requester->message("putField( does not have matching )", errorMessage);
    				return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "putField", 0);
    			if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true,requester))
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (offsetGetField != std::string::npos) {
    			size_t offsetBegin = request.find('(', offsetGetField);
    			size_t offsetEnd = request.find(')', offsetBegin);
    			if(offsetEnd == std::string::npos) {
    			     delete pvStructure;
    				 requester->message("getField( does not have matching )", errorMessage);
    				 return 0;
    			}
    			PVStructure* pvStruct = getPVDataCreate()->createPVStructure(pvStructure, "getField", 0);
    			if(!createFieldRequest(pvStruct,request.substr(offsetBegin+1, offsetEnd-offsetBegin-1),true,requester)) 
    			{
    			     delete pvStruct;
    			     delete pvStructure;
    			     return 0;
    			}
    			pvStructure->appendPVField(pvStruct);
    		}
    		if (pvStructure->getStructure()->getNumberFields()==0) {
    			if(!createFieldRequest(pvStructure,request,true,requester))
    			{
    			     delete pvStructure;
    			     return 0;
    			}
    		}
        	return pvStructure;
    	}
    	
};





static volatile int64 mockChannelProcess_totalConstruct = 0;
static volatile int64 mockChannelProcess_totalDestruct = 0;
static Mutex *mockChannelProcess_globalMutex = 0;

static int64 mockChannelProcess_processTotalConstruct()
{
    Lock xx(mockChannelProcess_globalMutex);
    return mockChannelProcess_totalConstruct;
}

static int64 mockChannelProcess_processTotalDestruct()
{
    Lock xx(mockChannelProcess_globalMutex);
    return mockChannelProcess_totalDestruct;
}

static ConstructDestructCallback *mockChannelProcess_pConstructDestructCallback;

static void mockChannelProcess_init()
{
     static Mutex mutex = Mutex();
     Lock xx(&mutex);
     if(mockChannelProcess_globalMutex==0) {
        mockChannelProcess_globalMutex = new Mutex();
        mockChannelProcess_pConstructDestructCallback = new ConstructDestructCallback(
            String("mockChannelProcess"),
            mockChannelProcess_processTotalConstruct,mockChannelProcess_processTotalDestruct,0);
     }
}

class MockChannelProcess : public ChannelProcess
{
    private:
		ChannelProcessRequester* m_channelProcessRequester;
		PVStructure* m_pvStructure;
		PVScalar* m_valueField;
    
    private:
    ~MockChannelProcess()
    {
        Lock xx(mockChannelProcess_globalMutex);
        mockChannelProcess_totalDestruct++;
    }

    public:
    MockChannelProcess(ChannelProcessRequester* channelProcessRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
        mockChannelProcess_init();   

        Lock xx(mockChannelProcess_globalMutex);
        mockChannelProcess_totalConstruct++;


        PVField* field = pvStructure->getSubField(String("value"));
        if (field == 0)
        {
            Status* noValueFieldStatus = getStatusCreate()->createStatus(STATUSTYPE_ERROR, "no 'value' field");
        	m_channelProcessRequester->channelProcessConnect(noValueFieldStatus, this);
        	delete noValueFieldStatus;
        	
        	// NOTE client must destroy this instance...
        	// do not access any fields and return ASAP
        	return;
        }
        
        if (field->getField()->getType() != scalar)
        {
            Status* notAScalarStatus = getStatusCreate()->createStatus(STATUSTYPE_ERROR, "'value' field not scalar type");
        	m_channelProcessRequester->channelProcessConnect(notAScalarStatus, this);
        	delete notAScalarStatus;
        	
        	// NOTE client must destroy this instance….
        	// do not access any fields and return ASAP
        	return;
        }
        
        m_valueField = static_cast<PVScalar*>(field);
        	
        // TODO pvRequest 
    	m_channelProcessRequester->channelProcessConnect(getStatusCreate()->getStatusOK(), this);
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
    	m_channelProcessRequester->processDone(getStatusCreate()->getStatusOK());
    	
    	if (lastRequest)
    	   destroy();
    }
    
    virtual void destroy()
    {
        delete this;
    }
    
};








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
        pvField->put(1.123);

        
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


class ChannelProcessRequesterImpl : public ChannelProcessRequester
{
    ChannelProcess *m_channelProcess;
                
    virtual String getRequesterName()
    {
        return "ProcessRequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }

    virtual void channelProcessConnect(epics::pvData::Status *status,ChannelProcess *channelProcess)
    {
        std::cout << "channelProcessConnect(" << status->toString() << ")" << std::endl;
        
        // TODO sync
        m_channelProcess = channelProcess;
    }

    virtual void processDone(epics::pvData::Status *status)
    {
        std::cout << "processDone(" << status->toString() << ")" << std::endl;
    }

};

/*
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


    ChannelProcessRequesterImpl channelProcessRequester;
    ChannelProcess* channelProcess = channel->createChannelProcess(&channelProcessRequester, 0);
    channelProcess->process(false);
    channelProcess->destroy();
    

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

*/


class RequesterImpl : public Requester {
    public:
    
    virtual String getRequesterName()
    {
        return "RequesterImpl";
    };
    
    virtual void message(String message,MessageType messageType) 
    {
        std::cout << "[" << getRequesterName() << "] message(" << message << ", " << messageTypeName[messageType] << ")" << std::endl; 
    }
};
    
#include <epicsAssert.h>
    
static void testCreateRequest() {
    RequesterImpl requester;
    String out;
	String request = "";
    PVStructure* pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "alarm,timeStamp,power.value";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "record[process=true]field(alarm,timeStamp,power.value)";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;
    
    request = "record[process=true]field(alarm,timeStamp[algorithm=onChange,causeMonitor=false],power{power.value,power.alarm})";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value)";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{power.value,power.alarm},"
    	+ "current{current.value,current.alarm},voltage{voltage.value,voltage.alarm})";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{power.value,power.alarm},"
    	+ "current{current.value,current.alarm},voltage{voltage.value,voltage.alarm},"
    	+ "ps0{"
    	+ "ps0.alarm,ps0.timeStamp,power{ps0.power.value,ps0.power.alarm},"
    	+ "current{ps0.current.value,ps0.current.alarm},voltage{ps0.voltage.value,ps0.voltage.alarm}},"
    	+ "ps1{"
    	+ "ps1.alarm,ps1.timeStamp,power{ps1.power.value,ps1.power.alarm},"
    	+ "current{ps1.current.value,ps1.current.alarm},voltage{ps1.voltage.value,ps1.voltage.alarm}"
    	+ "})";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "a{b{c{d}}}";
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest);
    out.clear(); pvRequest->toString(&out); std::cout << out << std::endl;
    delete pvRequest;

    request = "record[process=true,xxx=yyy]field(alarm,timeStamp[shareData=true],power.value";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest==0);
    
    request = String("record[process=true,xxx=yyy]")
    	+ "putField(power.value)"
    	+ "getField(alarm,timeStamp,power{power.value,power.alarm},"
    	+ "current{current.value,current.alarm},voltage{voltage.value,voltage.alarm},"
    	+ "ps0{"
    	+ "ps0.alarm,ps0.timeStamp,power{ps0.power.value,ps0.power.alarm},"
    	+ "current{ps0.current.value,ps0.current.alarm},voltage{ps0.voltage.value,ps0.voltage.alarm}},"
    	+ "ps1{"
    	+ "ps1.alarm,ps1.timeStamp,power{ps1.power.value,ps1.power.alarm},"
    	+ "current{ps1.current.value,ps1.current.alarm},voltage{ps1.voltage.value,ps1.voltage.alarm}"
    	+ ")";
    std::cout << std::endl << "Error Expected for next call!!" << std::endl;
    pvRequest = CreateRequestFactory::createRequest(request,&requester);
    assert(pvRequest==0);
}

int main(int argc,char *argv[])
{
    testCreateRequest();
    
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return 0;    
}
