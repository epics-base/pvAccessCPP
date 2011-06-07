/*
 * clientContext.h
 *
 *  Created on: Dec 21, 2010
 *      Author: msekoran
 */

#ifndef CLIENTCONTEXTIMPL_H_
#define CLIENTCONTEXTIMPL_H_

#include <pv/pvAccess.h>
#include <pv/remote.h>
#include <pv/sharedPtr.h>
#include <pv/channelSearchManager.h>

class ChannelSearchManager;

namespace epics {
    namespace pvAccess {
        
        class BeaconHandler;
        class ClientContextImpl;
        
        class ChannelImpl :
                public Channel,
                public TransportClient,
                public TransportSender,
                public BaseSearchInstance
        {
        public:
            typedef std::tr1::shared_ptr<ChannelImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ChannelImpl> const_shared_pointer;
            typedef std::tr1::weak_ptr<ChannelImpl> weak_pointer;
            typedef std::tr1::weak_ptr<const ChannelImpl> const_weak_pointer;

            virtual pvAccessID getChannelID() = 0;
            virtual void destroyChannel(bool force) = 0;
            virtual void connectionCompleted(pvAccessID sid/*,  rights*/) = 0;
            virtual void createChannelFailed() = 0;
            virtual std::tr1::shared_ptr<ClientContextImpl> getContext() = 0;

            virtual pvAccessID getServerChannelID() = 0;
            virtual void registerResponseRequest(ResponseRequest::shared_pointer const & responseRequest) = 0;
            virtual void unregisterResponseRequest(pvAccessID ioid) = 0;
            virtual Transport::shared_pointer checkAndGetTransport() = 0;
            virtual Transport::shared_pointer getTransport() = 0;

            static Status channelDestroyed;
            static Status channelDisconnected;

        };
        
        class ClientContextImpl : public ClientContext, public Context
        {
        public:
            typedef std::tr1::shared_ptr<ClientContextImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ClientContextImpl> const_shared_pointer;
            typedef std::tr1::weak_ptr<ClientContextImpl> weak_pointer;
            typedef std::tr1::weak_ptr<const ClientContextImpl> const_weak_pointer;

            virtual ChannelSearchManager::shared_pointer getChannelSearchManager() = 0;
            virtual void checkChannelName(String& name) = 0;

            virtual void registerChannel(ChannelImpl::shared_pointer const & channel) = 0;
            virtual void unregisterChannel(ChannelImpl::shared_pointer const & channel) = 0;

            virtual void destroyChannel(ChannelImpl::shared_pointer const & channel, bool force) = 0;
            virtual ChannelImpl::shared_pointer createChannelInternal(String name, ChannelRequester::shared_pointer const & requester, short priority, std::auto_ptr<InetAddrVector>& addresses) = 0;

            virtual ResponseRequest::shared_pointer getResponseRequest(pvAccessID ioid) = 0;
            virtual pvAccessID registerResponseRequest(ResponseRequest::shared_pointer const & request) = 0;
            virtual ResponseRequest::shared_pointer unregisterResponseRequest(pvAccessID ioid) = 0;


            virtual Transport::shared_pointer getTransport(TransportClient::shared_pointer const & client, osiSockAddr* serverAddress, int16 minorRevision, int16 priority) = 0;

            virtual void beaconAnomalyNotify() = 0;

            virtual std::tr1::shared_ptr<BeaconHandler> getBeaconHandler(osiSockAddr* responseFrom) = 0;

        };
        
        extern ClientContextImpl::shared_pointer createClientContextImpl();

    }
}

#endif /* CLIENTCONTEXTIMPL_H_ */
