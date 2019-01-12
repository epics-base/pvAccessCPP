/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsSignal.h>

#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/thread.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include <pv/responseHandlers.h>
#include <pv/logger.h>
#include <pv/serverContextImpl.h>
#include <pv/codec.h>
#include <pv/security.h>

using namespace std;
using namespace epics::pvData;
using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;

namespace epics {
namespace pvAccess {

const Version ServerContextImpl::VERSION("pvAccess Server", "cpp",
        EPICS_PVA_MAJOR_VERSION, EPICS_PVA_MINOR_VERSION, EPICS_PVA_MAINTENANCE_VERSION, EPICS_PVA_DEVELOPMENT_FLAG);

size_t ServerContextImpl::num_instances;

ServerContextImpl::ServerContextImpl():
    _beaconAddressList(),
    _ignoreAddressList(),
    _autoBeaconAddressList(true),
    _beaconPeriod(15.0),
    _broadcastPort(PVA_BROADCAST_PORT),
    _serverPort(PVA_SERVER_PORT),
    _receiveBufferSize(MAX_TCP_RECV),
    _timer(new Timer("PVAS timers", lowerPriority)),
    _beaconEmitter(),
    _acceptor(),
    _transportRegistry(),
    _channelProviders(),
    _beaconServerStatusProvider(),
    _startTime()
{
    REFTRACE_INCREMENT(num_instances);

    epicsTimeGetCurrent(&_startTime);

    // TODO maybe there is a better place for this (when there will be some factory)
    epicsSignalInstallSigAlarmIgnore ();
    epicsSignalInstallSigPipeIgnore ();

    generateGUID();
}

ServerContextImpl::~ServerContextImpl()
{
    try
    {
        shutdown();
    }
    catch(std::exception& e)
    {
        std::cerr<<"Error in: ServerContextImpl::~ServerContextImpl: "<<e.what()<<"\n";
    }
    REFTRACE_DECREMENT(num_instances);
}

const ServerGUID& ServerContextImpl::getGUID()
{
    return _guid;
}

const Version& ServerContextImpl::getVersion()
{
    return ServerContextImpl::VERSION;
}

void ServerContextImpl::generateGUID()
{
    // TODO use UUID
    epics::pvData::TimeStamp startupTime;
    startupTime.getCurrent();

    ByteBuffer buffer(_guid.value, sizeof(_guid.value));
    buffer.putLong(startupTime.getSecondsPastEpoch());
    buffer.putInt(startupTime.getNanoseconds());
}

Configuration::const_shared_pointer ServerContextImpl::getConfiguration()
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
    Configuration::const_shared_pointer config = configuration;

    // TODO for now just a simple switch
    int32 debugLevel = config->getPropertyAsInteger(PVACCESS_DEBUG, 0);
    if (debugLevel > 0)
        SET_LOG_LEVEL(logLevelDebug);

    // TODO multiple addresses
    memset(&_ifaceAddr, 0, sizeof(_ifaceAddr));
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

    if(_channelProviders.empty()) {
        std::string providers = config->getPropertyAsString("EPICS_PVAS_PROVIDER_NAMES", PVACCESS_DEFAULT_PROVIDER);

        ChannelProviderRegistry::shared_pointer reg(ChannelProviderRegistry::servers());

        if (providers == PVACCESS_ALL_PROVIDERS)
        {
            providers.resize(0); // VxWorks 5.5 omits clear()

            std::set<std::string> names;
            reg->getProviderNames(names);
            for (std::set<std::string>::const_iterator iter = names.begin(); iter != names.end(); iter++)
            {
                ChannelProvider::shared_pointer channelProvider = reg->getProvider(*iter);
                if (channelProvider) {
                    _channelProviders.push_back(channelProvider);
                } else {
                    LOG(logLevelDebug, "Provider '%s' all, but missing\n", iter->c_str());
                }
            }

        } else {
            // split space separated names
            std::stringstream ss(providers);
            std::string providerName;
            while (std::getline(ss, providerName, ' '))
            {
                ChannelProvider::shared_pointer channelProvider(reg->getProvider(providerName));
                if (channelProvider) {
                    _channelProviders.push_back(channelProvider);
                } else {
                    LOG(logLevelWarn, "Requested provider '%s' not found", providerName.c_str());
                }
            }
        }
    }

