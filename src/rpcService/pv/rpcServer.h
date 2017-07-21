/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef RPCSERVER_H
#define RPCSERVER_H

#ifdef epicsExportSharedSymbols
#   define rpcServerEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/sharedPtr.h>

#ifdef rpcServerEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef rpcServerEpicsExportSharedSymbols
#endif

#include <pv/pvAccess.h>
#include <pv/rpcService.h>
#include <pv/serverContext.h>

#include <shareLib.h>

namespace epics {
namespace pvAccess {

class ServerContext;
class RPCChannelProvider;

//! Serves (only) RPCServiceAsync and RPCService instances.
class epicsShareClass RPCServer :
    public std::tr1::enable_shared_from_this<RPCServer>
{
private:

    std::tr1::shared_ptr<ServerContext> m_serverContext;
    std::tr1::shared_ptr<RPCChannelProvider> m_channelProviderImpl;

public:
    POINTER_DEFINITIONS(RPCServer);

    explicit RPCServer(const Configuration::const_shared_pointer& conf = Configuration::const_shared_pointer());

    virtual ~RPCServer();

    void registerService(std::string const & serviceName, RPCServiceAsync::shared_pointer const & service);

    void unregisterService(std::string const & serviceName);

    void run(int seconds = 0);

    /// Method requires usage of std::tr1::shared_ptr<RPCServer>. This instance must be
    /// owned by a shared_ptr instance.
    void runInNewThread(int seconds = 0);

    void destroy();

    /**
     * Display basic information about the context.
     */
    void printInfo();

    const std::tr1::shared_ptr<ServerContext>& getServer() const { return m_serverContext; }
};

epicsShareFunc Channel::shared_pointer createRPCChannel(ChannelProvider::shared_pointer const & provider,
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        RPCServiceAsync::shared_pointer const & rpcService);

}
}

#endif  /* RPCSERVER_H */
