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

namespace epics {
namespace pvAccess {

using namespace epics::pvData;

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

static Mutex startStopMutex;

ChannelProviderRegistryPtr ClientFactory::channelRegistry = ChannelProviderRegistryPtr();
ChannelProviderFactoryImplPtr ClientFactory::channelProvider = ChannelProviderFactoryImplPtr();
int ClientFactory::numStart = 0;

void ClientFactory::start()
{
   Lock guard(startStopMutex);
std::cout << "ClientFactory::start() numStart " << numStart << std::endl; 
    ++numStart;
    if(numStart>1) return;
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();
    channelProvider.reset(new ChannelProviderFactoryImpl());
    channelRegistry = ChannelProviderRegistry::getChannelProviderRegistry();
std::cout << "channelRegistry::use_count " << channelRegistry.use_count() << std::endl;
    channelRegistry->registerChannelProviderFactory(channelProvider);
}

void ClientFactory::stop()
{
std::cout << "ClientFactory::stop() numStart " << numStart << std::endl; 
std::cout << "channelRegistry::use_count " << channelRegistry.use_count() << std::endl;
    Lock guard(startStopMutex);
    if(numStart==0) return;
    --numStart;
    if(numStart>=1) return;

    if (channelProvider)
    {
        channelRegistry->unregisterChannelProviderFactory(channelProvider);
        if(!channelProvider.unique()) {
            LOG(logLevelWarn, "ClientFactory::stop() finds shared client context with %u remaining users",
                (unsigned)channelProvider.use_count());
        }
        channelProvider.reset();
        channelRegistry.reset();
    }
}

}}
