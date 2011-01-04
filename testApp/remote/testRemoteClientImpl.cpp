/* testRemoteClientImpl.cpp */
/* Author:  Matej Sekoranja Date: 2011.1.1 */

#include <transportRegistry.h>
#include <pvAccess.h>
#include <iostream>
#include <showConstructDestruct.h>
#include <lock.h>
#include <standardPVField.h>

#include <caConstants.h>
#include <timer.h>
#include <blockingUDP.h>

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






PVDATA_REFCOUNT_MONITOR_DEFINE(mockChannel);

class ChannelImpl : public Channel {
    private:
        ChannelProvider* m_provider;
        ChannelRequester* m_requester;
        String m_name;
        String m_remoteAddress;

        PVStructure* m_pvStructure;

    private:
    ~ChannelImpl()
    {
        PVDATA_REFCOUNT_MONITOR_DESTRUCT(mockChannel);
    }

    public:

    ChannelImpl(
        ChannelProvider* provider,
        ChannelRequester* requester,
        String name,
        String remoteAddress) :
        m_provider(provider),
        m_requester(requester),
        m_name(name),
        m_remoteAddress(remoteAddress)
    {
        PVDATA_REFCOUNT_MONITOR_CONSTRUCT(mockChannel);


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

    ChannelProviderImpl() : m_mockChannelFind(new ChannelImplFind(this)) {
    }

    virtual epics::pvData::String getProviderName()
    {
        return "ChannelProviderImpl";
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
            Channel* channel = new ChannelImpl(this, channelRequester, channelName, address);
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
    ~ChannelProviderImpl() {};

    ChannelImplFind* m_mockChannelFind;

};


class ChannelSearchManager;
class BlockingTCPConnector;
class NamedLockPattern;
class ResponseRequest;
class BeaconHandlerImpl;

class ClientContextImpl : public ClientContext
{
    public:

    ClientContextImpl() :
    	m_addressList(""), m_autoAddressList(true), m_connectionTimeout(30.0f), m_beaconPeriod(15.0f),
    	m_broadcastPort(CA_BROADCAST_PORT), m_receiveBufferSize(MAX_TCP_RECV), m_timer(0),
    	m_broadcastTransport(0), m_searchTransport(0), m_connector(0), m_transportRegistry(0),
    	m_namedLocker(0), m_lastCID(0), m_lastIOID(0), m_channelSearchManager(0),
        m_version(new Version("CA Client", "cpp", 0, 0, 0, 1))
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
        m_provider = new ChannelProviderImpl();
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
    ~ClientContextImpl() {};

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
	typedef std::map<int, ChannelImpl*> IntChannelMap;
	IntChannelMap m_channelsByCID;

typedef int pvAccessID;
	/**
	 * Last CID cache.
	 */
	pvAccessID m_lastCID;

	/**
	 * Map of pending response requests (keys are IOID).
	 */
	 // TODO consider std::unordered_map
	typedef std::map<int, ResponseRequest*> IntResponseRequestMap;
	IntResponseRequestMap m_pendingResponseRequests;

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
    //Channel* noChannel
    context->getProvider()->createChannel("test", &channelRequester, ChannelProvider::PRIORITY_DEFAULT, "over the rainbow");

    Channel* channel = context->getProvider()->createChannel("test", &channelRequester);
    std::cout << channel->getChannelName() << std::endl;
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
