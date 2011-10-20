/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#ifndef CLIENTFACTORY_H
#define CLIENTFACTORY_H

#include <pv/clientContextImpl.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess { 

class ClientFactory {
    public:
    static void start();
    static void stop();
    
    private:
    static epics::pvData::Mutex m_mutex;
    static ClientContextImpl::shared_pointer m_context;
};

}}

#endif  /* CLIENTFACTORY_H */
