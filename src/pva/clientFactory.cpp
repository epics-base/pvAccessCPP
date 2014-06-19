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

// TODO global static variable (de/initialization order not guaranteed)
static Mutex mutex;
static ClientContextImpl::shared_pointer context;

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
        Lock guard(mutex);
        if (!context.get())
        {
            try {
                ClientContextImpl::shared_pointer lcontext = createClientContextImpl();
                lcontext->initialize();
                context = lcontext;
            } catch (std::exception &e) {
                LOG(logLevelError, "Unhandled exception caught at %s:%d: %s", __FILE__, __LINE__, e.what());
            } catch (...) {
                LOG(logLevelError, "Unhandled exception caught at %s:%d.", __FILE__, __LINE__);
            }
        }
        return context->getProvider();
    }

    virtual ChannelProvider::shared_pointer newInstance()
    {
        Lock guard(mutex);
        try {
            ClientContextImpl::shared_pointer lcontext = createClientContextImpl();
            lcontext->initialize();
            return lcontext->getProvider();
        } catch (std::exception &e) {
            LOG(logLevelError, "Unhandled exception caught at %s:%d: %s", __FILE__, __LINE__, e.what());
            return ChannelProvider::shared_pointer();
        } catch (...) {
            LOG(logLevelError, "Unhandled exception caught at %s:%d.", __FILE__, __LINE__);
            return ChannelProvider::shared_pointer();
        }
    }

    void destroySharedInstance()
    {
        Lock guard(mutex);
        if (context.get())
        {
            context->dispose();
            context.reset();
        }
    }
};

static ChannelProviderFactoryImpl::shared_pointer factory;

void ClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    Lock guard(mutex);
    if (!factory.get())
        factory.reset(new ChannelProviderFactoryImpl());

    registerChannelProviderFactory(factory);
}

void ClientFactory::stop()
{
    Lock guard(mutex);

    if (factory.get())
    {
        unregisterChannelProviderFactory(factory);
        factory->destroySharedInstance();
    }
}
