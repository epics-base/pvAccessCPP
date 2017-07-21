#ifndef SERVERCONTEXTIMPL_H
#define SERVERCONTEXTIMPL_H

#ifdef epicsExportSharedSymbols
#   define serverContextImplEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/thread.h>

#ifdef serverContextImplEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef serverContextImplEpicsExportSharedSymbols
#endif

#include <pv/blockingUDP.h>
#include <pv/blockingTCP.h>
#include <pv/beaconEmitter.h>

#include "serverContext.h"

namespace epics {
namespace pvAccess {

class ServerContextImpl :
    public ServerContext,
    public Context,
    public std::tr1::enable_shared_from_this<ServerContextImpl>
{
    friend class ServerContext;
public:
    typedef std::tr1::shared_ptr<ServerContextImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerContextImpl> const_shared_pointer;

    ServerContextImpl();
    virtual ~ServerContextImpl();

    //**************** derived from ServerContext ****************//
    const ServerGUID& getGUID() OVERRIDE FINAL;
    const Version& getVersion() OVERRIDE FINAL;
    void initialize();
    void run(epics::pvData::uint32 seconds) OVERRIDE FINAL;
    void shutdown() OVERRIDE FINAL;
    void printInfo(std::ostream& str) OVERRIDE FINAL;
    void setBeaconServerStatusProvider(BeaconServerStatusProvider::shared_pointer const & beaconServerStatusProvider) OVERRIDE FINAL;
    //**************** derived from Context ****************//
    epics::pvData::Timer::shared_pointer getTimer() OVERRIDE FINAL;
    Channel::shared_pointer getChannel(pvAccessID id) OVERRIDE FINAL;
    Transport::shared_pointer getSearchTransport() OVERRIDE FINAL;
    Configuration::const_shared_pointer getConfiguration() OVERRIDE FINAL;
    TransportRegistry* getTransportRegistry() OVERRIDE FINAL;
    std::map<std::string, std::tr1::shared_ptr<SecurityPlugin> >& getSecurityPlugins() OVERRIDE FINAL;

    virtual void newServerDetected() OVERRIDE FINAL;


    epicsTimeStamp& getStartTime() OVERRIDE FINAL;

    virtual Configuration::shared_pointer getCurrentConfig() OVERRIDE FINAL;

    /**
     * Version.
     */
    static const Version VERSION;

    /**
     * Get beacon address list.
     * @return beacon address list.
     */
    std::string getBeaconAddressList();

    /**
     * Get beacon address list auto flag.
     * @return beacon address list auto flag.
     */
    bool isAutoBeaconAddressList();

    /**
     * Get beacon period (in seconds).
     * @return beacon period (in seconds).
     */
    float getBeaconPeriod();

    /**
     * Get receiver buffer (payload) size.
     * @return max payload size.
     */
    epics::pvData::int32 getReceiveBufferSize();

    /**
     * Get server port.
     * @return server port.
     */
    epics::pvData::int32 getServerPort() OVERRIDE FINAL;

    /**
     * Get broadcast port.
     * @return broadcast port.
     */
    epics::pvData::int32 getBroadcastPort() OVERRIDE FINAL;

    /**
     * Get ignore search address list.
     * @return ignore search address list.
     */
    std::string getIgnoreAddressList();

    /**
     * Get registered beacon server status provider.
     * @return registered beacon server status provider.
     */
    BeaconServerStatusProvider::shared_pointer getBeaconServerStatusProvider();

    /**
     * Get server newtwork (IP) address.
     * @return server network (IP) address, <code>NULL</code> if not bounded.
     */
    const osiSockAddr *getServerInetAddress();

    /**
     * Broadcast (UDP send) transport.
     * @return broadcast transport.
     */
    BlockingUDPTransport::shared_pointer getBroadcastTransport();

    /**
     * Get channel providers.
     * @return channel providers.
     */
    virtual std::vector<ChannelProvider::shared_pointer>& getChannelProviders() OVERRIDE FINAL;

    /**
     * Return <code>true</code> if channel provider name is provided by configuration (e.g. system env. var.).
     * @return <code>true</code> if channel provider name is provided by configuration (e.g. system env. var.)
     */
    bool isChannelProviderNamePreconfigured();

private:

    /**
     * Server GUID.
     */
    ServerGUID _guid;

    /**
     * A space-separated list of broadcast address which to send beacons.
     * Each address must be of the form: ip.number:port or host.name:port
     */
    std::string _beaconAddressList;

    /**
     * List of used NIF.
     */
    IfaceNodeVector _ifaceList;

    osiSockAddr _ifaceAddr;

    /**
     * A space-separated list of address from which to ignore name resolution requests.
     * Each address must be of the form: ip.number:port or host.name:port
     */
    std::string _ignoreAddressList;

    /**
     * Define whether or not the network interfaces should be discovered at runtime.
     */
    bool _autoBeaconAddressList;

    /**
     * Period in second between two beacon signals.
     */
    float _beaconPeriod;

    /**
     * Broadcast port number to listen to.
     */
    epics::pvData::int32 _broadcastPort;

    /**
     * Port number for the server to listen to.
     */
    epics::pvData::int32 _serverPort;

    /**
     * Length in bytes of the maximum buffer (payload) size that may pass through PVA.
     */
    epics::pvData::int32 _receiveBufferSize;

    /**
     * Timer.
     */
    epics::pvData::Timer::shared_pointer _timer;

    /**
     * UDP transports needed to receive channel searches.
     */
    BlockingUDPTransportVector _udpTransports;

    /**
     * UDP socket used to sending.
     */
    BlockingUDPTransport::shared_pointer _broadcastTransport;

    /**
     * Beacon emitter.
     */
    BeaconEmitter::shared_pointer _beaconEmitter;

    /**
     * PVAS acceptor (accepts PVA virtual circuit).
     */
    BlockingTCPAcceptor::shared_pointer _acceptor;

    /**
     * PVA transport (virtual circuit) registry.
     * This registry contains all active transports - connections to PVA servers.
     */
    TransportRegistry _transportRegistry;

    /**
     * Response handler.
     */
    ResponseHandler::shared_pointer _responseHandler;

    /**
     * Channel provider.
     */
    std::vector<ChannelProvider::shared_pointer> _channelProviders;

    /**
     * Run mutex.
     */
    epics::pvData::Mutex _mutex;

    /**
     * Run event.
     */
    epics::pvData::Event _runEvent;

    /**
     * Beacon server status provider interface (optional).
     */
    BeaconServerStatusProvider::shared_pointer _beaconServerStatusProvider;

    /**
     * Generate ServerGUID.
     */
    void generateGUID();

    /**
     * Initialize logger.
     */
    void initializeLogger();

    /**
     * Load configuration.
     */
    void loadConfiguration();

    /**
     * Destroy all transports.
     */
    void destroyAllTransports();

    Configuration::const_shared_pointer configuration;

    epicsTimeStamp _startTime;
};

}
}

#endif // SERVERCONTEXTIMPL_H
