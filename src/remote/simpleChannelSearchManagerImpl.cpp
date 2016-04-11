/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdlib.h>
#include <time.h>
#include <vector>

#include <pv/timeStamp.h>

#define epicsExportSharedSymbols
#include <pv/simpleChannelSearchManagerImpl.h>
#include <pv/pvaConstants.h>
#include <pv/blockingUDP.h>
#include <pv/serializeHelper.h>

using namespace std;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

const int SimpleChannelSearchManagerImpl::DATA_COUNT_POSITION = PVA_MESSAGE_HEADER_SIZE + 4+1+3+16+2+1+4;
const int SimpleChannelSearchManagerImpl::CAST_POSITION = PVA_MESSAGE_HEADER_SIZE + 4;
const int SimpleChannelSearchManagerImpl::PAYLOAD_POSITION = 4;

// 225ms +/- 25ms random
const double SimpleChannelSearchManagerImpl::ATOMIC_PERIOD = 0.225;
const int SimpleChannelSearchManagerImpl::PERIOD_JITTER_MS = 25;

const int SimpleChannelSearchManagerImpl::DEFAULT_USER_VALUE = 1;
const int SimpleChannelSearchManagerImpl::BOOST_VALUE = 1;
// must be power of two (so that search is done)
const int SimpleChannelSearchManagerImpl::MAX_COUNT_VALUE = 1 << 8;
const int SimpleChannelSearchManagerImpl::MAX_FALLBACK_COUNT_VALUE = (1 << 7) + 1;

const int SimpleChannelSearchManagerImpl::MAX_FRAMES_AT_ONCE = 10;
const int SimpleChannelSearchManagerImpl::DELAY_BETWEEN_FRAMES_MS = 50;


SimpleChannelSearchManagerImpl::shared_pointer
SimpleChannelSearchManagerImpl::create(Context::shared_pointer const & context)
{
    SimpleChannelSearchManagerImpl::shared_pointer thisPtr(new SimpleChannelSearchManagerImpl(context));
    thisPtr->activate();
    return thisPtr;
}

SimpleChannelSearchManagerImpl::SimpleChannelSearchManagerImpl(Context::shared_pointer const & context) :
    m_context(context),
    m_responseAddress(*context->getSearchTransport()->getRemoteAddress()),
    m_canceled(),
    m_sequenceNumber(0),
    m_sendBuffer(MAX_UDP_UNFRAGMENTED_SEND),
    m_channels(),
    m_lastTimeSent(),
    m_mockTransportSendControl(),
    m_channelMutex(),
    m_userValueMutex(),
    m_mutex()
{

    // initialize send buffer
    initializeSendBuffer();


    // initialize random seed with some random value
    srand ( time(NULL) );
}

void SimpleChannelSearchManagerImpl::activate()
{
    // add some jitter so that all the clients do not send at the same time
    double period = ATOMIC_PERIOD + (rand() % (2*PERIOD_JITTER_MS+1) - PERIOD_JITTER_MS)/(double)1000;

    Context::shared_pointer context = m_context.lock();
    if (context.get())
        context->getTimer()->schedulePeriodic(shared_from_this(), period, period);

    //new Thread(this, "pvAccess immediate-search").start();
}

SimpleChannelSearchManagerImpl::~SimpleChannelSearchManagerImpl()
{
    // shared_from_this() is not allowed from destructor
    // be sure to call cancel() first
    // cancel();
}

void SimpleChannelSearchManagerImpl::cancel()
{
    Lock guard(m_mutex);

    if (m_canceled.get())
        return;
    m_canceled.set();

    Context::shared_pointer context = m_context.lock();
    if (context.get())
        context->getTimer()->cancel(shared_from_this());
}

int32_t SimpleChannelSearchManagerImpl::registeredCount()
{
    Lock guard(m_channelMutex);
    return static_cast<int32_t>(m_channels.size());
}

void SimpleChannelSearchManagerImpl::registerSearchInstance(SearchInstance::shared_pointer const & channel, bool penalize)
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

void SimpleChannelSearchManagerImpl::unregisterSearchInstance(SearchInstance::shared_pointer const & channel)
{
    Lock guard(m_channelMutex);
    pvAccessID id = channel->getSearchInstanceID();
    std::map<pvAccessID,SearchInstance::shared_pointer>::iterator channelsIter = m_channels.find(id);
    if(channelsIter != m_channels.end())
        m_channels.erase(id);
}

