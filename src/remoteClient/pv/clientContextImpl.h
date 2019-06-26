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

namespace epics {
namespace pvAccess {

class BeaconHandler;
class ClientContextImpl;

class ClientChannelImpl :
    public Channel,
    public TransportSender,
    public SearchInstance
{
public:
    POINTER_DEFINITIONS(ClientChannelImpl);

    virtual pvAccessID getChannelID() = 0;
    virtual void connectionCompleted(pvAccessID sid/*,  rights*/) = 0;
    virtual void createChannelFailed() = 0;
    virtual ClientContextImpl* getContext() = 0;
    virtual void channelDestroyedOnServer() = 0;

    virtual pvAccessID getID() =0;
    virtual pvAccessID getServerChannelID() = 0;
    virtual void registerResponseRequest(ResponseRequest::shared_pointer const & responseRequest) = 0;
    virtual void unregisterResponseRequest(pvAccessID ioid) = 0;
    virtual Transport::shared_pointer checkAndGetTransport() = 0;
    virtual Transport::shared_pointer checkDestroyedAndGetTransport() = 0;
    virtual Transport::shared_pointer getTransport() = 0;
    virtual void transportClosed() =0;

    static epics::pvData::Status channelDestroyed;
    static epics::pvData::Status channelDisconnected;

};

class ClientContextImpl : public Context
{
public:
    POINTER_DEFINITIONS(ClientContextImpl);

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
     * Prints detailed information about the context to the standard output stream.
     */
    virtual void printInfo() {printInfo(std::cout);}

    /**
     * Prints detailed information about the context to the specified output stream.
     * @param out the output stream.
     */
    virtual void printInfo(std::ostream& out) = 0;


    virtual ChannelSearchManager::shared_pointer getChannelSearchManager() = 0;
    virtual void checkChannelName(std::string const & name) = 0;

    virtual void registerChannel(ClientChannelImpl::shared_pointer const & channel) = 0;
    virtual void unregisterChannel(ClientChannelImpl::shared_pointer const & channel) = 0;

    virtual ClientChannelImpl::shared_pointer createChannelInternal(std::string const &name,
                                                              ChannelRequester::shared_pointer const & requester,
                                                              short priority,
                                                              const InetAddrVector& addresses) = 0;

    virtual ResponseRequest::shared_pointer getResponseRequest(pvAccessID ioid) = 0;
    virtual pvAccessID registerResponseRequest(ResponseRequest::shared_pointer const & request) = 0;
    virtual ResponseRequest::shared_pointer unregisterResponseRequest(pvAccessID ioid) = 0;


    virtual Transport::shared_pointer getTransport(ClientChannelImpl::shared_pointer const & client, osiSockAddr* serverAddress, epics::pvData::int8 minorRevision, epics::pvData::int16 priority) = 0;

    virtual void newServerDetected() = 0;

    virtual std::tr1::shared_ptr<BeaconHandler> getBeaconHandler(osiSockAddr* responseFrom) = 0;

    virtual void destroy() = 0;
};

ChannelProvider::shared_pointer createClientProvider(const Configuration::shared_pointer& conf);

}
}

#endif /* CLIENTCONTEXTIMPL_H_ */
