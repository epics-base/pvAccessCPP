/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdexcept>
#include <pv/rpcServer.h>

#ifdef __vxworks
#include <envLib.h>
using std::string;
inline int setenv(const char *name, const char *value, int overwrite)
{
    string e(name);
    e += "=";
    e += value;
    return putenv(const_cast<char*>(e.c_str()));
}
#endif

using namespace epics::pvData;


namespace epics { namespace pvAccess {


class ChannelRPCServiceImpl : public ChannelRPC
{
    private:
    ChannelRPCRequester::shared_pointer m_channelRPCRequester;
    RPCService::shared_pointer m_rpcService;

    public:
    ChannelRPCServiceImpl(
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        RPCService::shared_pointer const & rpcService) :
        m_channelRPCRequester(channelRPCRequester),
        m_rpcService(rpcService)
    {
    }

    virtual ~ChannelRPCServiceImpl()
    {
        destroy();
    }

    void processRequest(epics::pvData::PVStructure::shared_pointer const & pvArgument, bool lastRequest)
    {
        epics::pvData::PVStructure::shared_pointer result;
        Status status = Status::Ok;
        bool ok = true;
        try
        {
            result = m_rpcService->request(pvArgument);
        }
        catch (RPCRequestException& rre)
        {
            status = Status(rre.getStatus(), rre.what());
            ok = false;
        }
        catch (std::exception& ex)
        {
            status = Status(Status::STATUSTYPE_FATAL, ex.what());
            ok = false;
        }
        catch (...)
        {
            // handle user unexpected errors
            status = Status(Status::STATUSTYPE_FATAL, "Unexpected exception caught while calling RPCService.request(PVStructure).");
            ok = false;
        }
    
        // check null result
        if (ok && result.get() == 0)
        {
            status = Status(Status::STATUSTYPE_FATAL, "RPCService.request(PVStructure) returned null.");
        }
        
       m_channelRPCRequester->requestDone(status, result);
        
        if (lastRequest)
            destroy();

    }

    virtual void request(epics::pvData::PVStructure::shared_pointer const & pvArgument, bool lastRequest)
    {
        processRequest(pvArgument, lastRequest);
    }

    virtual void destroy()
    {
        // noop
    }

    virtual void lock()
    {
        // noop
    }

    virtual void unlock()
    {
        // noop
    }
};




class RPCChannel :
    public virtual Channel
{
private:

    static Status notSupportedStatus;
    static Status destroyedStatus;
    
    AtomicBoolean m_destroyed;
    
    ChannelProvider::shared_pointer m_provider;
    String m_channelName;
    ChannelRequester::shared_pointer m_channelRequester;
    
    RPCService::shared_pointer m_rpcService;

public:    
    POINTER_DEFINITIONS(RPCChannel);
    
    RPCChannel(
           ChannelProvider::shared_pointer const & provider,
           String const & channelName,
           ChannelRequester::shared_pointer const & channelRequester,
           RPCService::shared_pointer const & rpcService) :
           m_provider(provider),
           m_channelName(channelName),
           m_channelRequester(channelRequester),
           m_rpcService(rpcService)
    {
    }

    virtual ~RPCChannel()
    {
        destroy();
    }

    virtual std::tr1::shared_ptr<ChannelProvider> getProvider()
    {
        return m_provider;
    }
    
    virtual epics::pvData::String getRemoteAddress()
    {
        // local
        return getChannelName();
    }

    virtual ConnectionState getConnectionState()
    {
        return isConnected() ?
                Channel::CONNECTED :
                Channel::DESTROYED;
    }

    virtual epics::pvData::String getChannelName()
    {
        return m_channelName;
    }

    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester()
    {
        return m_channelRequester;
    }

    virtual bool isConnected()
    {
        return !m_destroyed.get();
    }


    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & /*pvField*/)
    {
        return none;
    }

    virtual void getField(GetFieldRequester::shared_pointer const & requester,epics::pvData::String const & /*subField*/)
    {
        requester->getDone(notSupportedStatus, epics::pvData::Field::shared_pointer());    
    }

    virtual ChannelProcess::shared_pointer createChannelProcess(
            ChannelProcessRequester::shared_pointer const & channelProcessRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelProcess::shared_pointer nullPtr;
        channelProcessRequester->channelProcessConnect(notSupportedStatus, nullPtr);
        return nullPtr; 
    }

