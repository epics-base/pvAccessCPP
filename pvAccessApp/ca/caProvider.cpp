/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/logger.h>
#include <pv/caProvider.h>
#include <pv/caChannel.h>

#include <algorithm>

/* for CA */
#include <cadef.h>
#include <epicsSignal.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvAccess::ca;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

String CAChannelProvider::PROVIDER_NAME = "ca";

CAChannelProvider::CAChannelProvider()
{
    initialize();
}

CAChannelProvider::~CAChannelProvider()
{
}

epics::pvData::String CAChannelProvider::getProviderName()
{
    return PROVIDER_NAME;
}

ChannelFind::shared_pointer CAChannelProvider::channelFind(
        epics::pvData::String const & channelName,
        ChannelFindRequester::shared_pointer const & channelFindRequester)
{
    if (channelName.empty())
        throw std::invalid_argument("empty channel name");

    if (!channelFindRequester)
        throw std::invalid_argument("null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    EXCEPTION_GUARD(channelFindRequester->channelFindResult(errorStatus, nullChannelFind, false));
    return nullChannelFind;
}

Channel::shared_pointer CAChannelProvider::createChannel(
        epics::pvData::String const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority)
{
    static String emptyString;
    return createChannel(channelName, channelRequester, priority, emptyString);
}

Channel::shared_pointer CAChannelProvider::createChannel(
        epics::pvData::String const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short priority,
        epics::pvData::String const & address)
{
    if (!address.empty())
        throw std::invalid_argument("CA does not support 'address' parameter");

    return CAChannel::create(shared_from_this(), channelName, priority, channelRequester);
}

void CAChannelProvider::configure(epics::pvData::PVStructure::shared_pointer /*configuration*/)
{
}

void CAChannelProvider::flush()
{
}

void CAChannelProvider::poll()
{
}

void CAChannelProvider::destroy()
{
    Lock lock(channelsMutex);
    {
        while (!channels.empty())
        {
            Channel::shared_pointer channel = channels.rbegin()->second.lock();
            if (channel)
                channel->destroy();
        }
    }

    /* Destroy CA Context */
    ca_context_destroy();
}

void CAChannelProvider::registerChannel(Channel::shared_pointer const & channel)
{
    Lock lock(channelsMutex);
    channels[channel.get()] = Channel::weak_pointer(channel);
}

void CAChannelProvider::unregisterChannel(Channel::shared_pointer const & channel)
{
    Lock lock(channelsMutex);
    channels.erase(channel.get());
}

void CAChannelProvider::initialize()
{
    /* Create Channel Access */
    int result = ca_context_create(ca_enable_preemptive_callback);
    if (result != ECA_NORMAL) {
        throw std::runtime_error(std::string("CA error %s occurred while trying "
                "to start channel access:") + ca_message(result));
    }

    // TODO create a ca_poll thread, if ca_disable_preemptive_callback
}











// TODO global static variable (de/initialization order not guaranteed)
static Mutex mutex;
static CAChannelProvider::shared_pointer sharedProvider;

class CAChannelProviderFactoryImpl : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(CAChannelProviderFactoryImpl);

    virtual epics::pvData::String getFactoryName()
    {
        return CAChannelProvider::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        Lock guard(mutex);
        if (!sharedProvider.get())
        {
            try {
                sharedProvider.reset(new CAChannelProvider());
            } catch (std::exception &e) {
                LOG(logLevelError, "Unhandled exception caught at %s:%d: %s", __FILE__, __LINE__, e.what());
            } catch (...) {
                LOG(logLevelError, "Unhandled exception caught at %s:%d.", __FILE__, __LINE__);
            }
        }
        return sharedProvider;
    }

    virtual ChannelProvider::shared_pointer newInstance()
    {
        try {
            return ChannelProvider::shared_pointer(new CAChannelProvider());
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
        sharedProvider->destroy();
        sharedProvider.reset();
    }
};

static CAChannelProviderFactoryImpl::shared_pointer factory;

void CAClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    Lock guard(mutex);
    if (!factory.get())
        factory.reset(new CAChannelProviderFactoryImpl());

    registerChannelProviderFactory(factory);
}

void CAClientFactory::stop()
{
    Lock guard(mutex);

    if (factory.get())
    {
        unregisterChannelProviderFactory(factory);
        factory->destroySharedInstance();
    }
}
