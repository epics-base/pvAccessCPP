/* clientFactory.cpp */
/* Author:  Matej Sekoranja Date: 2011.2.1 */

#include <pv/clientFactory.h>
#include <errlog.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

Mutex ClientFactory::m_mutex;
ClientContextImpl::shared_pointer ClientFactory::m_context;

void ClientFactory::start()
{
    Lock guard(m_mutex);
    
    if (m_context.get()) return;
    
    try {
        m_context = createClientContextImpl();
        m_context->initialize();
        ChannelProvider::shared_pointer provider = m_context->getProvider();
        registerChannelProvider(provider);
    } catch (std::exception &e) {
        errlogSevPrintf(errlogMajor, "Unhandled exception caught at %s:%d: %s", __FILE__, __LINE__, e.what());
    } catch (...) {
        errlogSevPrintf(errlogMajor, "Unhandled exception caught at %s:%d.", __FILE__, __LINE__);
    }
}

void ClientFactory::stop()
{
    Lock guard(m_mutex);
    
    if (!m_context.get()) return;

    ChannelProvider::shared_pointer provider = m_context->getProvider();
    unregisterChannelProvider(provider);
    
    m_context->dispose(); 
    m_context.reset();
}