    virtual ChannelGet::shared_pointer createChannelGet(
            ChannelGetRequester::shared_pointer const & channelGetRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelGet::shared_pointer nullPtr;
        channelGetRequester->channelGetConnect(notSupportedStatus, nullPtr,
            epics::pvData::PVStructure::shared_pointer(), epics::pvData::BitSet::shared_pointer());
        return nullPtr; 
    }
            
    virtual ChannelPut::shared_pointer createChannelPut(
            ChannelPutRequester::shared_pointer const & channelPutRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelPut::shared_pointer nullPtr;
        channelPutRequester->channelPutConnect(notSupportedStatus, nullPtr,
            epics::pvData::PVStructure::shared_pointer(), epics::pvData::BitSet::shared_pointer());
        return nullPtr; 
    }
            

    virtual ChannelPutGet::shared_pointer createChannelPutGet(
            ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelPutGet::shared_pointer nullPtr;
        epics::pvData::PVStructure::shared_pointer nullStructure;
        channelPutGetRequester->channelPutGetConnect(notSupportedStatus, nullPtr, nullStructure, nullStructure);
        return nullPtr; 
    }

    virtual ChannelRPC::shared_pointer createChannelRPC(
            ChannelRPCRequester::shared_pointer const & channelRPCRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        // nothing expected to be in pvRequest
        
        if (channelRPCRequester.get() == 0)
            throw std::invalid_argument("channelRPCRequester == null");

        if (m_destroyed.get())
        {
            ChannelRPC::shared_pointer nullPtr;
            channelRPCRequester->channelRPCConnect(destroyedStatus, nullPtr);
            return nullPtr;
        }
        
        ChannelRPC::shared_pointer channelRPCImpl(new ChannelRPCServiceImpl(channelRPCRequester, m_rpcService));
        channelRPCRequester->channelRPCConnect(Status::Ok, channelRPCImpl);
        return channelRPCImpl;
    }

    virtual epics::pvData::Monitor::shared_pointer createMonitor(
            epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        epics::pvData::Monitor::shared_pointer nullPtr;
        monitorRequester->monitorConnect(notSupportedStatus, nullPtr, epics::pvData::Structure::shared_pointer());
        return nullPtr; 
    }

    virtual ChannelArray::shared_pointer createChannelArray(
            ChannelArrayRequester::shared_pointer const & channelArrayRequester,
            epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelArray::shared_pointer nullPtr;
        channelArrayRequester->channelArrayConnect(notSupportedStatus, nullPtr, epics::pvData::PVArray::shared_pointer());
        return nullPtr; 
    }
            

    virtual void printInfo()
    {
        std::cout << "RPCChannel: " << getChannelName() << " [" << Channel::ConnectionStateNames[getConnectionState()] << "]" << std::endl;
    }

    virtual void printInfo(epics::pvData::StringBuilder out)
    {
        *out += "RPCChannel: ";
        *out += getChannelName();
        *out += " [";
        *out += Channel::ConnectionStateNames[getConnectionState()];
        *out += "]";
    }

    virtual String getRequesterName()
    {
        return getChannelName();
    }
    
    virtual void message(String const & message,MessageType messageType)
    {
        // just delegate
        m_channelRequester->message(message, messageType);
    }

