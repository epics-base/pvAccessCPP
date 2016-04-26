/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsSignal.h>

#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/thread.h>

#define epicsExportSharedSymbols
#include <pv/responseHandlers.h>
#include <pv/logger.h>
#include <pv/serverContext.h>
#include <pv/security.h>

using namespace std;
using namespace epics::pvData;
using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;

namespace epics {
namespace pvAccess {

const char* ServerContextImpl::StateNames[] = { "NOT_INITIALIZED", "INITIALIZED", "RUNNING", "SHUTDOWN", "DESTROYED"};
const Version ServerContextImpl::VERSION("pvAccess Server", "cpp",
        EPICS_PVA_MAJOR_VERSION, EPICS_PVA_MINOR_VERSION, EPICS_PVA_MAINTENANCE_VERSION, EPICS_PVA_DEVELOPMENT_FLAG);

ServerContextImpl::ServerContextImpl():
    _state(NOT_INITIALIZED),
    _beaconAddressList(),
    _ignoreAddressList(),
    _autoBeaconAddressList(true),
    _beaconPeriod(15.0),
    _broadcastPort(PVA_BROADCAST_PORT),
    _serverPort(PVA_SERVER_PORT),
    _receiveBufferSize(MAX_TCP_RECV),
    _timer(),
    _beaconEmitter(),
    _acceptor(),
    _transportRegistry(),
    _channelProviderRegistry(),
    _channelProviderNames(PVACCESS_DEFAULT_PROVIDER),
    _channelProviders(),
    _beaconServerStatusProvider(),
    _startTime()

{
    epicsTimeGetCurrent(&_startTime);

    // TODO maybe there is a better place for this (when there will be some factory)
    epicsSignalInstallSigAlarmIgnore ();
    epicsSignalInstallSigPipeIgnore ();

    generateGUID();
    initializeLogger();
}

ServerContextImpl::shared_pointer ServerContextImpl::create()
{
    ServerContextImpl::shared_pointer thisPointer(new ServerContextImpl());
    thisPointer->loadConfiguration();
    return thisPointer;
}

ServerContextImpl::shared_pointer ServerContextImpl::create(
    const Configuration::shared_pointer& conf)
{
    ServerContextImpl::shared_pointer thisPointer(new ServerContextImpl());
    thisPointer->configuration = conf;
    thisPointer->loadConfiguration();
    return thisPointer;
}

ServerContextImpl::~ServerContextImpl()
{
    dispose();
}

const GUID& ServerContextImpl::getGUID()
{
    return _guid;
}

const Version& ServerContextImpl::getVersion()
{
    return ServerContextImpl::VERSION;
}

/*
#ifdef WIN32
  UUID uuid;
  UuidCreate ( &uuid );
#else
  uuid_t uuid;
  uuid_generate_random ( uuid );
#endif
*/

void ServerContextImpl::generateGUID()
{
    // TODO use UUID
    epics::pvData::TimeStamp startupTime;
    startupTime.getCurrent();

    ByteBuffer buffer(_guid.value, sizeof(_guid.value));
    buffer.putLong(startupTime.getSecondsPastEpoch());
    buffer.putInt(startupTime.getNanoseconds());
}

void ServerContextImpl::initializeLogger()
{
    //createFileLogger("serverContextImpl.log");
}

Configuration::shared_pointer ServerContextImpl::getConfiguration()
{
    Lock guard(_mutex);
    if (configuration.get() == 0)
    {
        ConfigurationProvider::shared_pointer configurationProvider = ConfigurationFactory::getProvider();
        configuration = configurationProvider->getConfiguration("pvAccess-server");
        if (configuration.get() == 0)
        {
            configuration = configurationProvider->getConfiguration("system");
        }
    }
    return configuration;
}

/**
 * Load configuration.
 */
void ServerContextImpl::loadConfiguration()
{
    Configuration::shared_pointer config = getConfiguration();

    // TODO for now just a simple switch
    int32 debugLevel = config->getPropertyAsInteger(PVACCESS_DEBUG, 0);
    if (debugLevel > 0)
        SET_LOG_LEVEL(logLevelDebug);

    // TODO multiple addresses
    _ifaceAddr.ia.sin_family = AF_INET;
    _ifaceAddr.ia.sin_addr.s_addr = htonl(INADDR_ANY);
    _ifaceAddr.ia.sin_port = 0;
    config->getPropertyAsAddress("EPICS_PVAS_INTF_ADDR_LIST", &_ifaceAddr);

    _beaconAddressList = config->getPropertyAsString("EPICS_PVA_ADDR_LIST", _beaconAddressList);
    _beaconAddressList = config->getPropertyAsString("EPICS_PVAS_BEACON_ADDR_LIST", _beaconAddressList);

    _autoBeaconAddressList = config->getPropertyAsBoolean("EPICS_PVA_AUTO_ADDR_LIST", _autoBeaconAddressList);
    _autoBeaconAddressList = config->getPropertyAsBoolean("EPICS_PVAS_AUTO_BEACON_ADDR_LIST", _autoBeaconAddressList);

    _beaconPeriod = config->getPropertyAsFloat("EPICS_PVA_BEACON_PERIOD", _beaconPeriod);
    _beaconPeriod = config->getPropertyAsFloat("EPICS_PVAS_BEACON_PERIOD", _beaconPeriod);

    _serverPort = config->getPropertyAsInteger("EPICS_PVA_SERVER_PORT", _serverPort);
    _serverPort = config->getPropertyAsInteger("EPICS_PVAS_SERVER_PORT", _serverPort);
    _ifaceAddr.ia.sin_port = htons(_serverPort);

    _broadcastPort = config->getPropertyAsInteger("EPICS_PVA_BROADCAST_PORT", _broadcastPort);
    _broadcastPort = config->getPropertyAsInteger("EPICS_PVAS_BROADCAST_PORT", _broadcastPort);

    _receiveBufferSize = config->getPropertyAsInteger("EPICS_PVA_MAX_ARRAY_BYTES", _receiveBufferSize);
    _receiveBufferSize = config->getPropertyAsInteger("EPICS_PVAS_MAX_ARRAY_BYTES", _receiveBufferSize);

    _channelProviderNames = config->getPropertyAsString("EPICS_PVA_PROVIDER_NAMES", _channelProviderNames);
    _channelProviderNames = config->getPropertyAsString("EPICS_PVAS_PROVIDER_NAMES", _channelProviderNames);

    //
    // introspect network interfaces
    //

    osiSockAttach();

    SOCKET sock = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);
    if (!sock) {
        THROW_BASE_EXCEPTION("Failed to create a socket needed to introspect network interfaces.");
    }

