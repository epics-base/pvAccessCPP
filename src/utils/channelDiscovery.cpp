/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include "pv/channelDiscovery.h"

namespace epics { namespace pvAccess { namespace ChannelDiscovery {

ChannelEntry::ChannelEntry()
{
}

ChannelEntry::ChannelEntry(const std::string& channelName_, const std::string& serverAddress_, const epicsTimeStamp& updateTime_)
    : channelName(channelName_)
    , serverAddress(serverAddress_)
    , updateTime(updateTime_)
{
}

ChannelEntry::~ChannelEntry()
{
}

}}}
