/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CLIENTCONTEXTIMPL_H_
#define CLIENTCONTEXTIMPL_H_

#ifdef epicsExportSharedSymbols
#   define clientContextImplEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/sharedPtr.h>

#ifdef clientContextImplEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef clientContextImplEpicsExportSharedSymbols
#endif

#include <pv/pvAccess.h>
#include <pv/remote.h>
#include <pv/channelSearchManager.h>
#include <pv/inetAddressUtil.h>

#include <shareLib.h>

class ChannelSearchManager;

namespace epics {
namespace pvAccess {

class BeaconHandler;
class ClientContextImpl;

class ChannelImpl :
    public Channel,
    public TransportClient,
    public TransportSender,
    public SearchInstance
{
public:
    POINTER_DEFINITIONS(ChannelImpl);

    virtual pvAccessID getChannelID() = 0;
    virtual void destroyChannel(bool force) = 0;
    virtual void connectionCompleted(pvAccessID sid/*,  rights*/) = 0;
    virtual void createChannelFailed() = 0;
    virtual std::tr1::shared_ptr<ClientContextImpl> getContext() = 0;
    virtual void channelDestroyedOnServer() = 0;

    virtual pvAccessID getServerChannelID() = 0;
    virtual void registerResponseRequest(ResponseRequest::shared_pointer const & responseRequest) = 0;
    virtual void unregisterResponseRequest(pvAccessID ioid) = 0;
    virtual Transport::shared_pointer checkAndGetTransport() = 0;
    virtual Transport::shared_pointer checkDestroyedAndGetTransport() = 0;
    virtual Transport::shared_pointer getTransport() = 0;

    static epics::pvData::Status channelDestroyed;
    static epics::pvData::Status channelDisconnected;

};

class ClientContextImpl : public Context
{
public:
    POINTER_DEFINITIONS(ClientContextImpl);

    static std::string PROVIDER_NAME;

    /**
     * Get context implementation version.
     * @return version of the context implementation.
     */
    virtual const Version& getVersion() = 0;

    /**
     * Initialize client context. This method is called immediately after instance construction (call of constructor).
     */
    virtual void initialize() = 0; // public?

    /**
     * Get channel provider implementation.
     * @return the channel provider.
     */
    //virtual ChannelProvider::shared_pointer const & getProvider() = 0;

    /**
     * Prints detailed information about the context to the standard output stream.
     */
    virtual void printInfo() = 0;

    /**
     * Prints detailed information about the context to the specified output stream.
     * @param out the output stream.
     */
    virtual void printInfo(std::ostream& out) = 0;

    /**
     * Dispose (destroy) server context.
     * This calls <code>destroy()</code> and silently handles all exceptions.
     */
    virtual void dispose() = 0;


    virtual ChannelSearchManager::shared_pointer getChannelSearchManager() = 0;
    virtual void checkChannelName(std::string const & name) = 0;

    virtual void registerChannel(ChannelImpl::shared_pointer const & channel) = 0;
    virtual void unregisterChannel(ChannelImpl::shared_pointer const & channel) = 0;

    virtual void destroyChannel(ChannelImpl::shared_pointer const & channel, bool force) = 0;
    virtual ChannelImpl::shared_pointer createChannelInternal(std::string const &name, ChannelRequester::shared_pointer const & requester, short priority, std::auto_ptr<InetAddrVector>& addresses) = 0;

    virtual ResponseRequest::shared_pointer getResponseRequest(pvAccessID ioid) = 0;
    virtual pvAccessID registerResponseRequest(ResponseRequest::shared_pointer const & request) = 0;
    virtual ResponseRequest::shared_pointer unregisterResponseRequest(pvAccessID ioid) = 0;


    virtual Transport::shared_pointer getTransport(TransportClient::shared_pointer const & client, osiSockAddr* serverAddress, epics::pvData::int8 minorRevision, epics::pvData::int16 priority) = 0;

    virtual void newServerDetected() = 0;

    virtual std::tr1::shared_ptr<BeaconHandler> getBeaconHandler(std::string const & protocol, osiSockAddr* responseFrom) = 0;

    virtual void configure(epics::pvData::PVStructure::shared_pointer configuration) = 0;
    virtual void flush() = 0;
    virtual void poll() = 0;

    virtual void destroy() = 0;
};

epicsShareFunc ChannelProvider::shared_pointer createClientProvider(const Configuration::shared_pointer& conf);

}
}

#endif /* CLIENTCONTEXTIMPL_H_ */
