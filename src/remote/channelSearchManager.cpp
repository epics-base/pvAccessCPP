/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdlib.h>
#include <time.h>
#include <vector>

#include <epicsMutex.h>

#include <pv/serializationHelper.h>
#include <pv/timeStamp.h>

#define epicsExportSharedSymbols
#include <pv/channelSearchManager.h>
#include <pv/pvaConstants.h>
#include <pv/blockingUDP.h>
#include <pv/serializeHelper.h>
#include <pv/logger.h>

using namespace std;
using namespace epics::pvData;

namespace {
namespace pva = epics::pvAccess;

class MockTransportSendControl: public pva::TransportSendControl
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

}// namespace

namespace epics {
namespace pvAccess {

// these are byte offset in a CMD_SEARCH request message
// used to mangle a buffer to support incremental construction.  (ick!!!)
static const int DATA_COUNT_POSITION = PVA_MESSAGE_HEADER_SIZE + 4+1+3+16+2+1+4;
static const int CAST_POSITION = PVA_MESSAGE_HEADER_SIZE + 4;
static const int PAYLOAD_POSITION = 4;

// 225ms +/- 25ms random
static const double ATOMIC_PERIOD = 0.225;
static const double PERIOD_JITTER_MS = 0.025;

static const int DEFAULT_USER_VALUE = 1;
static const int BOOST_VALUE = 1;
// must be power of two (so that search is done)
static const int MAX_COUNT_VALUE = 1 << 8;
static const int MAX_FALLBACK_COUNT_VALUE = (1 << 7) + 1;

static const int MAX_FRAMES_AT_ONCE = 10;
static const int DELAY_BETWEEN_FRAMES_MS = 50;


ChannelSearchManager::ChannelSearchManager(Context::shared_pointer const & context) :
    m_context(context),
    m_responseAddress(), // initialized in activate()
    m_canceled(),
    m_sequenceNumber(0),
    m_sendBuffer(MAX_UDP_UNFRAGMENTED_SEND),
    m_channels(),
    m_lastTimeSent(),
    m_channelMutex(),
    m_userValueMutex(),
    m_mutex()
{
    // initialize random seed with some random value
    srand ( time(NULL) );
}

void ChannelSearchManager::activate()
{
    m_responseAddress = Context::shared_pointer(m_context)->getSearchTransport()->getRemoteAddress();

    // initialize send buffer
    initializeSendBuffer();

    // add some jitter so that all the clients do not send at the same time
    double period = ATOMIC_PERIOD + double(rand())/RAND_MAX*PERIOD_JITTER_MS;

    Context::shared_pointer context(m_context.lock());
    if (context)
        context->getTimer()->schedulePeriodic(shared_from_this(), period, period);
}

ChannelSearchManager::~ChannelSearchManager()
{
    Lock guard(m_mutex);
    if (!m_canceled.get()) {
        LOG(logLevelWarn, "Logic error: ChannelSearchManager destroyed w/o cancel()");
    }
}

void ChannelSearchManager::cancel()
{
    Lock guard(m_mutex);

    if (m_canceled.get())
        return;
    m_canceled.set();

    Context::shared_pointer context(m_context.lock());
    if (context)
        context->getTimer()->cancel(shared_from_this());
}

int32_t ChannelSearchManager::registeredCount()
{
    Lock guard(m_channelMutex);
    return static_cast<int32_t>(m_channels.size());
}

void ChannelSearchManager::registerSearchInstance(SearchInstance::shared_pointer const & channel, bool penalize)
{
    if (m_canceled.get())
        return;

    bool immediateTrigger;
    {
        Lock guard(m_channelMutex);

        // overrides if already registered
        m_channels[channel->getSearchInstanceID()] = channel;
        immediateTrigger = (m_channels.size() == 1);

        Lock guard2(m_userValueMutex);
        int32_t& userValue = channel->getUserValue();
        userValue = (penalize ? MAX_FALLBACK_COUNT_VALUE : DEFAULT_USER_VALUE);
    }

    if (immediateTrigger)
        callback();
}

void ChannelSearchManager::unregisterSearchInstance(SearchInstance::shared_pointer const & channel)
{
    Lock guard(m_channelMutex);
    pvAccessID id = channel->getSearchInstanceID();
    m_channels.erase(id);
}

void ChannelSearchManager::searchResponse(const ServerGUID & guid, pvAccessID cid, int32_t /*seqNo*/, int8_t minorRevision, osiSockAddr* serverAddress)
{
    Lock guard(m_channelMutex);
    m_channels_t::iterator channelsIter = m_channels.find(cid);
    if(channelsIter == m_channels.end())
    {
        guard.unlock();
        Context::shared_pointer ctxt(m_context.lock());
        // TODO: proper action if !ctxt???
        if(!ctxt) return;

        // enable duplicate reports
        SearchInstance::shared_pointer si = std::tr1::dynamic_pointer_cast<SearchInstance>(ctxt->getChannel(cid));
        if (si)
            si->searchResponse(guid, minorRevision, serverAddress);
    }
    else
    {
        SearchInstance::shared_pointer si(channelsIter->second.lock());

        // remove from search list
        m_channels.erase(cid);

        guard.unlock();

        // then notify SearchInstance
        if(si)
            si->searchResponse(guid, minorRevision, serverAddress);
    }
}

void ChannelSearchManager::newServerDetected()
{
    boost();
    callback();
}

void ChannelSearchManager::initializeSendBuffer()
{
    // for now OK, since it is only set here
    m_sequenceNumber++;


    // new buffer
    m_sendBuffer.clear();
    m_sendBuffer.putByte(PVA_MAGIC);
    m_sendBuffer.putByte(PVA_CLIENT_PROTOCOL_REVISION);
    m_sendBuffer.putByte((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG) ? 0x80 : 0x00); // data + 7-bit endianess
    m_sendBuffer.putByte(CMD_SEARCH);
    m_sendBuffer.putInt(4+1+3+16+2+1);		// "zero" payload
    m_sendBuffer.putInt(m_sequenceNumber);

    // multicast vs unicast mask
    // This is CAST_POSITION, which is overwritten before send
    m_sendBuffer.putByte((int8_t)0);

    // reserved part
    m_sendBuffer.putByte((int8_t)0);
    m_sendBuffer.putShort((int16_t)0);

    // NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
    encodeAsIPv6Address(&m_sendBuffer, &m_responseAddress);
    m_sendBuffer.putShort((int16_t)ntohs(m_responseAddress.ia.sin_port));

    // TODO now only TCP is supported
    // note: this affects DATA_COUNT_POSITION
    m_sendBuffer.putByte((int8_t)1);

    MockTransportSendControl control;
    SerializeHelper::serializeString("tcp", &m_sendBuffer, &control);
    m_sendBuffer.putShort((int16_t)0);	// count
}

void ChannelSearchManager::flushSendBuffer()
{
    Lock guard(m_mutex);

    Transport::shared_pointer tt = m_context.lock()->getSearchTransport();
    BlockingUDPTransport::shared_pointer ut = std::tr1::static_pointer_cast<BlockingUDPTransport>(tt);

    m_sendBuffer.putByte(CAST_POSITION, (int8_t)0x80);  // unicast, no reply required
    ut->send(&m_sendBuffer, inetAddressType_unicast);

    m_sendBuffer.putByte(CAST_POSITION, (int8_t)0x00);  // b/m-cast, no reply required
    ut->send(&m_sendBuffer, inetAddressType_broadcast_multicast);

    initializeSendBuffer();
}


bool ChannelSearchManager::generateSearchRequestMessage(SearchInstance::shared_pointer const & channel,
        ByteBuffer* requestMessage, TransportSendControl* control)
{
    epics::pvData::int16 dataCount = requestMessage->getShort(DATA_COUNT_POSITION);

    dataCount++;

    /*
    if(dataCount >= MAX_SEARCH_BATCH_COUNT)
        return false;
    */

    const std::string& name(channel->getSearchInstanceName());
    // not nice...
    const int addedPayloadSize = sizeof(int32)/sizeof(int8) + (1 + sizeof(int32)/sizeof(int8) + name.length());
    if(((int)requestMessage->getRemaining()) < addedPayloadSize)
        return false;

    requestMessage->putInt(channel->getSearchInstanceID());
    SerializeHelper::serializeString(name, requestMessage, control);

    requestMessage->putInt(PAYLOAD_POSITION, requestMessage->getPosition() - PVA_MESSAGE_HEADER_SIZE);
    requestMessage->putShort(DATA_COUNT_POSITION, dataCount);
    return true;
}

bool ChannelSearchManager::generateSearchRequestMessage(SearchInstance::shared_pointer const & channel,
        bool allowNewFrame, bool flush)
{
    MockTransportSendControl control;

    Lock guard(m_mutex);
    bool success = generateSearchRequestMessage(channel, &m_sendBuffer, &control);
    // buffer full, flush
    if(!success)
    {
        flushSendBuffer();
        if(allowNewFrame)
            generateSearchRequestMessage(channel, &m_sendBuffer, &control);
        if (flush)
            flushSendBuffer();
        return true;
    }

    if (flush)
        flushSendBuffer();

    return flush;
}

void ChannelSearchManager::boost()
{
    Lock guard(m_channelMutex);
    Lock guard2(m_userValueMutex);
    m_channels_t::iterator channelsIter = m_channels.begin();
    for(; channelsIter != m_channels.end(); channelsIter++)
    {
        SearchInstance::shared_pointer inst(channelsIter->second.lock());
        if(!inst) continue;
        int32_t& userValue = inst->getUserValue();
        userValue = BOOST_VALUE;
    }
}

void ChannelSearchManager::callback()
{
    // high-frequency beacon anomaly trigger guard
    {
        Lock guard(m_mutex);

        epics::pvData::TimeStamp now;
        now.getCurrent();
        int64_t nowMS = now.getMilliseconds();

        if (nowMS - m_lastTimeSent < 100)
            return;
        m_lastTimeSent = nowMS;
    }


    int count = 0;
    int frameSent = 0;

    vector<SearchInstance::shared_pointer> toSend;
    {
        Lock guard(m_channelMutex);
        toSend.reserve(m_channels.size());

        for(m_channels_t::iterator channelsIter = m_channels.begin();
            channelsIter != m_channels.end(); channelsIter++)
        {
            SearchInstance::shared_pointer inst(channelsIter->second.lock());
            if(!inst) continue;
            toSend.push_back(inst);
        }
    }

    vector<SearchInstance::shared_pointer>::iterator siter = toSend.begin();
    for (; siter != toSend.end(); siter++)
    {
        bool skip;
        {
            epicsGuard<epicsMutex> G(m_userValueMutex);
            int32_t& countValue = (*siter)->getUserValue();
            skip = !isPowerOfTwo(countValue);

            if (countValue >= MAX_COUNT_VALUE)
                countValue = MAX_FALLBACK_COUNT_VALUE;
            else
                countValue++;
        }

        // back-off
        if (skip)
            continue;

        count++;

        if (generateSearchRequestMessage(*siter, true, false))
            frameSent++;
        if (frameSent == MAX_FRAMES_AT_ONCE)
        {
            epicsThreadSleep(DELAY_BETWEEN_FRAMES_MS/(double)1000.0);
            frameSent = 0;
        }
    }

    if (count > 0)
        flushSendBuffer();
}

bool ChannelSearchManager::isPowerOfTwo(int32_t x)
{
    return ((x > 0) && (x & (x - 1)) == 0);
}

void ChannelSearchManager::timerStopped()
{
}

}
}