    if (discoverInterfaces(_ifaceList, sock, &_ifaceAddr))
    {
        THROW_BASE_EXCEPTION("Failed to introspect network interfaces.");
    }
    else if (_ifaceList.size() == 0)
    {
        THROW_BASE_EXCEPTION("No (specified) network interface(s) available.");
    }
    epicsSocketDestroy(sock);
}

bool ServerContextImpl::isChannelProviderNamePreconfigured()
{
    Configuration::shared_pointer config = getConfiguration();
    return config->hasProperty("EPICS_PVA_PROVIDER_NAMES") || config->hasProperty("EPICS_PVAS_PROVIDER_NAMES");
}

void ServerContextImpl::initialize(ChannelProviderRegistry::shared_pointer const & channelProviderRegistry)
{
    Lock guard(_mutex);
    if (!channelProviderRegistry.get())
    {
        THROW_BASE_EXCEPTION("channelProviderRegistry == NULL");
    }

    if (_state == DESTROYED)
    {
        THROW_BASE_EXCEPTION("Context destroyed.");
    }
    else if (_state != NOT_INITIALIZED)
    {
        THROW_BASE_EXCEPTION("Context already initialized.");
    }

    _channelProviderRegistry = channelProviderRegistry;


    // user all providers
    if (_channelProviderNames == PVACCESS_ALL_PROVIDERS)
    {
        _channelProviderNames.resize(0); // VxWorks 5.5 omits clear()

        std::auto_ptr<ChannelProviderRegistry::stringVector_t> names = _channelProviderRegistry->getProviderNames();
        for (ChannelProviderRegistry::stringVector_t::iterator iter = names->begin(); iter != names->end(); iter++)
        {
            ChannelProvider::shared_pointer channelProvider = _channelProviderRegistry->getProvider(*iter);
            if (channelProvider)
            {
                _channelProviders.push_back(channelProvider);

                // compile a list
                if (!_channelProviderNames.empty())
                    _channelProviderNames += ' ';
                _channelProviderNames += *iter;
            }
        }
    }
    else
    {
        // split space separated names
        std::stringstream ss(_channelProviderNames);
        std::string providerName;
        while (std::getline(ss, providerName, ' '))
        {
            ChannelProvider::shared_pointer channelProvider = _channelProviderRegistry->getProvider(providerName);
            if (channelProvider)
                _channelProviders.push_back(channelProvider);
        }
    }

    //_channelProvider = _channelProviderRegistry->getProvider(_channelProviderNames);
    if (_channelProviders.size() == 0)
    {
        std::string msg = "None of the specified channel providers are available: " + _channelProviderNames + ".";
        THROW_BASE_EXCEPTION(msg.c_str());
    }

    internalInitialize();

    _state = INITIALIZED;
}

void ServerContextImpl::internalInitialize()
{
    // already called in loadConfiguration
    //osiSockAttach();

    _timer.reset(new Timer("pvAccess-server timer", lowerPriority));
    _transportRegistry.reset(new TransportRegistry());

    ServerContextImpl::shared_pointer thisServerContext = shared_from_this();
    _responseHandler.reset(new ServerResponseHandler(thisServerContext));

    _acceptor.reset(new BlockingTCPAcceptor(thisServerContext, _responseHandler, _ifaceAddr, _receiveBufferSize));
    _serverPort = ntohs(_acceptor->getBindAddress()->ia.sin_port);

    // setup broadcast UDP transport
    initializeBroadcastTransport();

    // TODO introduce "tcp" a constant
    _beaconEmitter.reset(new BeaconEmitter("tcp", _broadcastTransport, thisServerContext));
}

void ServerContextImpl::initializeBroadcastTransport()
{
    initializeUDPTransports(true, _udpTransports, _ifaceList, _responseHandler, _broadcastTransport,
                            _broadcastPort, _autoBeaconAddressList, _beaconAddressList, _ignoreAddressList);
}

void ServerContextImpl::run(int32 seconds)
{
    if (seconds < 0)
    {
        THROW_BASE_EXCEPTION("seconds cannot be negative.");
    }

    {
        Lock guard(_mutex);

        if (_state == NOT_INITIALIZED)
        {
            THROW_BASE_EXCEPTION("Context not initialized.");
        }
        else if (_state == DESTROYED)
        {
            THROW_BASE_EXCEPTION("Context destroyed.");
        }
        else if (_state == RUNNING)
        {
            THROW_BASE_EXCEPTION("Context is already running.");
        }
        else if (_state == SHUTDOWN)
        {
            THROW_BASE_EXCEPTION("Context was shutdown.");
        }

        _state = RUNNING;
    }

    // run...
    _beaconEmitter->start();

    //TODO review this
    if(seconds == 0)
    {
        _runEvent.wait();
    }
    else
    {
        _runEvent.wait(seconds);
    }

    {
        Lock guard(_mutex);
        _state = SHUTDOWN;
    }
}

void ServerContextImpl::shutdown()
{
    Lock guard(_mutex);
    if(_state == DESTROYED)
    {
        THROW_BASE_EXCEPTION("Context already destroyed.");
    }

    // notify to stop running...
    _runEvent.signal();
}

void ServerContextImpl::destroy()
{
    Lock guard(_mutex);
    if (_state == DESTROYED)
    {
        // silent return
        return;
        // exception is not OK, since we use
        // shared_pointer-s auto-cleanup/destruction
        // THROW_BASE_EXCEPTION("Context already destroyed.");
    }

    // shutdown if not already
    shutdown();

    // go into destroyed state ASAP
    _state = DESTROYED;

    internalDestroy();
}


void ServerContextImpl::internalDestroy()
{
    // stop responding to search requests
    for (BlockingUDPTransportVector::const_iterator iter = _udpTransports.begin();
            iter != _udpTransports.end(); iter++)
        (*iter)->close();
    _udpTransports.clear();

    // stop emitting beacons
    if (_beaconEmitter)
    {
        _beaconEmitter->destroy();
        _beaconEmitter.reset();
    }

    // close UDP sent transport
    if (_broadcastTransport)
    {
        _broadcastTransport->close();
        _broadcastTransport.reset();
    }

    // stop accepting connections
    if (_acceptor)
    {
        _acceptor->destroy();
        _acceptor.reset();
    }

    // this will also destroy all channels
    destroyAllTransports();
}

void ServerContextImpl::destroyAllTransports()
{

    // not initialized yet
    if (!_transportRegistry.get())
    {
        return;
    }

    std::auto_ptr<TransportRegistry::transportVector_t> transports = _transportRegistry->toArray();
    if (transports.get() == 0)
        return;

    int size = (int)transports->size();
    if (size == 0)
        return;

    LOG(logLevelInfo, "Server context still has %d transport(s) active and closing...", size);

    for (int i = 0; i < size; i++)
    {
        Transport::shared_pointer transport = (*transports)[i];
        try
        {
            transport->close();
        }
        catch (std::exception &e)
        {
            // do all exception safe, log in case of an error
            LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what());
        }
        catch (...)
        {
            // do all exception safe, log in case of an error
            LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__);
        }
    }

    // now clear all (release)
    _transportRegistry->clear();

}

