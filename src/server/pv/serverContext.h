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

#include <shareLib.h>

namespace epics {
namespace pvAccess {

/**
 * The class representing a PVA Server context.
 */
class epicsShareClass ServerContext
{
public:
    typedef std::tr1::shared_ptr<ServerContext> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerContext> const_shared_pointer;

    /**
     * Destructor
     */
    virtual ~ServerContext() {};

    /**
     * Returns GUID (12-byte array).
     * @return GUID.
     */
    virtual const GUID& getGUID() = 0;

    /**
     * Get context implementation version.
     * @return version of the context implementation.
     */
    virtual const Version& getVersion() = 0;

    /**
     * Set <code>ChannelProviderRegistry</code> implementation and initialize server.
     * @param channelProviderRegistry channel providers registry to be used.
     */
    virtual void initialize(ChannelProviderRegistry::shared_pointer const & channelProviderRegistry) = 0;

    /**
     * Run server (process events).
     * @param	seconds	time in seconds the server will process events (method will block), if <code>0</code>
     * 				the method would block until <code>destroy()</code> is called.
     * @throws BaseException if server is already destroyed.
     */
    virtual void run(epics::pvData::int32 seconds) = 0;

    /**
     * Shutdown (stop executing run() method) of this context.
     * After shutdown Context cannot be rerun again, destroy() has to be called to clear all used resources.
     * @throws BaseException if the context has been destroyed.
     */
    virtual void shutdown() = 0;

    /**
     * Clear all resources attached to this context.
     * @throws BaseException if the context has been destroyed.
     */
    virtual void destroy() = 0;

    /**
     * Prints detailed information about the context to the standard output stream.
     */
    virtual void printInfo() = 0;

    /**
     * Prints detailed information about the context to the specified output stream.
     * @param str stream to which to print the info
     */
    virtual void printInfo(std::ostream& str) = 0;

    /**
     * Dispose (destroy) server context.
     * This calls <code>destroy()</code> and silently handles all exceptions.
     */
    virtual void dispose() = 0;

    virtual epicsTimeStamp& getStartTime() = 0;

    // ************************************************************************** //
    // **************************** [ Plugins ] ********************************* //
    // ************************************************************************** //

    /**
     * Set beacon server status provider.
     * @param beaconServerStatusProvider <code>BeaconServerStatusProvider</code> implementation to set.
     */
    virtual void setBeaconServerStatusProvider(BeaconServerStatusProvider::shared_pointer const & beaconServerStatusProvider) = 0;

};

epicsShareFunc ServerContext::shared_pointer startPVAServer(
    std::string const & providerNames = PVACCESS_ALL_PROVIDERS,
    int timeToRun = 0,
    bool runInSeparateThread = false,
    bool printInfo = false);

}
}


#endif /* SERVERCONTEXT_H_ */
