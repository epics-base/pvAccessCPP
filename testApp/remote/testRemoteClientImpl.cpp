/* testRemoteClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2011.1.1 */


#include <pvAccess.h>
#include <iostream>
#include <sstream>
#include <showConstructDestruct.h>
#include <lock.h>
#include <standardPVField.h>

#include <caConstants.h>
#include <timer.h>
#include <blockingUDP.h>
#include <inetAddressUtil.h>

using namespace epics::pvData;
using namespace epics::pvAccess;



PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelProcess);

class ChannelImplProcess : public ChannelProcess
{
    private:
		ChannelProcessRequester* m_channelProcessRequester;
		PVStructure* m_pvStructure;
		PVScalar* m_valueField;
    
    private:
    ~ChannelImplProcess()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelProcess);
    }

    public:
    ChannelImplProcess(ChannelProcessRequester* channelProcessRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelProcessRequester(channelProcessRequester), m_pvStructure(pvStructure)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelProcess);

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






PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelGet);

class ChannelImplGet : public ChannelGet
{
    private:
		ChannelGetRequester* m_channelGetRequester;
		PVStructure* m_pvStructure;
		BitSet* m_bitSet;
		volatile bool m_first;
    
    private:
    ~ChannelImplGet()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelGet);
    }

    public:
    ChannelImplGet(ChannelGetRequester* channelGetRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelGetRequester(channelGetRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelGet);

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








PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannelPut);

class ChannelImplPut : public ChannelPut
{
    private:
		ChannelPutRequester* m_channelPutRequester;
		PVStructure* m_pvStructure;
		BitSet* m_bitSet;
		volatile bool m_first;
    
    private:
    ~ChannelImplPut()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannelPut);
    }

    public:
    ChannelImplPut(ChannelPutRequester* channelPutRequester, PVStructure *pvStructure, PVStructure *pvRequest) :
        m_channelPutRequester(channelPutRequester), m_pvStructure(pvStructure),
        m_bitSet(new BitSet(pvStructure->getNumberFields())), m_first(true)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannelPut);

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







PVDATA_REFCOUNT_MONITOR_DEFINE(mockMonitor);

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
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockMonitor);
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
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockMonitor);

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


class TransportRegistry;
class ChannelSearchManager;
class BlockingTCPConnector;
class NamedLockPattern;
class ResponseRequest;
class BeaconHandlerImpl;

class ClientContextImpl;


typedef int pvAccessID;
 // TODO consider std::unordered_map
typedef std::map<pvAccessID, ResponseRequest*> IOIDResponseRequestMap;


// TODO log
#define CALLBACK_GUARD(code) try { code } catch(...) { }


PVDATA_REFCOUNT_MONITOR_DEFINE(channel);


/**
 * Context state enum.
 */
enum ContextState {
	/**
	 * State value of non-initialized context.
	 */
	CONTEXT_NOT_INITIALIZED,

	/**
	 * State value of initialized context.
	 */
	CONTEXT_INITIALIZED,

	/**
	 * State value of destroyed context.
	 */
	CONTEXT_DESTROYED
};
    

class ClientContextImpl : public ClientContext
{

/**
 * Implementation of CAJ JCA <code>Channel</code>.
 * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
 */
class ChannelImpl :
            public Channel /*,
            public TransportClient,
            public TransportSender,
            public BaseSearchInstance */ {
    private:
        
    	/**
    	 * Context.
    	 */
    	ClientContextImpl* m_context;

    	/**
    	 * Client channel ID.
    	 */
    	pvAccessID m_channelID;
    
    	/**
    	 * Channel name.
    	 */
        String m_name;
    
    	/**
    	 * Channel requester.
    	 */
         ChannelRequester* m_requester;

    	/**
    	 * Process priority.
    	 */
    	short m_priority;
    
    	/**
    	 * List of fixed addresses, if <code<0</code> name resolution will be used.
    	 */
    	InetAddrVector* m_addresses;
    	
    	/**
    	 * Connection status.
    	 */
    	ConnectionState m_connectionState;
    
    	/**
    	 * List of all channel's pending requests (keys are subscription IDs). 
    	 */
    	IOIDResponseRequestMap m_responseRequests;
    	
    	/**
    	 * Allow reconnection flag. 
    	 */
    	bool m_allowCreation; // = true;
    
    	/**
    	 * Reference counting.
    	 * NOTE: synced on <code>this</code>. 
    	 */
    	int m_references; // = 1;
    
    	/* ****************** */
    	/* CA protocol fields */ 
    	/* ****************** */
    
    	/**
    	 * Server transport.
    	 */
    	Transport* m_transport;
    
    	/**
    	 * Server channel ID.
    	 */
    	pvAccessID m_serverChannelID;

        
        // TODO mock
        PVStructure* m_pvStructure;
        
    private:
    ~ChannelImpl() 
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(channel);
    }
    