    if(_channelProviders.empty())
        LOG(logLevelError, "ServerContext configured with not Providers will do nothing!\n");

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

Configuration::shared_pointer
ServerContextImpl::getCurrentConfig()
{
    ConfigurationBuilder B;

    std::ostringstream providerName;
    for(size_t i=0; i<_channelProviders.size(); i++) {
        if(i>0)
            providerName<<" ";
        providerName<<_channelProviders[i]->getProviderName();
    }

#define SET(K, V) B.add(K, V);

    {
        char buf[50];
        ipAddrToA(&_ifaceAddr.ia, buf, sizeof(buf));
        buf[sizeof(buf)-1] = '\0';
        SET("EPICS_PVAS_INTF_ADDR_LIST", buf);
    }

    SET("EPICS_PVAS_BEACON_ADDR_LIST", getBeaconAddressList());
    SET("EPICS_PVA_ADDR_LIST", getBeaconAddressList());

    SET("EPICS_PVAS_AUTO_BEACON_ADDR_LIST",
                         isAutoBeaconAddressList() ? "YES" : "NO");
    SET("EPICS_PVA_AUTO_ADDR_LIST",
                         isAutoBeaconAddressList() ? "YES" : "NO");

    SET("EPICS_PVAS_BEACON_PERIOD", getBeaconPeriod());
    SET("EPICS_PVA_BEACON_PERIOD", getBeaconPeriod());

    SET("EPICS_PVAS_SERVER_PORT", getServerPort());
    SET("EPICS_PVA_SERVER_PORT", getServerPort());

    SET("EPICS_PVAS_BROADCAST_PORT", getBroadcastPort());
    SET("EPICS_PVA_BROADCAST_PORT", getBroadcastPort());

    SET("EPICS_PVAS_MAX_ARRAY_BYTES", getReceiveBufferSize());
    SET("EPICS_PVA_MAX_ARRAY_BYTES", getReceiveBufferSize());

    SET("EPICS_PVAS_PROVIDER_NAMES", providerName.str());

#undef SET

    return B.push_map().build();
}

bool ServerContextImpl::isChannelProviderNamePreconfigured()
{
    Configuration::const_shared_pointer config = getConfiguration();
    return config->hasProperty("EPICS_PVAS_PROVIDER_NAMES");
}

void ServerContextImpl::initialize()
{
    Lock guard(_mutex);

    // already called in loadConfiguration
    //osiSockAttach();

    ServerContextImpl::shared_pointer thisServerContext = shared_from_this();
    // we create reference cycles here which are broken by our shutdown() method,
    _responseHandler.reset(new ServerResponseHandler(thisServerContext));

    _acceptor.reset(new BlockingTCPAcceptor(thisServerContext, _responseHandler, _ifaceAddr, _receiveBufferSize));
    _serverPort = ntohs(_acceptor->getBindAddress()->ia.sin_port);

    // setup broadcast UDP transport
    initializeUDPTransports(true, _udpTransports, _ifaceList, _responseHandler, _broadcastTransport,
                            _broadcastPort, _autoBeaconAddressList, _beaconAddressList, _ignoreAddressList);

    _beaconEmitter.reset(new BeaconEmitter("tcp", _broadcastTransport, thisServerContext));

    _beaconEmitter->start();
}

void ServerContextImpl::run(uint32 seconds)
{
    //TODO review this
    if(seconds == 0)
    {
        _runEvent.wait();
    }
    else
    {
        _runEvent.wait(seconds);
    }
}

#define LEAK_CHECK(PTR, NAME) if((PTR) && !(PTR).unique()) { std::cerr<<"Leaking ServerContext " NAME " use_count="<<(PTR).use_count()<<"\n"<<show_referrers(PTR, false);}

void ServerContextImpl::shutdown()
{
    if(!_timer)
        return; // already shutdown

    // abort pending timers and prevent new timers from starting
    _timer->close();

    // stop responding to search requests
    for (BlockingUDPTransportVector::const_iterator iter = _udpTransports.begin();
            iter != _udpTransports.end(); iter++)
    {
        const BlockingUDPTransport::shared_pointer& transport = *iter;
        // joins worker thread
        transport->close();
        // _udpTransports contains _broadcastTransport
        // _broadcastTransport is referred to be _beaconEmitter
        if(transport!=_broadcastTransport)
            LEAK_CHECK(transport, "udp transport")
    }
    _udpTransports.clear();

    // stop emitting beacons
    if (_beaconEmitter)
    {
        _beaconEmitter->destroy();
        LEAK_CHECK(_beaconEmitter, "_beaconEmitter")
        _beaconEmitter.reset();
    }

    // close UDP sent transport
    if (_broadcastTransport)
    {
        _broadcastTransport->close();
        LEAK_CHECK(_broadcastTransport, "_broadcastTransport")
        _broadcastTransport.reset();
    }

    // stop accepting connections
    if (_acceptor)
    {
        _acceptor->destroy();
        LEAK_CHECK(_acceptor, "_acceptor")
        _acceptor.reset();
    }

    // this will also destroy all channels
    _transportRegistry.clear();

    // drop timer queue
    LEAK_CHECK(_timer, "_timer")
    _timer.reset();

    // response handlers hold strong references to us,
    // so must break the cycles
    LEAK_CHECK(_responseHandler, "_responseHandler")
    _responseHandler.reset();

    _runEvent.signal();
}

void ServerContext::printInfo(int lvl)
{
    printInfo(cout, lvl);
}

void ServerContextImpl::printInfo(ostream& str, int lvl)
{
    if(lvl==0) {
        Lock guard(_mutex);
        str << "VERSION : " << getVersion().getVersionString() << endl
            << "PROVIDER_NAMES : ";
        for(std::vector<ChannelProvider::shared_pointer>::const_iterator it = _channelProviders.begin();
            it != _channelProviders.end(); ++it)
        {
            str<<(*it)->getProviderName()<<", ";
        }
        str << endl
            << "BEACON_ADDR_LIST : " << _beaconAddressList << endl
            << "AUTO_BEACON_ADDR_LIST : " << _autoBeaconAddressList << endl
            << "BEACON_PERIOD : " << _beaconPeriod << endl
            << "BROADCAST_PORT : " << _broadcastPort << endl
            << "SERVER_PORT : " << _serverPort << endl
            << "RCV_BUFFER_SIZE : " << _receiveBufferSize << endl
            << "IGNORE_ADDR_LIST: " << _ignoreAddressList << endl
            << "INTF_ADDR_LIST : " << inetAddressToString(_ifaceAddr, false) << endl;

    } else {
        // lvl >= 1

        TransportRegistry::transportVector_t transports;
        _transportRegistry.toArray(transports);

        str<<"Clients:\n";
        for(TransportRegistry::transportVector_t::const_iterator it(transports.begin()), end(transports.end());
            it!=end; ++it)
        {
            const Transport::shared_pointer& transport(*it);

            str<<"client "<<transport->getType()<<"://"<<transport->getRemoteName()
              <<" ver="<<unsigned(transport->getRevision())
              <<" "<<(transport->isClosed()?"closed!":"");

            const detail::BlockingServerTCPTransportCodec *casTransport = dynamic_cast<const detail::BlockingServerTCPTransportCodec*>(transport.get());

            if(casTransport) {
              str<<" "<<(casTransport ? casTransport->getChannelCount() : size_t(-1))<<" channels";
            }

            str<<"\n";

            if(!casTransport || lvl<2)
                return;
            // lvl >= 2

            typedef std::vector<ServerChannel::shared_pointer> channels_t;
            channels_t channels;
            casTransport->getChannels(channels);

            for(channels_t::const_iterator it(channels.begin()), end(channels.end()); it!=end; ++it)
            {
                const ServerChannel *channel(static_cast<const ServerChannel*>(it->get()));
                const Channel::shared_pointer& providerChan(channel->getChannel());
                if(!providerChan)
                    continue;

                str<<"  "<<providerChan->getChannelName()
                   <<(providerChan->isConnected()?"":" closed");
                if(lvl>=3) {
                    str<<"\t: ";
                    providerChan->printInfo(str);
                }
                str<<"\n";
            }
        }
    }
}

void ServerContextImpl::setBeaconServerStatusProvider(BeaconServerStatusProvider::shared_pointer const & beaconServerStatusProvider)
{
    _beaconServerStatusProvider = beaconServerStatusProvider;
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

const osiSockAddr* ServerContextImpl::getServerInetAddress()
{
    if(_acceptor.get())
    {
        return const_cast<osiSockAddr*>(_acceptor->getBindAddress());
    }
    return NULL;
}

const BlockingUDPTransport::shared_pointer& ServerContextImpl::getBroadcastTransport()
{
    return _broadcastTransport;
}

const std::vector<ChannelProvider::shared_pointer>& ServerContextImpl::getChannelProviders()
{
    return _channelProviders;
}

Timer::shared_pointer ServerContextImpl::getTimer()
{
    return _timer;
}

epics::pvAccess::TransportRegistry* ServerContextImpl::getTransportRegistry()
{
    return &_transportRegistry;
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

ServerContext::shared_pointer startPVAServer(std::string const & providerNames, int timeToRun, bool runInSeparateThread, bool printInfo)
{
    ServerContext::shared_pointer ret(ServerContext::create(ServerContext::Config()
                                 .config(ConfigurationBuilder()
                                         .add("EPICS_PVAS_PROVIDER_NAMES", providerNames)
                                         .push_map()
                                         .push_env() // environment takes precidence (top of stack)
                                         .build())));
    if(printInfo)
        ret->printInfo();

    if(!runInSeparateThread) {
        ret->run(timeToRun);
        ret->shutdown();
    } else if(timeToRun!=0) {
        LOG(logLevelWarn, "startPVAServer() timeToRun!=0 only supported when runInSeparateThread==false\n");
    }

    return ret;
}

namespace {
struct shutdown_dtor {
    ServerContextImpl::shared_pointer wrapped;
    shutdown_dtor(const ServerContextImpl::shared_pointer& wrapped) :wrapped(wrapped) {}
    void operator()(ServerContext* self) {
        wrapped->shutdown();
        if(!wrapped.unique())
            LOG(logLevelWarn, "ServerContextImpl::shutdown() doesn't break all internal ref. loops. use_count=%u\n", (unsigned)wrapped.use_count());
        wrapped.reset();
    }
};
}

ServerContext::shared_pointer ServerContext::create(const Config &conf)
{
    ServerContextImpl::shared_pointer ret(new ServerContextImpl());
    ret->configuration = conf._conf;
    ret->_channelProviders = conf._providers;

    if (!ret->configuration)
    {
        ConfigurationProvider::shared_pointer configurationProvider = ConfigurationFactory::getProvider();
        ret->configuration = configurationProvider->getConfiguration("pvAccess-server");
        if (!ret->configuration)
        {
            ret->configuration = configurationProvider->getConfiguration("system");
        }
    }
    if(!ret->configuration) {
        ret->configuration = ConfigurationBuilder().push_env().build();
    }

    ret->loadConfiguration();
    ret->initialize();

    // wrap the returned shared_ptr so that it's dtor calls ->shutdown() to break internal referance loops
    {
        ServerContextImpl::shared_pointer wrapper(ret.get(), shutdown_dtor(ret));
        wrapper.swap(ret);
    }

    return ret;
}

}}