void ServerContextImpl::printInfo()
{
    printInfo(cout);
}

void ServerContextImpl::printInfo(ostream& str)
{
    Lock guard(_mutex);
    str << "VERSION : " << getVersion().getVersionString() << endl \
        << "PROVIDER_NAMES : " << _channelProviderNames << endl \
        << "BEACON_ADDR_LIST : " << _beaconAddressList << endl \
        << "AUTO_BEACON_ADDR_LIST : " << _autoBeaconAddressList << endl \
        << "BEACON_PERIOD : " << _beaconPeriod << endl \
        << "BROADCAST_PORT : " << _broadcastPort << endl \
        << "SERVER_PORT : " << _serverPort << endl \
        << "RCV_BUFFER_SIZE : " << _receiveBufferSize << endl \
        << "IGNORE_ADDR_LIST: " << _ignoreAddressList << endl \
        << "INTF_ADDR_LIST : " << inetAddressToString(_ifaceAddr, false) << endl \
        << "STATE : " << ServerContextImpl::StateNames[_state] << endl;
}

void ServerContextImpl::dispose()
{
    try
    {
        destroy();
    }
    catch(std::exception& e)
    {
        std::cerr<<"Error in: ServerContextImpl::dispose: "<<e.what()<<"\n";
    }
    catch(...)
    {
        std::cerr<<"Oh no, something when wrong in ServerContextImpl::dispose!\n";
    }
}