    virtual void destroy()
    {
        m_destroyed.set();   
    } 
};

Status RPCChannel::notSupportedStatus(Status::STATUSTYPE_ERROR, "only channelRPC requests are supported by this channel");
Status RPCChannel::destroyedStatus(Status::STATUSTYPE_ERROR, "channel destroyed");



class RPCChannelProvider :
    public virtual ChannelProvider, 
    public virtual ChannelFind,
    public std::tr1::enable_shared_from_this<RPCChannelProvider> {

public:
    POINTER_DEFINITIONS(RPCChannelProvider);
    
    static String PROVIDER_NAME;

    static Status noSuchChannelStatus;
    
    // TODO thread pool support
    
    RPCChannelProvider() {
    }

    virtual String getProviderName() {
        return PROVIDER_NAME;
    }

    virtual std::tr1::shared_ptr<ChannelProvider> getChannelProvider()
    {
        return shared_from_this();
    }
    
    virtual void cancelChannelFind() {}

    virtual void destroy() {}
    
    virtual ChannelFind::shared_pointer channelFind(epics::pvData::String const & channelName,
                        ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        bool found;
        {
            Lock guard(m_mutex);
            found = (m_services.find(channelName) != m_services.end());
        }
        ChannelFind::shared_pointer thisPtr(shared_from_this());
        channelFindRequester->channelFindResult(Status::Ok, thisPtr, found);
        return thisPtr;
    }


     virtual Channel::shared_pointer createChannel(
            epics::pvData::String const & channelName,
            ChannelRequester::shared_pointer const & channelRequester,
            short /*priority*/)
    {
        RPCServiceMap::const_iterator iter;
        {
            Lock guard(m_mutex);
            iter = m_services.find(channelName);
        }
        
        if (iter == m_services.end())
        {
            Channel::shared_pointer nullChannel;
            channelRequester->channelCreated(noSuchChannelStatus, nullChannel);
            return nullChannel;
        }
               
        Channel::shared_pointer rpcChannel(new RPCChannel(
                shared_from_this(),
                channelName,
                channelRequester,
                iter->second));
        channelRequester->channelCreated(Status::Ok, rpcChannel);
        return rpcChannel;
    }

    virtual Channel::shared_pointer createChannel(
        epics::pvData::String const & /*channelName*/,
        ChannelRequester::shared_pointer const & /*channelRequester*/,
        short /*priority*/,
        epics::pvData::String const & /*address*/)
    {
        // this will never get called by the pvAccess server
        throw std::runtime_error("not supported");
    }

    void registerService(String const & serviceName, RPCService::shared_pointer const & service)
    {
        Lock guard(m_mutex);
        m_services[serviceName] = service;
    }
    
    void unregisterService(String const & serviceName)
    {
        Lock guard(m_mutex);
        m_services.erase(serviceName);
    }

private:    
    typedef std::map<String, RPCService::shared_pointer> RPCServiceMap;
    RPCServiceMap m_services;
    epics::pvData::Mutex m_mutex;
};

String RPCChannelProvider::PROVIDER_NAME("rpcService");
Status RPCChannelProvider::noSuchChannelStatus(Status::STATUSTYPE_ERROR, "no such channel");



class RPCChannelProviderFactory : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(RPCChannelProviderFactory);

    RPCChannelProviderFactory() :
        m_channelProviderImpl(new RPCChannelProvider())
    {
    }

    virtual epics::pvData::String getFactoryName()
    {
        return RPCChannelProvider::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        return m_channelProviderImpl;
    }

    virtual ChannelProvider::shared_pointer newInstance()
    {
        return ChannelProvider::shared_pointer(new RPCChannelProvider());
    }

private:
    RPCChannelProvider::shared_pointer m_channelProviderImpl;
};


RPCServer::RPCServer()
{
    // TODO factory is never deregistered, multiple RPCServer instances create multiple factories, etc.
    m_channelProviderFactory.reset(new RPCChannelProviderFactory());
    registerChannelProviderFactory(m_channelProviderFactory);

    m_channelProviderImpl = m_channelProviderFactory->sharedInstance();

    setenv("EPICS_PVAS_PROVIDER_NAMES", m_channelProviderImpl->getProviderName().c_str(), 1);
    m_serverContext = ServerContextImpl::create();
    
    m_serverContext->initialize(getChannelAccess());
}

RPCServer::~RPCServer()
{
    // multiple destroy call is OK
    destroy();
}

void RPCServer::printInfo()
{
    std::cout << m_serverContext->getVersion().getVersionString() << std::endl;
    m_serverContext->printInfo();
}

void RPCServer::run(int seconds)
{
    m_serverContext->run(seconds);
}

void RPCServer::destroy()
{
    m_serverContext->destroy();
}

void RPCServer::registerService(String const & serviceName, RPCService::shared_pointer const & service)
{
    std::tr1::dynamic_pointer_cast<RPCChannelProvider>(m_channelProviderImpl)->registerService(serviceName, service);
}

void RPCServer::unregisterService(String const & serviceName)
{
    std::tr1::dynamic_pointer_cast<RPCChannelProvider>(m_channelProviderImpl)->unregisterService(serviceName);
}

}}
