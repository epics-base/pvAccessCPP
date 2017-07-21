/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef PIPELINESERVER_H
#define PIPELINESERVER_H

#ifdef epicsExportSharedSymbols
#   define pipelineServerEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/sharedPtr.h>

#ifdef pipelineServerEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef pipelineServerEpicsExportSharedSymbols
#endif

#include <pv/pvAccess.h>
#include <pv/pipelineService.h>
#include <pv/serverContext.h>

#include <shareLib.h>

namespace epics {
namespace pvAccess {

class PipelineChannelProvider;

class epicsShareClass PipelineServer :
    public std::tr1::enable_shared_from_this<PipelineServer>
{
private:

    ServerContext::shared_pointer m_serverContext;
    std::tr1::shared_ptr<PipelineChannelProvider> m_channelProviderImpl;

    // TODO no thread poll implementation

public:
    POINTER_DEFINITIONS(PipelineServer);

    PipelineServer();

    virtual ~PipelineServer();

    void registerService(std::string const & serviceName, PipelineService::shared_pointer const & service);

    void unregisterService(std::string const & serviceName);

    void run(int seconds = 0);

    /// Method requires usage of std::tr1::shared_ptr<PipelineServer>. This instance must be
    /// owned by a shared_ptr instance.
    void runInNewThread(int seconds = 0);

    void destroy();

    /**
     * Display basic information about the context.
     */
    void printInfo();

};

epicsShareFunc Channel::shared_pointer createPipelineChannel(ChannelProvider::shared_pointer const & provider,
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        PipelineService::shared_pointer const & pipelineService);

}
}

#endif  /* PIPELINESERVER_H */
