/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CHANNEL_DISCOVERY_H
#define CHANNEL_DISCOVERY_H

#include <string>
#include <map>
#include <list>

#include <epicsTime.h>
#include <shareLib.h>

namespace epics { namespace pvAccess { namespace ChannelDiscovery {

/*
 * Name server channel entry
 */
struct epicsShareClass ChannelEntry {
    ChannelEntry();
    ChannelEntry(const std::string& channelName, const std::string& serverAddress, const epicsTimeStamp& updateTime);
    virtual ~ChannelEntry();
    std::string channelName;
    std::string serverAddress;
    epicsTimeStamp updateTime;
};
typedef std::map<std::string, ChannelEntry> ChannelMap;
typedef std::list<std::string> ServerAddressList;

}}}

#endif
