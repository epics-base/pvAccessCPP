/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>

#include <cadef.h>
#include <epicsSignal.h>
#include <epicsThread.h>
#include <epicsExit.h>
#include <pv/logger.h>
#include <pv/configuration.h>
#include <pv/pvAccess.h>

#include "caProviderPvt.h"
#include "caChannel.h"


namespace epics {
namespace pvAccess {
namespace ca {

using namespace epics::pvData;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

CAChannelProvider::CAChannelProvider() : current_context(0), destroyed(false)
{
    initialize();
}

CAChannelProvider::CAChannelProvider(const std::tr1::shared_ptr<Configuration>&)
    : current_context(0)
    , destroyed(false)
{
    // Ignoring Configuration as CA only allows config via. environment,
    // and we don't want to change this here.
    initialize();
}

CAChannelProvider::~CAChannelProvider()
{
    // call destroy() to destroy CA context
    destroy();
}

std::string CAChannelProvider::getProviderName()
{
    return "ca";
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
        if (destroyed)
            return;
        destroyed = true;

        while (!channels.empty())
        {
            Channel::shared_pointer channel = channels.begin()->second.lock();
            if (channel)
                channel->destroy();
            else
                channels.erase(channels.begin());
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

void CAChannelProvider::unregisterChannel(Channel* pchannel)
{
    Lock lock(channelsMutex);
    channels.erase(pchannel);
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


static
void ca_factory_cleanup(void*)
{
    try {
        ChannelProviderRegistry::clients()->remove("ca");
        ca_context_destroy(); 
    } catch(std::exception& e) {
        LOG(logLevelWarn, "Error when unregister \"ca\" factory");
    }
}

void CAClientFactory::start()
{
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();

    if(ChannelProviderRegistry::clients()->add<CAChannelProvider>("ca", false))
        epicsAtExit(&ca_factory_cleanup, NULL);
}

void CAClientFactory::stop()
{
    // unregister now done with exit hook
}

// perhaps useful during dynamic loading?
extern "C" {
void registerClientProvider_ca()
{
    try {
        CAClientFactory::start();
    } catch(std::exception& e){
        std::cerr<<"Error loading ca: "<<e.what()<<"\n";
    }
}
} // extern "C"

}
}
}

