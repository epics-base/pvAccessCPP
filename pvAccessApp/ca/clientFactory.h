/* clientFactory.h */
#ifndef CLIENTFACTORY_H
#define CLIENTFACTORY_H

#include <clientContextImpl.h>
#include <lock.h>

namespace epics { namespace pvAccess { 

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
