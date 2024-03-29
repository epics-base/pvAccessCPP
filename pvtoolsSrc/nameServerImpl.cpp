/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#include <pv/hexDump.h>
#include <pv/stringUtility.h>

#include "nameServerImpl.h"
#include "pvutils.h"

using namespace std;
using namespace epics::pvData;
using epics::pvAccess::ChannelDiscovery::ChannelEntry;
using epics::pvAccess::ChannelDiscovery::ChannelMap;
using epics::pvAccess::ChannelDiscovery::ServerAddressList;


namespace epics { namespace pvAccess {


NameServerImpl::NameServerImpl(const epics::pvAccess::Configuration::shared_pointer& conf)
    : NameServer(conf)
{
}

NameServerImpl::~NameServerImpl()
{
}

void NameServerImpl::discoverServers(ServerAddressList& serverAddressList)
{
    if (!autoDiscovery) {
        LOG(logLevelDebug, "Skipping server discovery");
        return;
    }

    LOG(logLevelDebug, "Starting server discovery");
    std::string nsGuid = ::toHex(nameServerGuid.value, sizeof(nameServerGuid.value));
    ServerMap serverMap;
    ::discoverServers(timeout, serverMap);

    int nDiscoveredServers = 0;
    for (ServerMap::const_iterator it = serverMap.begin(); it != serverMap.end(); ++it) {
        const ServerEntry& entry = it->second;
        if (nsGuid == entry.guid) {
            LOG(logLevelDebug, "Ignoring our own server GUID 0x%s", entry.guid.c_str());
            continue;
        }
        size_t count = entry.addresses.size();
        std::string addresses = " ";
        for (size_t i = 0; i < count; i++) {
            addresses = addresses + inetAddressToString(entry.addresses[i]) + " ";
        }
        LOG(logLevelDebug, "Found server GUID 0x%s version %d: %s@[%s]", entry.guid.c_str(), (int)entry.version, entry.protocol.c_str(), addresses.c_str());
        if (count > 0) {
            std::string serverAddress = inetAddressToString(entry.addresses[0]);
            if (addUniqueServerToList(serverAddress, serverAddressList)) {
                nDiscoveredServers++;
            }
        }
    }
    LOG(logLevelDebug, "Discovered %d servers", nDiscoveredServers);
}

void NameServerImpl::discoverServerChannels(const std::string& serverAddress, ChannelMap& channelMap)
{
    LOG(logLevelDebug, "Discovering channels for server %s", serverAddress.c_str());
    try {
        PVStructure::shared_pointer ret = getChannelInfo(serverAddress, "channels", timeout);
        PVStringArray::shared_pointer pvs(ret->getSubField<PVStringArray>("value"));
        PVStringArray::const_svector val(pvs->view());
        epicsTimeStamp now;
        epicsTimeGetCurrent(&now);
        for (unsigned int i = 0; i < val.size(); i++) {
            ChannelEntry channelEntry;
            channelEntry.channelName = val[i];
            channelEntry.serverAddress = serverAddress;
            channelEntry.updateTime = now;
            channelMap[val[i]] = channelEntry;
        }
        LOG(logLevelDebug, "Discovered %d channels for server %s", int(val.size()), serverAddress.c_str());
    }
    catch(std::exception& e) {
        LOG(logLevelError, "Error retrieving channels for server %s: %s", serverAddress.c_str(), e.what());
    }
}

}}
