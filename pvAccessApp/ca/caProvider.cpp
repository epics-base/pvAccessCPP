/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/logger.h>
#include <pv/caProvider.h>
#include <pv/caChannel.h>

/* for CA */
#include <cadef.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

CAChannelProvider::CAChannelProvider()
{
    initialize();
}

CAChannelProvider::~CAChannelProvider()
{
}

epics::pvData::String CAChannelProvider::getProviderName()
{
    return "ca";
}

void CAChannelProvider::destroy()
{
    /* Shut down Channel Access */
    ca_context_destroy();
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

namespace epics { namespace pvAccess {

ChannelProvider::shared_pointer createCAChannelProvider()
{
    ChannelProvider::shared_pointer ptr(new CAChannelProvider());
    return ptr;
}

}}