void ServerContextImpl::setBeaconServerStatusProvider(BeaconServerStatusProvider::shared_pointer const & beaconServerStatusProvider)
{
    _beaconServerStatusProvider = beaconServerStatusProvider;
}

bool ServerContextImpl::isInitialized()
{
    Lock guard(_mutex);
    return _state == INITIALIZED || _state == RUNNING || _state == SHUTDOWN;
}

bool ServerContextImpl::isDestroyed()
{
    Lock guard(_mutex);
    return _state == DESTROYED;
}

std::string ServerContextImpl::getBeaconAddressList()
{
    return _beaconAddressList;
}

bool ServerContextImpl::isAutoBeaconAddressList()
{
    return _autoBeaconAddressList;
}

float ServerContextImpl::getBeaconPeriod()
{
    return _beaconPeriod;
}

int32 ServerContextImpl::getReceiveBufferSize()
{
    return _receiveBufferSize;
}

int32 ServerContextImpl::getServerPort()
{
    return _serverPort;
}

int32 ServerContextImpl::getBroadcastPort()
{
    return _broadcastPort;
}

std::string ServerContextImpl::getIgnoreAddressList()
{
    return _ignoreAddressList;
}

BeaconServerStatusProvider::shared_pointer ServerContextImpl::getBeaconServerStatusProvider()
{
    return _beaconServerStatusProvider;
}