void SimpleChannelSearchManagerImpl::searchResponse(const GUID & guid, pvAccessID cid, int32_t /*seqNo*/, int8_t minorRevision, osiSockAddr* serverAddress)
{
    Lock guard(m_channelMutex);
    std::map<pvAccessID,SearchInstance::shared_pointer>::iterator channelsIter = m_channels.find(cid);
    if(channelsIter == m_channels.end())
    {
        guard.unlock();

        // enable duplicate reports
        SearchInstance::shared_pointer si = std::tr1::dynamic_pointer_cast<SearchInstance>(m_context.lock()->getChannel(cid));
        if (si)
            si->searchResponse(guid, minorRevision, serverAddress);
    }
    else
    {
        SearchInstance::shared_pointer si = channelsIter->second;

        // remove from search list
        m_channels.erase(cid);

        guard.unlock();

        // then notify SearchInstance
        si->searchResponse(guid, minorRevision, serverAddress);
    }
}

void SimpleChannelSearchManagerImpl::newServerDetected()
{
    boost();
    callback();
}

void SimpleChannelSearchManagerImpl::initializeSendBuffer()
{
    // for now OK, since it is only set here
    m_sequenceNumber++;


    // new buffer
    m_sendBuffer.clear();
    m_sendBuffer.putByte(PVA_MAGIC);
    m_sendBuffer.putByte(PVA_VERSION);
    m_sendBuffer.putByte((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG) ? 0x80 : 0x00); // data + 7-bit endianess
    m_sendBuffer.putByte(CMD_SEARCH);
    m_sendBuffer.putInt(4+1+3+16+2+1);		// "zero" payload
    m_sendBuffer.putInt(m_sequenceNumber);

    // multicast vs unicast mask
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
    // TODO "tcp" constant
    SerializeHelper::serializeString("tcp", &m_sendBuffer, &m_mockTransportSendControl);
    m_sendBuffer.putShort((int16_t)0);	// count
}

void SimpleChannelSearchManagerImpl::flushSendBuffer()
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


bool SimpleChannelSearchManagerImpl::generateSearchRequestMessage(SearchInstance::shared_pointer const & channel,
        ByteBuffer* requestMessage, TransportSendControl* control)
{
    epics::pvData::int16 dataCount = requestMessage->getShort(DATA_COUNT_POSITION);

    dataCount++;

    /*
    if(dataCount >= MAX_SEARCH_BATCH_COUNT)
        return false;
    */

    const std::string name = channel->getSearchInstanceName();
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

bool SimpleChannelSearchManagerImpl::generateSearchRequestMessage(SearchInstance::shared_pointer const & channel,
        bool allowNewFrame, bool flush)
{
    Lock guard(m_mutex);
    bool success = generateSearchRequestMessage(channel, &m_sendBuffer, &m_mockTransportSendControl);
    // buffer full, flush
    if(!success)
    {
        flushSendBuffer();
        if(allowNewFrame)
            generateSearchRequestMessage(channel, &m_sendBuffer, &m_mockTransportSendControl);
        if (flush)
            flushSendBuffer();
        return true;
    }

    if (flush)
        flushSendBuffer();

    return flush;
}

void SimpleChannelSearchManagerImpl::boost()
{
    Lock guard(m_channelMutex);
    Lock guard2(m_userValueMutex);
    std::map<pvAccessID,SearchInstance::shared_pointer>::iterator channelsIter = m_channels.begin();
    for(; channelsIter != m_channels.end(); channelsIter++)
    {
        int32_t& userValue = channelsIter->second->getUserValue();
        userValue = BOOST_VALUE;
    }
}

void SimpleChannelSearchManagerImpl::callback()
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
        std::map<pvAccessID,SearchInstance::shared_pointer>::iterator channelsIter = m_channels.begin();
        for(; channelsIter != m_channels.end(); channelsIter++)
            toSend.push_back(channelsIter->second);
    }

    vector<SearchInstance::shared_pointer>::iterator siter = toSend.begin();
    for (; siter != toSend.end(); siter++)
    {
        m_userValueMutex.lock();
        int32_t& countValue = (*siter)->getUserValue();
        bool skip = !isPowerOfTwo(countValue);

        if (countValue >= MAX_COUNT_VALUE)
            countValue = MAX_FALLBACK_COUNT_VALUE;
        else
            countValue++;
        m_userValueMutex.unlock();

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

bool SimpleChannelSearchManagerImpl::isPowerOfTwo(int32_t x)
{
    return ((x > 0) && (x & (x - 1)) == 0);
}

void SimpleChannelSearchManagerImpl::timerStopped()
{
}

}
}

