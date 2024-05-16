/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef SERVERCONTEXT_H_
#define SERVERCONTEXT_H_

#include <epicsTime.h>

#include <pv/pvaDefs.h>
#include <pv/beaconServerStatusProvider.h>
#include <pv/pvaConstants.h>
#include <pv/pvaVersion.h>
#include <pv/pvAccess.h>
#include <pv/configuration.h>

#include <shareLib.h>

namespace epics {
namespace pvAccess {

/**
 * The class representing a PVA Server context.
 */
class epicsShareClass ServerContext
{
public:
    POINTER_DEFINITIONS(ServerContext);

    /**
     * Destructor
     */
    virtual ~ServerContext() {};

    /**
     * Returns GUID (12-byte array).
     * @return GUID.
     */
    virtual const ServerGUID& getGUID() = 0;

    /**
     * Get context implementation version.
     * @return version of the context implementation.
     */
    virtual const Version& getVersion() = 0;

    /**
     * Run server (process events).
     * @param   seconds time in seconds the server will process events (method will block), if <code>0</code>
     *                          the method would block until <code>destroy()</code> is called.
     * @throws BaseException if server is already destroyed.
     */
    virtual void run(epics::pvData::uint32 seconds) = 0;

    virtual void shutdown() = 0;

    /**
     * Prints detailed information about the context to the standard output stream.
     */
    void printInfo(int lvl =0);

    /**
     * Prints detailed information about the context to the specified output stream.
     * @param lvl detail level
     * @param str stream to which to print the info
     */
    virtual void printInfo(std::ostream& str, int lvl=0) = 0;

    virtual epicsTimeStamp& getStartTime() = 0;

    /**
     * Get server port.
     * @return server port.
     */
    virtual epics::pvData::int32 getServerPort() = 0;

    /**
     * Get broadcast port.
     * @return broadcast port.
     */
    virtual epics::pvData::int32 getBroadcastPort() = 0;

    /** Return a Configuration with the actual values being used,
     *  including defaults used, and bounds limits applied.
     */
    virtual Configuration::shared_pointer getCurrentConfig() = 0;

    virtual const std::vector<ChannelProvider::shared_pointer>& getChannelProviders() =0;

    // ************************************************************************** //
    // **************************** [ Plugins ] ********************************* //
    // ************************************************************************** //

    /**
     * Set beacon server status provider.
     * @param beaconServerStatusProvider <code>BeaconServerStatusProvider</code> implementation to set.
     */
    virtual void setBeaconServerStatusProvider(BeaconServerStatusProvider::shared_pointer const & beaconServerStatusProvider) = 0;

    //! Options for a server insatnce
    class Config {
        friend class ServerContext;
        Configuration::const_shared_pointer _conf;
        std::vector<ChannelProvider::shared_pointer> _providers;
    public:
        Config() {}
        //! Use specific configuration.  Default is process environment
        Config& config(const Configuration::const_shared_pointer& c) { _conf = c; return *this; }
        //! Attach many providers.
        Config& providers(const std::vector<ChannelProvider::shared_pointer>& p) { _providers = p; return *this; }
        //! short hand for providers() with a length 1 vector.
        Config& provider(const ChannelProvider::shared_pointer& p) { _providers.push_back(p); return *this; }
    };

    /** Start a new PVA server
     *
     * By default the server will select ChannelProviders using the
     * EPICS_PVAS_PROVIDER_NAMES Configuration key.
     *
     * If a list of provided is given with Config::providers() then this
     * overrides any Configuration.
     *
     * If a specific Configuration is given with Config::config() then
     * this overrides the default Configuration.
     *
     * @returns shared_ptr<ServerContext> which will automatically shutdown() when the last reference is released.
     */
    static ServerContext::shared_pointer create(const Config& conf = Config());
};

// Caller must store the returned pointer to keep the server alive.
epicsShareFunc ServerContext::shared_pointer startPVAServer(
    std::string const & providerNames = PVACCESS_ALL_PROVIDERS,
    int timeToRun = 0,
    bool runInSeparateThread = false,
    bool printInfo = false);

}
}


#endif /* SERVERCONTEXT_H_ */
