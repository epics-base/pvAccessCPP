/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>

/* for CA */
#include <cadef.h>
#include <epicsSignal.h>

#define epicsExportSharedSymbols
#include <pv/logger.h>
#include <pv/caProvider.h>
#include <pv/caChannel.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvAccess::ca;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

std::string CAChannelProvider::PROVIDER_NAME = "ca";

CAChannelProvider::CAChannelProvider() : current_context(0)
{
    initialize();
}

CAChannelProvider::~CAChannelProvider()
{
}

std::string CAChannelProvider::getProviderName()
{
    return PROVIDER_NAME;
}

ChannelFind::shared_pointer CAChannelProvider::channelFind(
    std::string const & channelName,
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

ChannelFind::shared_pointer CAChannelProvider::channelList(
    ChannelListRequester::shared_pointer const & channelListRequester)
{
    if (!channelListRequester.get())
        throw std::runtime_error("null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    PVStringArray::const_svector none;
    EXCEPTION_GUARD(channelListRequester->channelListResult(errorStatus, nullChannelFind, none, false));
    return nullChannelFind;
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const & channelName,
    ChannelRequester::shared_pointer const & channelRequester,
    short priority)
{
    threadAttach();

    static std::string emptyString;
    return createChannel(channelName, channelRequester, priority, emptyString);
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const & channelName,
    ChannelRequester::shared_pointer const & channelRequester,
    short priority,
    std::string const & address)
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

void CAChannelProvider::threadAttach()
{
    ca_attach_context(current_context);
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

    current_context = ca_current_context();

    // TODO create a ca_poll thread, if ca_disable_preemptive_callback
}











// TODO global static variable (de/initialization order not guaranteed)
static Mutex mutex;
static CAChannelProvider::shared_pointer sharedProvider;

class CAChannelProviderFactoryImpl : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(CAChannelProviderFactoryImpl);

    virtual std::string getFactoryName()
    {
        return CAChannelProvider::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        Lock guard(mutex);
        if (!sharedProvider.get())
        {
            try {
                // TODO use std::make_shared
                std::tr1::shared_ptr<CAChannelProvider> tp(new CAChannelProvider());
                sharedProvider = tp;
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
            // TODO use std::make_shared
            std::tr1::shared_ptr<CAChannelProvider> tp(new CAChannelProvider());
            ChannelProvider::shared_pointer ni = tp;
            return ni;
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
        if(!sharedProvider) return;
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
