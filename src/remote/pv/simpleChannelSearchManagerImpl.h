/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef SIMPLECHANNELSEARCHMANAGERIMPL_H
#define SIMPLECHANNELSEARCHMANAGERIMPL_H

#ifdef epicsExportSharedSymbols
#   define simpleChannelSearchManagerEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/lock.h>
#include <pv/byteBuffer.h>
#include <pv/timer.h>

#ifdef simpleChannelSearchManagerEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef simpleChannelSearchManagerEpicsExportSharedSymbols
#endif

#include <pv/channelSearchManager.h>

namespace epics {
namespace pvAccess {


class MockTransportSendControl: public TransportSendControl
{
public:
    void endMessage() {}
    void flush(bool /*lastMessageCompleted*/) {}
    void setRecipient(const osiSockAddr& /*sendTo*/) {}
    void startMessage(epics::pvData::int8 /*command*/, std::size_t /*ensureCapacity*/, epics::pvData::int32 /*payloadSize*/) {}
    void ensureBuffer(std::size_t /*size*/) {}
    void alignBuffer(std::size_t /*alignment*/) {}
    void flushSerializeBuffer() {}
    void cachedSerialize(const std::tr1::shared_ptr<const epics::pvData::Field>& field, epics::pvData::ByteBuffer* buffer)
    {
        // no cache
        field->serialize(buffer, this);
    }
    virtual bool directSerialize(epics::pvData::ByteBuffer* /*existingBuffer*/, const char* /*toSerialize*/,
                                 std::size_t /*elementCount*/, std::size_t /*elementSize*/)
    {
        return false;
    }
};


class SimpleChannelSearchManagerImpl :
    public ChannelSearchManager,
    public epics::pvData::TimerCallback,
    public std::tr1::enable_shared_from_this<SimpleChannelSearchManagerImpl>
{
public:
    POINTER_DEFINITIONS(SimpleChannelSearchManagerImpl);

    /**
     * Constructor.
     * @param context
     */
    static shared_pointer create(Context::shared_pointer const & context);
    /**
     * Constructor.
     * @param context
     */
    virtual ~SimpleChannelSearchManagerImpl();
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
    void searchResponse(const GUID & guid, pvAccessID cid, int32_t seqNo, int8_t minorRevision, osiSockAddr* serverAddress);
    /**
     * New server detected.
     * Boost searching of all channels.
     */
    void newServerDetected();

    /// Timer callback.
    void callback();

    /// Timer stooped callback.
    void timerStopped();

private:

    /**
     * Private constructor.
     * @param context
     */
    SimpleChannelSearchManagerImpl(Context::shared_pointer const & context);
    void activate();

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
    std::map<pvAccessID,SearchInstance::shared_pointer> m_channels;

    /**
     * Time of last frame send.
     */
    int64_t m_lastTimeSent;

    /**
     * Mock transport send control
     */
    MockTransportSendControl m_mockTransportSendControl;

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

    static const int DATA_COUNT_POSITION;
    static const int CAST_POSITION;
    static const int PAYLOAD_POSITION;

    static const double ATOMIC_PERIOD;
    static const int PERIOD_JITTER_MS;

    static const int DEFAULT_USER_VALUE;
    static const int BOOST_VALUE;
    static const int MAX_COUNT_VALUE;
    static const int MAX_FALLBACK_COUNT_VALUE;

    static const int MAX_FRAMES_AT_ONCE;
    static const int DELAY_BETWEEN_FRAMES_MS;

};

}
}

#endif  /* SIMPLECHANNELSEARCHMANAGERIMPL_H */
