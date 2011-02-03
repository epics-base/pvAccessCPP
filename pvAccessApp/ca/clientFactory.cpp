/* clientFactory.cpp */
/* Author:  Matej Sekoranja Date: 2011.2.1 */

#include <clientFactory.h>
#include <errlog.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

Mutex ClientFactory::m_mutex;
ClientContextImpl* ClientFactory::m_context = 0;

void ClientFactory::start()
{
    Lock guard(&m_mutex);
    
    if (m_context) return;
    
    try {
        m_context = createClientContextImpl();
        m_context->initialize();
        registerChannelProvider(m_context->getProvider());
    } catch (std::exception &e) {
        errlogSevPrintf(errlogMajor, "Unhandled exception caught at %s:%d: %s", __FILE__, __LINE__, e.what());
    } catch (...) {
        errlogSevPrintf(errlogMajor, "Unhandled exception caught at %s:%d.", __FILE__, __LINE__);
    }
}

void ClientFactory::stop()
{
    Lock guard(&m_mutex);
    
    unregisterChannelProvider(m_context->getProvider());
    m_context->dispose(); 
    m_context = 0;
}
