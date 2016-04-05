/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef CHANNELSEARCHMANAGER_H
#define CHANNELSEARCHMANAGER_H

#ifdef epicsExportSharedSymbols
#   define channelSearchManagerEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <osiSock.h>

#ifdef channelSearchManagerEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef channelSearchManagerEpicsExportSharedSymbols
#endif

#include <pv/remote.h>

namespace epics {
namespace pvAccess {

class SearchInstance {
public:
    POINTER_DEFINITIONS(SearchInstance);

    /**
     * Destructor
     */
    virtual ~SearchInstance() {};

    virtual pvAccessID getSearchInstanceID() = 0;

    virtual std::string getSearchInstanceName() = 0;

    virtual int32_t& getUserValue() = 0;

    /**
     * Search response from server (channel found).
     * @param guid server GUID.
     * @param minorRevision	server minor PVA revision.
     * @param serverAddress	server address.
     */
    // TODO make serverAddress an URI or similar
    virtual void searchResponse(const GUID & guid, int8_t minorRevision, osiSockAddr* serverAddress) = 0;
};

class ChannelSearchManager {
public:
    POINTER_DEFINITIONS(ChannelSearchManager);

    /**
     * Destructor
     */
    virtual ~ChannelSearchManager() {};

    /**
     * Get number of registered channels.
     * @return number of registered channels.
     */
    virtual int32_t registeredCount() = 0;

    /**
     * Register channel.
     * @param channel
     */
    virtual void registerSearchInstance(SearchInstance::shared_pointer const & channel, bool penalize = false) = 0;


    /**
     * Unregister channel.
     * @param channel
     */
    virtual void unregisterSearchInstance(SearchInstance::shared_pointer const & channel) = 0;

    /**
     * Search response from server (channel found).
     * @param guid  server GUID.
     * @param cid	client channel ID.
     * @param seqNo	search sequence number.
     * @param minorRevision	server minor PVA revision.
     * @param serverAddress	server address.
     */
    virtual void searchResponse(const GUID & guid, pvAccessID cid, int32_t seqNo, int8_t minorRevision, osiSockAddr* serverAddress) = 0;

    /**
     * New server detected.
     * Boost searching of all channels.
     */
    virtual void newServerDetected() = 0;

    /**
     * Cancel.
     */
    virtual void cancel() = 0;

};

}
}

#endif
