/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#ifndef RPCSERVER_H
#define RPCSERVER_H

#include <pv/sharedPtr.h>
#include <pv/pvAccess.h>

#include <pv/rpcService.h>
#include <pv/serverContext.h>

namespace epics { namespace pvAccess { 

class RPCServer {
    private:

    ServerContextImpl::shared_pointer m_serverContext;
    ChannelProviderFactory::shared_pointer m_channelProviderFactory;
    ChannelProvider::shared_pointer m_channelProviderImpl;

    // TODO no thread poll implementation
    
    public:
    POINTER_DEFINITIONS(RPCServer);
    
    RPCServer();

    virtual ~RPCServer();
    
    void registerService(epics::pvData::String const & serviceName, RPCService::shared_pointer const & service);
    
    void unregisterService(epics::pvData::String const & serviceName);

    void run(int seconds = 0);
    
    void destroy();    
    
    /**
     * Display basic information about the context.
     */
    void printInfo();

};


}}

#endif  /* RPCSERVER_H */