osiSockAddr* ServerContextImpl::getServerInetAddress()
{
    if(_acceptor.get())
    {
        return const_cast<osiSockAddr*>(_acceptor->getBindAddress());
    }
    return NULL;
}

BlockingUDPTransport::shared_pointer ServerContextImpl::getBroadcastTransport()
{
    return _broadcastTransport;
}

ChannelProviderRegistry::shared_pointer ServerContextImpl::getChannelProviderRegistry()
{
    return _channelProviderRegistry;
}

std::string ServerContextImpl::getChannelProviderName()
{
    return _channelProviderNames;
}

// NOTE: not synced
void ServerContextImpl::setChannelProviderName(std::string channelProviderName)
{
    if (_state != NOT_INITIALIZED)
        throw std::logic_error("must be called before initialize");
    _channelProviderNames = channelProviderName;
}

std::vector<ChannelProvider::shared_pointer>& ServerContextImpl::getChannelProviders()
{
    return _channelProviders;
}

Timer::shared_pointer ServerContextImpl::getTimer()
{
    return _timer;
}

TransportRegistry::shared_pointer ServerContextImpl::getTransportRegistry()
{
    return _transportRegistry;
}

Channel::shared_pointer ServerContextImpl::getChannel(pvAccessID /*id*/)
{
    // not used
    return Channel::shared_pointer();
}

Transport::shared_pointer ServerContextImpl::getSearchTransport()
{
    // not used
    return Transport::shared_pointer();
}

void ServerContextImpl::newServerDetected()
{
    // not used
}

epicsTimeStamp& ServerContextImpl::getStartTime()
{
    return _startTime;
}


std::map<std::string, std::tr1::shared_ptr<SecurityPlugin> >& ServerContextImpl::getSecurityPlugins()
{
    return SecurityPluginRegistry::instance().getServerSecurityPlugins();
}



struct ThreadRunnerParam {
    ServerContextImpl::shared_pointer ctx;
    int timeToRun;
};

static void threadRunner(void* usr)
{
    ThreadRunnerParam* pusr = static_cast<ThreadRunnerParam*>(usr);
    ThreadRunnerParam param = *pusr;
    delete pusr;

    param.ctx->run(param.timeToRun);
}



ServerContext::shared_pointer startPVAServer(std::string const & providerNames, int timeToRun, bool runInSeparateThread, bool printInfo)
{
    ServerContextImpl::shared_pointer ctx = ServerContextImpl::create();

    // do not override configuration
    if (!ctx->isChannelProviderNamePreconfigured())
        ctx->setChannelProviderName(providerNames);

    ChannelProviderRegistry::shared_pointer channelProviderRegistry = getChannelProviderRegistry();
    ctx->initialize(channelProviderRegistry);

    if (printInfo)
        ctx->printInfo();


    if (runInSeparateThread)
    {
        // delete left to the thread
        auto_ptr<ThreadRunnerParam> param(new ThreadRunnerParam());
        param->ctx = ctx;
        param->timeToRun = timeToRun;

        // TODO can this fail?
        epicsThreadCreate("startPVAServer",
                          epicsThreadPriorityMedium,
                          epicsThreadGetStackSize(epicsThreadStackBig),
                          threadRunner, param.get());

        param.release();
    }
    else
    {
        ctx->run(timeToRun);
    }

    return ctx;
}

}
}