    public:
    
	/**
	 * Constructor.
	 * @param context
	 * @param name
	 * @param listener
	 * @throws CAException
	 */
    ChannelImpl(
            ClientContextImpl* context,
            pvAccessID channelID,
            String name,
			ChannelRequester* requester,
			short priority,
			InetAddrVector* addresses) :
        m_context(context),
        m_channelID(channelID),
        m_name(name),
        m_requester(requester),
        m_priority(priority),
        m_addresses(addresses),
        m_connectionState(NEVER_CONNECTED),
        m_allowCreation(true),
        m_serverChannelID(0xFFFFFFFF)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(channel);
     
     /*
		// register before issuing search request
		m_context->registerChannel(this);
		
		// connect
		connect();
    */
        //
        // mock
        //     
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
        if (m_addresses) delete m_addresses;
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
        return m_context->getProvider();
    }

    virtual epics::pvData::String getRemoteAddress()
    {
        return "TODO";
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
    	return new ChannelImplProcess(channelProcessRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelGet* createChannelGet(
            ChannelGetRequester *channelGetRequester,
            epics::pvData::PVStructure *pvRequest)
    {
    	return new ChannelImplGet(channelGetRequester, m_pvStructure, pvRequest);
    }

    virtual ChannelPut* createChannelPut(
            ChannelPutRequester *channelPutRequester,
            epics::pvData::PVStructure *pvRequest)
    {
    	return new ChannelImplPut(channelPutRequester, m_pvStructure, pvRequest);
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
        String info;
        printInfo(&info);
        std::cout << info.c_str() << std::endl;
    }
    
    virtual void printInfo(epics::pvData::StringBuilder out) {
        //Lock lock(&m_channelMutex);
        //std::ostringstream ostr;
        //static String emptyString;
        
		out->append(  "CHANNEL  : "); out->append(m_name);
		out->append("\nSTATE    : "); out->append(ConnectionStateNames[m_connectionState]);
		if (m_connectionState == CONNECTED)
		{
       		out->append("\nADDRESS  : "); out->append(getRemoteAddress());
			//out->append("\nRIGHTS   : "); out->append(getAccessRights());
		}
		out->append("\n");
    }
};
    

    class ChannelProviderImpl;
    
    class ChannelImplFind : public ChannelFind
    {
        public:
        ChannelImplFind(ChannelProvider* provider) : m_provider(provider)
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
        friend class ChannelProviderImpl;
        virtual ~ChannelImplFind() {}
        
        ChannelProvider* m_provider;  
    };
        
    class ChannelProviderImpl : public ChannelProvider {
        public:
        
        ChannelProviderImpl(ClientContextImpl* context) :
            m_context(context) {
        }
    
        virtual epics::pvData::String getProviderName()
        {
            return "ChannelProviderImpl";
        }
        
        virtual void destroy()
        {
            delete this;
        }
        
        virtual ChannelFind* channelFind(
            epics::pvData::String channelName,
            ChannelFindRequester *channelFindRequester)
        {
            m_context->checkChannelName(channelName);
    
    		if (!channelFindRequester)
    			throw std::runtime_error("null requester");
            
            std::auto_ptr<Status> errorStatus(getStatusCreate()->createStatus(STATUSTYPE_ERROR, "not implemented", 0));
            channelFindRequester->channelFindResult(errorStatus.get(), 0, false);
            return 0;
        }
    
        virtual Channel* createChannel(
            epics::pvData::String channelName,
            ChannelRequester *channelRequester,
            short priority)
        {
            return createChannel(channelName, channelRequester, priority, emptyString);
        }
    
        virtual Channel* createChannel(
            epics::pvData::String channelName,
            ChannelRequester *channelRequester,
            short priority,
            epics::pvData::String address)
        {
            // TODO support addressList
    		Channel* channel = m_context->createChannelInternal(channelName, channelRequester, priority, 0);
    		if (channel)
          	     channelRequester->channelCreated(getStatusCreate()->getStatusOK(), channel);
    	    return channel;
    		
    		// NOTE it's up to internal code to respond w/ error to requester and return 0 in case of errors
        }
        
        private:
        ~ChannelProviderImpl() {};
        
        /* TODO static*/ String emptyString;
        ClientContextImpl* m_context;
    };

    public:
    
    ClientContextImpl() : 
    	m_addressList(""), m_autoAddressList(true), m_connectionTimeout(30.0f), m_beaconPeriod(15.0f),
    	m_broadcastPort(CA_BROADCAST_PORT), m_receiveBufferSize(MAX_TCP_RECV), m_timer(0),
    	m_broadcastTransport(0), m_searchTransport(0), m_connector(0), m_transportRegistry(0),
    	m_namedLocker(0), m_lastCID(0), m_lastIOID(0), m_channelSearchManager(0),
        m_version(new Version("CA Client", "cpp", 0, 0, 0, 1)),
        m_provider(new ChannelProviderImpl(this)),
        m_contextState(CONTEXT_NOT_INITIALIZED)
    {
        loadConfiguration();
    }
    
    virtual Version* getVersion() {
        return m_version;
    }

    virtual ChannelProvider* getProvider() {
        Lock lock(&m_contextMutex);
        return m_provider;
    }
    
    virtual void initialize() {
        Lock lock(&m_contextMutex);
        
		if (m_contextState == CONTEXT_DESTROYED)
			throw std::runtime_error("Context destroyed.");
		else if (m_contextState == CONTEXT_INITIALIZED)
			throw std::runtime_error("Context already initialized.");
		
		internalInitialize();

		m_contextState = CONTEXT_INITIALIZED;
    }
        
    virtual void printInfo() {
        String info;
        printInfo(&info);
        std::cout << info.c_str() << std::endl;
    }
    
    virtual void printInfo(epics::pvData::StringBuilder out) {
        Lock lock(&m_contextMutex);
        std::ostringstream ostr;
        static String emptyString;
        
		out->append(  "CLASS : ::epics::pvAccess::ClientContextImpl");
		out->append("\nVERSION : "); out->append(m_version->getVersionString());
		out->append("\nADDR_LIST : "); ostr << m_addressList; out->append(ostr.str()); ostr.str(emptyString);
		out->append("\nAUTO_ADDR_LIST : ");  out->append(m_autoAddressList ? "true" : "false");
		out->append("\nCONNECTION_TIMEOUT : "); ostr << m_connectionTimeout; out->append(ostr.str()); ostr.str(emptyString);
		out->append("\nBEACON_PERIOD : "); ostr << m_beaconPeriod; out->append(ostr.str()); ostr.str(emptyString);
		out->append("\nBROADCAST_PORT : "); ostr << m_broadcastPort; out->append(ostr.str()); ostr.str(emptyString);
		out->append("\nRCV_BUFFER_SIZE : "); ostr << m_receiveBufferSize; out->append(ostr.str()); ostr.str(emptyString);
		out->append("\nSTATE : ");
		switch (m_contextState)
		{
			case CONTEXT_NOT_INITIALIZED:
				out->append("CONTEXT_NOT_INITIALIZED");
				break;
			case CONTEXT_INITIALIZED:
				out->append("CONTEXT_INITIALIZED");
				break;
			case CONTEXT_DESTROYED:
				out->append("CONTEXT_DESTROYED");
				break;
			default:
				out->append("UNKNOWN");
		}
		out->append("\n");
    }
    
    virtual void destroy()
    {
       m_contextMutex.lock();

		if (m_contextState == CONTEXT_DESTROYED)
		{
		    m_contextMutex.unlock();
			throw std::runtime_error("Context already destroyed.");
		}
		
		// go into destroyed state ASAP			
		m_contextState =  CONTEXT_DESTROYED;

		internalDestroy();
    }
    
    virtual void dispose()
    {
        destroy();
    }    
           
    private:
    ~ClientContextImpl() {};
    
    void loadConfiguration() {
        // TODO
        /*
		m_addressList = config->getPropertyAsString("EPICS4_CA_ADDR_LIST", m_addressList);
		m_autoAddressList = config->getPropertyAsBoolean("EPICS4_CA_AUTO_ADDR_LIST", m_autoAddressList);
		m_connectionTimeout = config->getPropertyAsFloat("EPICS4_CA_CONN_TMO", m_connectionTimeout);
		m_beaconPeriod = config->getPropertyAsFloat("EPICS4_CA_BEACON_PERIOD", m_beaconPeriod);
		m_broadcastPort = config->getPropertyAsInteger("EPICS4_CA_BROADCAST_PORT", m_broadcastPort);
		m_receiveBufferSize = config->getPropertyAsInteger("EPICS4_CA_MAX_ARRAY_BYTES", m_receiveBufferSize);
        */
    }

    void internalInitialize() {
		
		m_timer = new Timer("pvAccess-client timer", lowPriority);
		/* TODO
		connector = new BlockingTCPConnector(this, receiveBufferSize, beaconPeriod);
		transportRegistry = new TransportRegistry();
		namedLocker = new NamedLockPattern();
        */
        
		// setup UDP transport
		initializeUDPTransport();

        // TODO
		// setup search manager
		//channelSearchManager = new ChannelSearchManager(this);
    }
    
    void initializeUDPTransport() {
        // TODO
    }
    
    void internalDestroy() {
        
		// stop searching
		/* TODO
		if (m_channelSearchManager)
			channelSearchManager->destroy();
        */
        
		// stop timer
		if (m_timer) 
			delete m_timer;
		 
		//
		// cleanup
		//
		
		// this will also close all CA transports
		destroyAllChannels();
		
		// close broadcast transport
		/* TODO
		if (m_broadcastTransport)
			m_broadcastTransport->destroy(true);
		if (m_searchTransport != null)
			m_searchTransport->destroy(true);
		*/

        m_provider->destroy();
        delete m_version;
		m_contextMutex.unlock();
        delete this;
    }
    
    void destroyAllChannels() {
        // TODO
    }

	/**
	 * Check channel name.
	 */
	void checkChannelName(String& name) {
		if (name.empty())
			throw std::runtime_error("null or empty channel name");
		else if (name.length() > UNREASONABLE_CHANNEL_NAME_LENGTH)
			throw std::runtime_error("name too long");
	}

	/**
	 * Check context state and tries to establish necessary state.
	 */
	void checkState() {
        Lock lock(&m_contextMutex); // TODO check double-lock?!!!

		if (m_contextState == CONTEXT_DESTROYED)
			throw std::runtime_error("Context destroyed.");
		else if (m_contextState == CONTEXT_NOT_INITIALIZED)
			initialize();
	}

	/**
	 * Searches for a channel with given channel ID.
	 * @param channelID CID.
	 * @return channel with given CID, <code>0</code> if non-existent.
	 */
	ChannelImpl* getChannel(pvAccessID channelID)
	{
	   Lock guard(&m_cidMapMutex);
	   CIDChannelMap::iterator it = m_channelsByCID.find(channelID); 
	   return (it == m_channelsByCID.end() ? 0 : it->second);
	}
	

	/**
	 * Generate Client channel ID (CID).
	 * @return Client channel ID (CID). 
	 */
	pvAccessID generateCID()
	{
        Lock guard(&m_cidMapMutex);
        
        // search first free (theoretically possible loop of death)
        while (m_channelsByCID.find(++m_lastCID) != m_channelsByCID.end());
        // reserve CID
        m_channelsByCID[m_lastCID] = 0;
        return m_lastCID;
	}

	/**
	 * Free generated channel ID (CID).
	 */
	void freeCID(int cid)
	{
        Lock guard(&m_cidMapMutex);
        m_channelsByCID.erase(cid);
	}

	/**
	 * Internal create channel.
	 */
	// TODO no minor version with the addresses
	// TODO what if there is an channel with the same name, but on different host!
	Channel* createChannelInternal(String name, ChannelRequester* requester, short priority,
			InetAddrVector* addresses) { // TODO addresses

		checkState();
		checkChannelName(name);
		
		if (requester == 0)
			throw std::runtime_error("null requester");
		
		if (priority < ChannelProvider::PRIORITY_MIN || priority > ChannelProvider::PRIORITY_MAX)
			throw std::range_error("priority out of bounds");

		bool lockAcquired = true; // TODO namedLocker->acquireSynchronizationObject(name, LOCK_TIMEOUT);
		if (lockAcquired)
		{ 
			try
			{
		  	    pvAccessID cid = generateCID();
				return new ChannelImpl(this, cid, name, requester, priority, addresses);
			}
			catch(...) {
			 // TODO
			 return 0;
			}
			// TODO namedLocker.releaseSynchronizationObject(name);	
		}
		else
		{     
			throw std::runtime_error("Failed to obtain synchronization lock for '" + name + "', possible deadlock.");
		}
	}

	/**
	 * A space-separated list of broadcast address for process variable name resolution.
	 * Each address must be of the form: ip.number:port or host.name:port
	 */
	String m_addressList;
	
	/**
	 * Define whether or not the network interfaces should be discovered at runtime. 
	 */
	bool m_autoAddressList;

	/**
	 * If the context doesn't see a beacon from a server that it is connected to for
	 * connectionTimeout seconds then a state-of-health message is sent to the server over TCP/IP.
	 * If this state-of-health message isn't promptly replied to then the context will assume that
	 * the server is no longer present on the network and disconnect.
	 */
	float m_connectionTimeout;
	
	/**
	 * Period in second between two beacon signals.
	 */
	float m_beaconPeriod;
	
	/**
	 * Broadcast (beacon, search) port number to listen to.
	 */
	int m_broadcastPort;
	
	/**
	 * Receive buffer size (max size of payload).
	 */
	int m_receiveBufferSize;
	
	/**
	 * Timer.
	 */
	Timer* m_timer;

	/**
	 * Broadcast transport needed to listen for broadcasts.
	 */
	BlockingUDPTransport* m_broadcastTransport;
	
	/**
	 * UDP transport needed for channel searches.
	 */
	BlockingUDPTransport* m_searchTransport;

	/**
	 * CA connector (creates CA virtual circuit).
	 */
	BlockingTCPConnector* m_connector;

	/**
	 * CA transport (virtual circuit) registry.
	 * This registry contains all active transports - connections to CA servers. 
	 */
	TransportRegistry* m_transportRegistry;

	/**
	 * Context instance.
	 */
	NamedLockPattern* m_namedLocker;

	/**
	 * Context instance.
	 */
	static const int LOCK_TIMEOUT = 20 * 1000;	// 20s

	/**
	 * Map of channels (keys are CIDs).
	 */
	 // TODO consider std::unordered_map
	typedef std::map<pvAccessID, ChannelImpl*> CIDChannelMap;
	CIDChannelMap m_channelsByCID;

    /**
     *  CIDChannelMap mutex.
     */
    Mutex m_cidMapMutex;

	/**
	 * Last CID cache. 
	 */
	pvAccessID m_lastCID;

	/**
	 * Map of pending response requests (keys are IOID).
	 */
	 // TODO consider std::unordered_map
	typedef std::map<pvAccessID, ResponseRequest*> IOIDResponseRequestMap;
	IOIDResponseRequestMap m_pendingResponseRequests;

	/**
	 * Last IOID cache. 
	 */
	pvAccessID m_lastIOID;

	/**
	 * Channel search manager.
	 * Manages UDP search requests.
	 */
	ChannelSearchManager* m_channelSearchManager;

	/**
	 * Beacon handler map.
	 */
	 // TODO consider std::unordered_map
	typedef std::map<osiSockAddr, BeaconHandlerImpl*> AddressBeaconHandlerMap;
	AddressBeaconHandlerMap m_beaconHandlers;
	
	/**
	 * Version.
	 */
    Version* m_version;

	/**
	 * Provider implementation.
	 */
    ChannelProviderImpl* m_provider;
    
    /**
     * Context state.
     */
    ContextState m_contextState;
    
    /**
     * Context sync. mutex.
     */
    Mutex m_contextMutex;
    
    friend class ChannelProviderImpl;
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

int main(int argc,char *argv[])
{
    ClientContextImpl* context = new ClientContextImpl();
    context->printInfo();
    
    
    ChannelFindRequesterImpl findRequester;
    context->getProvider()->channelFind("something", &findRequester);
    
    ChannelRequesterImpl channelRequester;
    Channel* channel = context->getProvider()->createChannel("test", &channelRequester);
    channel->printInfo();
    /*
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
    */
    channel->destroy();
    
    context->destroy();
    
    std::cout << "-----------------------------------------------------------------------" << std::endl;
    getShowConstructDestruct()->constuctDestructTotals(stdout);
    return(0);
}
