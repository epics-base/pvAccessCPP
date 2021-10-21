/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <cadef.h>
#include <epicsSignal.h>
#include <epicsMutex.h>
#include <epicsGuard.h>     // Needed for 3.15 builds
#include <pv/logger.h>
#include <pv/pvAccess.h>

#define epicsExportSharedSymbols
#include "pv/caProvider.h"
#include "caChannel.h"


namespace epics {
namespace pvAccess {
namespace ca {

using namespace epics::pvData;

CAChannelProvider::CAChannelProvider(const std::tr1::shared_ptr<Configuration> &)
    : ca_context(CAContextPtr(new CAContext()))
{
    connectNotifier.start();
    resultNotifier.start();
}

CAChannelProvider::~CAChannelProvider()
{
    epicsGuard<epicsMutex> G(channelListMutex);
    while (CAChannel *ch = caChannelList.get()) {
        // Here disconnectChannel() can't call our delChannel()
        // beacuse its CAChannelProviderPtr has by now expired.
        // That's why we removed it from caChannelList above.
        ch->disconnectChannel();
    }
}

std::string CAChannelProvider::getProviderName()
{
    return "ca";
}

ChannelFind::shared_pointer CAChannelProvider::channelFind(
    std::string const &channelName,
    ChannelFindRequester::shared_pointer const &channelFindRequester)
{
    if (channelName.empty())
        throw std::invalid_argument("CAChannelProvider::channelFind empty channel name");

    if (!channelFindRequester)
        throw std::invalid_argument("CAChannelProvider::channelFind null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "CAChannelProvider::channelFind not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    EXCEPTION_GUARD(channelFindRequester->channelFindResult(errorStatus, nullChannelFind, false));
    return nullChannelFind;
}

ChannelFind::shared_pointer CAChannelProvider::channelList(
    ChannelListRequester::shared_pointer const &channelListRequester)
{
    if (!channelListRequester.get())
        throw std::runtime_error("CAChannelProvider::channelList null requester");

    Status errorStatus(Status::STATUSTYPE_ERROR, "CAChannelProvider::channelList not implemented");
    ChannelFind::shared_pointer nullChannelFind;
    PVStringArray::const_svector none;
    EXCEPTION_GUARD(channelListRequester->channelListResult(errorStatus, nullChannelFind, none, false));
    return nullChannelFind;
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const &channelName,
    ChannelRequester::shared_pointer const &channelRequester,
    short priority)
{
    Channel::shared_pointer channel(
        createChannel(channelName, channelRequester, priority, std::string()));
    return channel;
}

Channel::shared_pointer CAChannelProvider::createChannel(
    std::string const &channelName,
    ChannelRequester::shared_pointer const &channelRequester,
    short priority,
    std::string const &address)
{
    if (!address.empty())
        throw std::invalid_argument("CAChannelProvider::createChannel does not support 'address' parameter");

    return CAChannel::create(shared_from_this(), channelName, priority, channelRequester);
}

void CAChannelProvider::addChannel(CAChannel &channel)
{
    epicsGuard<epicsMutex> G(channelListMutex);
    caChannelList.add(channel);
}

void CAChannelProvider::delChannel(CAChannel &channel)
{
    epicsGuard<epicsMutex> G(channelListMutex);
    caChannelList.remove(channel);
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

// ---------------- CAClientFactory ----------------

void CAClientFactory::start()
{
    if (ChannelProviderRegistry::clients()->getProvider("ca"))
        return;
    epicsSignalInstallSigAlarmIgnore();
    epicsSignalInstallSigPipeIgnore();
    if (!ChannelProviderRegistry::clients()->add<CAChannelProvider>("ca", true))
        throw std::runtime_error("CAClientFactory::start failed");
}

void CAClientFactory::stop()
{
}

}}}
