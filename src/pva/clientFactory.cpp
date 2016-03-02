/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
 
#include <pv/lock.h>
#include <pv/logger.h>

#include <epicsSignal.h>

#define epicsExportSharedSymbols
#include <pv/clientFactory.h>
#include <pv/clientContextImpl.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

static Mutex cfact_mutex;
static ChannelProvider::shared_pointer cfact_shared_provider;

class ChannelProviderFactoryImpl : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(ChannelProviderFactoryImpl);

    virtual std::string getFactoryName()
    {
        return ClientContextImpl::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        Lock guard(cfact_mutex);
        if (!cfact_shared_provider)
        {
            epics::pvAccess::Configuration::shared_pointer def;
            cfact_shared_provider = createClientProvider(def);
        }
        return cfact_shared_provider;
    }

    virtual ChannelProvider::shared_pointer newInstance(const std::tr1::shared_ptr<epics::pvAccess::Configuration>& conf)
    {
        Lock guard(cfact_mutex);
        return createClientProvider(conf);
    }
};

static ChannelProviderFactoryImpl::shared_pointer pva_factory;

void ClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    Lock guard(cfact_mutex);
    if (!pva_factory)
        pva_factory.reset(new ChannelProviderFactoryImpl());

    registerChannelProviderFactory(pva_factory);
}

void ClientFactory::stop()
{
    Lock guard(cfact_mutex);

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
