/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#include <pv/clientFactory.h>
#include <pv/logger.h>

#include <epicsSignal.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

Mutex ClientFactory::m_mutex;
ClientContextImpl::shared_pointer ClientFactory::m_context;

void ClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore ();
    epicsSignalInstallSigPipeIgnore ();

    Lock guard(m_mutex);
    
    if (m_context.get()) return;
    
    try {
        m_context = createClientContextImpl();
        m_context->initialize();
        ChannelProvider::shared_pointer provider = m_context->getProvider();
        registerChannelProvider(provider);
    } catch (std::exception &e) {
        LOG(logLevelError, "Unhandled exception caught at %s:%d: %s", __FILE__, __LINE__, e.what());
    } catch (...) {
        LOG(logLevelError, "Unhandled exception caught at %s:%d.", __FILE__, __LINE__);
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
