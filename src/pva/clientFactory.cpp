/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <string>

#include <epicsSignal.h>

#include <pv/lock.h>

#define epicsExportSharedSymbols
#include <pv/logger.h>
#include <pv/clientFactory.h>
#include <pv/clientContextImpl.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

class ChannelProviderFactoryImpl : public ChannelProviderFactory
{
private:
    Mutex m_mutex;
    ChannelProvider::shared_pointer m_shared_provider;

public:
    POINTER_DEFINITIONS(ChannelProviderFactoryImpl);

    virtual ~ChannelProviderFactoryImpl()
    {
        Lock guard(m_mutex);
        if (m_shared_provider)
        {
            ChannelProvider::shared_pointer provider;
            m_shared_provider.swap(provider);
            // factroy cleans up also shared provider
            provider->destroy();
        }
    }

    virtual std::string getFactoryName()
    {
        return ClientContextImpl::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        Lock guard(m_mutex);
        if (!m_shared_provider)
        {
            epics::pvAccess::Configuration::shared_pointer def;
            m_shared_provider = createClientProvider(def);
        }
        return m_shared_provider;
    }

    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<epics::pvAccess::Configuration>& conf)
    {
        Lock guard(m_mutex);
        return createClientProvider(conf);
    }
};

static Mutex cprovfact_mutex;
static ChannelProviderFactoryImpl::shared_pointer pva_factory;

void ClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    Lock guard(cprovfact_mutex);
    if (!pva_factory)
        pva_factory.reset(new ChannelProviderFactoryImpl());

    registerChannelProviderFactory(pva_factory);
}

void ClientFactory::stop()
{
    Lock guard(cprovfact_mutex);

    if (pva_factory)
    {
        unregisterChannelProviderFactory(pva_factory);
        if(!pva_factory.unique()) {
            LOG(logLevelWarn, "ClientFactory::stop() finds shared client context with %u remaining users",
                (unsigned)pva_factory.use_count());
        }
        pva_factory.reset();
    }
}
