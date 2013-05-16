/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 

#include <pv/clientFactory.h>
#include <pv/clientContextImpl.h>
#include <pv/lock.h>
#include <pv/logger.h>

#include <epicsSignal.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

// TODO global static variable (de/initialization order not guaranteed)
static Mutex m_mutex;
static ClientContextImpl::shared_pointer m_context;

void ClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    Lock guard(m_mutex);
    if (m_context.get()) return;
    
    try {
        m_context = createClientContextImpl();
        m_context->initialize();
        registerChannelProvider(m_context->getProvider());
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

    unregisterChannelProvider(m_context->getProvider());
    
    m_context->dispose(); 
    m_context.reset();
}
