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

#include <pv/pvaDefs.h>
#include <pv/remote.h>

namespace epics {
namespace pvAccess {

class SearchInstance {
public:
    POINTER_DEFINITIONS(SearchInstance);

    /**
     * Destructor
     */
    virtual ~SearchInstance() {}

    virtual pvAccessID getSearchInstanceID() = 0;

    virtual const std::string& getSearchInstanceName() = 0;

    virtual int32_t& getUserValue() = 0;

    /**
     * Search response from server (channel found).
     * @param guid server GUID.
     * @param minorRevision	server minor PVA revision.
     * @param serverAddress	server address.
     */
    // TODO make serverAddress an URI or similar
    virtual void searchResponse(const ServerGUID & guid, int8_t minorRevision, osiSockAddr* serverAddress) = 0;
};


class ChannelSearchManager :
        public epics::pvData::TimerCallback,
        public std::tr1::enable_shared_from_this<ChannelSearchManager>
{
public:
    POINTER_DEFINITIONS(ChannelSearchManager);

    virtual ~ChannelSearchManager();
    /**
     * Cancel.
     */
    void cancel();
    /**
     * Get number of registered channels.
     * @return number of registered channels.
     */
    int32_t registeredCount();
    /**
     * Register channel.
     * @param channel to register.
     */
    void registerSearchInstance(SearchInstance::shared_pointer const & channel, bool penalize = false);
    /**
     * Unregister channel.
     * @param channel to unregister.
     */
    void unregisterSearchInstance(SearchInstance::shared_pointer const & channel);
    /**
     * Search response from server (channel found).
     * @param guid  server GUID.
     * @param cid	client channel ID.
     * @param seqNo	search sequence number.
     * @param minorRevision	server minor PVA revision.
     * @param serverAddress	server address.
     */
    void searchResponse(const ServerGUID & guid, pvAccessID cid, int32_t seqNo, int8_t minorRevision, osiSockAddr* serverAddress);
    /**
     * New server detected.
     * Boost searching of all channels.
     */
    void newServerDetected();

    /// Timer callback.
    virtual void callback() OVERRIDE FINAL;

    /// Timer stooped callback.
    virtual void timerStopped() OVERRIDE FINAL;

    /**
     * Private constructor.
     * @param context
     */
    ChannelSearchManager(Context::shared_pointer const & context);
    void activate();

private:

    bool generateSearchRequestMessage(SearchInstance::shared_pointer const & channel, bool allowNewFrame, bool flush);

    static bool generateSearchRequestMessage(SearchInstance::shared_pointer const & channel,
            epics::pvData::ByteBuffer* byteBuffer, TransportSendControl* control);

    void boost();

    void initializeSendBuffer();
    void flushSendBuffer();

    static bool isPowerOfTwo(int32_t x);

    /**
     * Context.
     */
    Context::weak_pointer m_context;

    /**
     * Response address.
     */
    osiSockAddr m_responseAddress;

    /**
     * Canceled flag.
     */
    AtomicBoolean m_canceled;

    /**
     * Search (datagram) sequence number.
     */
    int32_t m_sequenceNumber;

    /**
     * Send byte buffer (frame)
     */
    epics::pvData::ByteBuffer m_sendBuffer;

    /**
     * Set of registered channels.
     */
    typedef std::map<pvAccessID,SearchInstance::weak_pointer> m_channels_t;
    m_channels_t m_channels;

    /**
     * Time of last frame send.
     */
    int64_t m_lastTimeSent;

    /**
     * This instance mutex.
     */
    epics::pvData::Mutex m_channelMutex;

    /**
     * User value lock.
     */
    epics::pvData::Mutex m_userValueMutex;

    /**
     * m_channels mutex.
     */
    epics::pvData::Mutex m_mutex;
};

}
}

#endif
