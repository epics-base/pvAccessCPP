/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/

#include <map>
#include <string>
#include <vector>
#include <limits>
#include <stdexcept>
#include <sstream>
#include <sys/types.h>

#include <osiSock.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsVersion.h>
#include <errlog.h>
#include <epicsAtomic.h>

#include <pv/byteBuffer.h>
#include <pv/pvType.h>
#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/event.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/inetAddressUtil.h>
#include <pv/hexDump.h>
#include <pv/logger.h>
#include <pv/likely.h>
#include <pv/codec.h>
#include <pv/serializationHelper.h>
#include <pv/serverChannelImpl.h>
#include <pv/clientContextImpl.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;

typedef epicsGuard<epicsMutex> Guard;

namespace {
struct BreakTransport : TransportSender
{
    virtual ~BreakTransport() {}
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL
    {
        throw epics::pvAccess::detail::connection_closed_exception("Break");
    }
};
} // namespace

namespace epics {
namespace pvAccess {

size_t Transport::num_instances;

Transport::Transport()
    :_totalBytesSent(0u)
    ,_totalBytesRecv(0u)
{
    REFTRACE_INCREMENT(num_instances);
}

Transport::~Transport()
{
    REFTRACE_DECREMENT(num_instances);
}

namespace detail {

const std::size_t AbstractCodec::MAX_MESSAGE_PROCESS = 100;
const std::size_t AbstractCodec::MAX_MESSAGE_SEND = 100;
const std::size_t AbstractCodec::MAX_ENSURE_SIZE = 1024;
const std::size_t AbstractCodec::MAX_ENSURE_DATA_SIZE = MAX_ENSURE_SIZE/2;
const std::size_t AbstractCodec::MAX_ENSURE_BUFFER_SIZE = MAX_ENSURE_SIZE;
const std::size_t AbstractCodec::MAX_ENSURE_DATA_BUFFER_SIZE = 1024;

static
size_t bufSizeSelect(size_t request)
{
    return std::max(request, size_t(MAX_TCP_RECV + AbstractCodec::MAX_ENSURE_DATA_BUFFER_SIZE));
}

AbstractCodec::AbstractCodec(
    bool serverFlag,
    size_t sendBufferSize,
    size_t receiveBufferSize,
    int32_t socketSendBufferSize,
    bool blockingProcessQueue):
    //PROTECTED
    _readMode(NORMAL), _version(0), _flags(0), _command(0), _payloadSize(0),
    _remoteTransportSocketReceiveBufferSize(MAX_TCP_RECV),
    _senderThread(0),
    _writeMode(PROCESS_SEND_QUEUE),
    _writeOpReady(false),
    _socketBuffer(bufSizeSelect(receiveBufferSize)),
    _sendBuffer(bufSizeSelect(sendBufferSize)),
    //PRIVATE
    _storedPayloadSize(0), _storedPosition(0), _startPosition(0),
    _maxSendPayloadSize(_sendBuffer.getSize() - 2*PVA_MESSAGE_HEADER_SIZE),    // start msg + control
    _lastMessageStartPosition(std::numeric_limits<size_t>::max()),_lastSegmentedMessageType(0),
    _lastSegmentedMessageCommand(0), _nextMessagePayloadOffset(0),
    _byteOrderFlag(EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00),
    _clientServerFlag(serverFlag ? 0x40 : 0x00)
{
    if (_socketBuffer.getSize() < 2*MAX_ENSURE_SIZE)
        throw std::invalid_argument(
            "receiveBuffer.capacity() < 2*MAX_ENSURE_SIZE");

    if (_sendBuffer.getSize() < 2*MAX_ENSURE_SIZE)
        throw std::invalid_argument("sendBuffer() < 2*MAX_ENSURE_SIZE");

    // initialize to be empty
    _socketBuffer.setPosition(_socketBuffer.getLimit());
    _startPosition = _socketBuffer.getPosition();

    // clear send
    _sendBuffer.clear();
}


// thows io_exception, connection_closed_exception, invalid_stream_exception
void AbstractCodec::processRead() {
    switch (_readMode)
    {
    case NORMAL:
        processReadNormal();
        break;
    case SEGMENTED:
        processReadSegmented();
        break;
    case SPLIT:
        throw std::logic_error("ReadMode == SPLIT not supported");
    }

}


void AbstractCodec::processHeader() {

    Guard G(_mutex); // guards access to _version et al.

    // magic code
    int8_t magicCode = _socketBuffer.getByte();

    // version
    int8_t ver = _socketBuffer.getByte();
    if(_version!=ver) {
        // enable timeout if both ends support
        _version = ver;
        setRxTimeout(getRevision()>1);
    }

    // flags
    _flags = _socketBuffer.getByte();

    // command
    _command = _socketBuffer.getByte();

    // read payload size
    _payloadSize = _socketBuffer.getInt();

    // check magic code
    if (magicCode != PVA_MAGIC || _version==0)
    {
        LOG(logLevelError,
            "Invalid header received from the client : %s %02x%02x%02x%02x disconnecting...",
            inetAddressToString(*getLastReadBufferSocketAddress()).c_str(),
            unsigned(magicCode), unsigned(_version), unsigned(_flags), unsigned(_command));
        invalidDataStreamHandler();
        throw invalid_data_stream_exception("invalid header received");
    }

}


void AbstractCodec::processReadNormal()  {

    try
    {
        std::size_t messageProcessCount = 0;
        while (messageProcessCount++ < MAX_MESSAGE_PROCESS)
        {
            // read as much as available, but at least for a header
            // readFromSocket checks if reading from socket is really necessary
            if (!readToBuffer(PVA_MESSAGE_HEADER_SIZE, false)) {
                return;
            }

            // read header fields
            processHeader();
            bool isControl = ((_flags & 0x01) == 0x01);
            if (isControl) {
                processControlMessage();
            }
            else
            {
                // segmented sanity check
                bool notFirstSegment = (_flags & 0x20) != 0;
                if (notFirstSegment)
                {
                    // not-first segmented message with zero payload is "kind of" valid
                    // TODO this should check if previous message was first- or middle-segmented message
                    if (_payloadSize == 0)
                        continue;

                    LOG(logLevelError,
                        "Protocol Violation: Not-a-first segmented message received in normal mode"
                        " from the client at %s:%d: %s, disconnecting...",
                        __FILE__, __LINE__, inetAddressToString(*getLastReadBufferSocketAddress()).c_str());
                    invalidDataStreamHandler();
                    throw invalid_data_stream_exception(
                        "not-a-first segmented message received in normal mode");
                }

                _storedPayloadSize = _payloadSize;
                _storedPosition = _socketBuffer.getPosition();
                _storedLimit = _socketBuffer.getLimit();
                _socketBuffer.setLimit(std::min(_storedPosition + _storedPayloadSize, _storedLimit));
                bool postProcess = true;
                try
                {
                    // handle response
                    processApplicationMessage();

                    if (!isOpen())
                        return;

                    postProcess = false;
                    postProcessApplicationMessage();
                }
                catch(...)
                {
                    if (!isOpen())
                        return;

                    if (postProcess)
                    {
                        postProcessApplicationMessage();
                    }

                    throw;
                }
            }
        }

    }
    catch (invalid_data_stream_exception & )
    {
        // noop, should be already handled (and logged)
    }
    catch (connection_closed_exception & )
    {
        // noop, should be already handled (and logged)
    }
}

void AbstractCodec::postProcessApplicationMessage()
{
    // can be closed by now
    // isOpen() should be efficiently implemented
    while (true)
        //while (isOpen())
    {
        // set position as whole message was read
        //(in case code haven't done so)
        std::size_t newPosition = _storedPosition + _storedPayloadSize;

        // aligned buffer size ensures that there is enough space
        //in buffer,
        // however data might not be fully read

        // discard the rest of the packet
        if (newPosition > _storedLimit)
        {
            // processApplicationMessage() did not read up
            //quite some buffer

            // we only handle unused alignment bytes
            int bytesNotRead =
                newPosition - _socketBuffer.getPosition();
            assert(bytesNotRead>=0);

            if (bytesNotRead==0)
            {
                // reveal currently existing padding
                _socketBuffer.setLimit(_storedLimit);
                continue;
            }

            // TODO we do not handle this for now (maybe never)
            LOG(logLevelWarn,
                "unprocessed read buffer from client at %s:%d: %s,"
                " disconnecting...",
                __FILE__, __LINE__, inetAddressToString(*getLastReadBufferSocketAddress()).c_str());
            invalidDataStreamHandler();
            throw invalid_data_stream_exception(
                "unprocessed read buffer");
        }
        _socketBuffer.setLimit(_storedLimit);
        _socketBuffer.setPosition(newPosition);
        break;
    }
}

void AbstractCodec::processReadSegmented() {

    while (true)
    {
        // read as much as available, but at least for a header
        // readFromSocket checks if reading from socket is really necessary
        readToBuffer(PVA_MESSAGE_HEADER_SIZE, true);

        // read header fields
        processHeader();

        bool isControl = ((_flags & 0x01) == 0x01);
        if (isControl)
            processControlMessage();
        else
        {
            // last segment bit set (means in-between segment or last segment)
            // we expect this, no non-control messages between
            //segmented message are supported
            // NOTE: for now... it is easy to support non-semgented
            //messages between segmented messages
            bool notFirstSegment = (_flags & 0x20) != 0;
            if (!notFirstSegment)
            {
                LOG(logLevelWarn,
                    "Protocol Violation: Not-a-first segmented message expected from the client at"
                    " %s:%d: %s, disconnecting...",
                    __FILE__, __LINE__, inetAddressToString(*getLastReadBufferSocketAddress()).c_str());
                invalidDataStreamHandler();
                throw invalid_data_stream_exception(
                    "not-a-first segmented message expected");
            }

            _storedPayloadSize = _payloadSize;

            // return control to caller code
            return;
        }
    }

}


bool AbstractCodec::readToBuffer(
    std::size_t requiredBytes,
    bool persistent)  {

    // do we already have requiredBytes available?
    std::size_t remainingBytes = _socketBuffer.getRemaining();
    if (remainingBytes >= requiredBytes) {
        return true;
    }

    // assumption: remainingBytes < MAX_ENSURE_DATA_BUFFER_SIZE &&
    //			   requiredBytes < (socketBuffer.capacity() - 1)

    //
    // copy unread part to the beginning of the buffer
    // to make room for new data (as much as we can read)
    // NOTE: requiredBytes is expected to be small (order of 10 bytes)
    //

    // a new start position, we are careful to preserve alignment
    _startPosition = MAX_ENSURE_SIZE;

    std::size_t endPosition = _startPosition + remainingBytes;

    for (std::size_t i = _startPosition; i < endPosition; i++)
        _socketBuffer.putByte(i, _socketBuffer.getByte());

    // update buffer to the new position
    _socketBuffer.setLimit(_socketBuffer.getSize());
    _socketBuffer.setPosition(endPosition);

    // read at least requiredBytes bytes
    std::size_t requiredPosition = _startPosition + requiredBytes;
    while (_socketBuffer.getPosition() < requiredPosition)
    {
        int bytesRead = read(&_socketBuffer);

        if (bytesRead < 0)
        {
            close();
            throw connection_closed_exception("bytesRead < 0");
        }
        // non-blocking IO support
        else if (bytesRead == 0)
        {
            if (persistent)
                readPollOne();
            else
            {
                // set pointers (aka flip)
                _socketBuffer.setLimit(_socketBuffer.getPosition());
                _socketBuffer.setPosition(_startPosition);

                return false;
            }
        }

        atomic::add(_totalBytesRecv, bytesRead);
    }

    // set pointers (aka flip)
    _socketBuffer.setLimit(_socketBuffer.getPosition());
    _socketBuffer.setPosition(_startPosition);

    return true;
}


void AbstractCodec::ensureData(std::size_t size) {

    // enough of data?
    if (_socketBuffer.getRemaining() >= size)
        return;

    // to large for buffer...
    if (size > MAX_ENSURE_DATA_SIZE)	{// half for SPLIT, half for SEGMENTED
        std::ostringstream msg;
        msg << "requested for buffer size " << size
            << ", but maximum " << MAX_ENSURE_DATA_SIZE << " is allowed.";
        LOG(logLevelWarn,
            "%s at %s:%d.,", msg.str().c_str(), __FILE__, __LINE__);
        throw std::invalid_argument(msg.str());
    }

    try
    {

        // subtract what was already processed
        std::size_t pos = _socketBuffer.getPosition();
        _storedPayloadSize -= pos - _storedPosition;

        // SPLIT message case
        // no more data and we have some payload left => read buffer
        // NOTE: (storedPayloadSize >= size) does not work if size
        //spans over multiple messages
        if (_storedPayloadSize >= (_storedLimit-pos))
        {
            // just read up remaining payload
            // this will move current (<size) part of the buffer
            // to the beginning of the buffer
            ReadMode storedMode = _readMode;
            _readMode = SPLIT;
            readToBuffer(size, true);
            _readMode = storedMode;
            _storedPosition = _socketBuffer.getPosition();
            _storedLimit = _socketBuffer.getLimit();
            _socketBuffer.setLimit(
                std::min<std::size_t>(
                    _storedPosition + _storedPayloadSize, _storedLimit));

            // check needed, if not enough data is available or
            // we run into segmented message
            ensureData(size);
        }
        // SEGMENTED message case
        else
        {
            // TODO check flags
            //if (flags && SEGMENTED_FLAGS_MASK == 0)
            //	throw IllegalStateException("segmented message expected,
            //but current message flag does not indicate it");


            // copy remaining bytes of payload to safe area
            //[0 to MAX_ENSURE_DATA_BUFFER_SIZE/2), if any
            // remaining is relative to payload since buffer is
            //bounded from outside
            std::size_t remainingBytes = _socketBuffer.getRemaining();
            for (std::size_t i = 0; i < remainingBytes; i++)
                _socketBuffer.putByte(i, _socketBuffer.getByte());

            // restore limit (there might be some data already present
            //and readToBuffer needs to know real limit)
            _socketBuffer.setLimit(_storedLimit);

            // we expect segmented message, we expect header
            // that (and maybe some control packets) needs to be "removed"
            // so that we get combined payload
            ReadMode storedMode = _readMode;
            _readMode = SEGMENTED;
            processRead();
            _readMode = storedMode;

            // make sure we have all the data (maybe we run into SPLIT)
            readToBuffer(size - remainingBytes, true);

            // SPLIT cannot mess with this, since start of the message,
            //i.e. current position, is always aligned
            _socketBuffer.setPosition(
                _socketBuffer.getPosition());

            // copy before position (i.e. start of the payload)
            for (int32_t i = remainingBytes - 1,
                    j = _socketBuffer.getPosition() - 1; i >= 0; i--, j--)
                _socketBuffer.putByte(j, _socketBuffer.getByte(i));

            _startPosition = _socketBuffer.getPosition() - remainingBytes;
            _socketBuffer.setPosition(_startPosition);

            _storedPayloadSize += remainingBytes;
            _storedPosition = _startPosition;
            _storedLimit = _socketBuffer.getLimit();
            _socketBuffer.setLimit(
                std::min<std::size_t>(
                    _storedPosition + _storedPayloadSize, _storedLimit));

            // sequential small segmented messages in the buffer
            ensureData(size);
        }
    }
    catch (io_exception &) {
        try {
            close();
        } catch (io_exception & ) {
            // noop, best-effort close
        }
        throw connection_closed_exception(
            "Failed to ensure data to read buffer.");
    }
}


std::size_t AbstractCodec::alignedValue(
    std::size_t value,
    std::size_t alignment) {

    std::size_t k = (alignment - 1);
    return (value + k) & (~k);
}


void AbstractCodec::alignData(std::size_t alignment) {

    std::size_t k = (alignment - 1);
    std::size_t pos = _socketBuffer.getPosition();
    std::size_t newpos = (pos + k) & (~k);
    if (pos == newpos)
        return;

    std::size_t diff = _socketBuffer.getLimit() - newpos;
    if (diff > 0)
    {
        _socketBuffer.setPosition(newpos);
        return;
    }

    ensureData(diff);

    // position has changed, recalculate
    newpos = (_socketBuffer.getPosition() + k) & (~k);
    _socketBuffer.setPosition(newpos);
}

static const char PADDING_BYTES[] =
{
    static_cast<char>(0xFF),
    static_cast<char>(0xFF),
    static_cast<char>(0xFF),
    static_cast<char>(0xFF),
    static_cast<char>(0xFF),
    static_cast<char>(0xFF),
    static_cast<char>(0xFF),
    static_cast<char>(0xFF)
};

void AbstractCodec::alignBuffer(std::size_t alignment) {

    std::size_t k = (alignment - 1);
    std::size_t pos = _sendBuffer.getPosition();
    std::size_t newpos = (pos + k) & (~k);
    if (pos == newpos)
        return;

    // for safety reasons we really pad (override previous message data)
    std::size_t padCount = newpos - pos;
    _sendBuffer.put(PADDING_BYTES, 0, padCount);
}


void AbstractCodec::startMessage(
    epics::pvData::int8 command,
    std::size_t ensureCapacity,
    epics::pvData::int32 payloadSize) {
    _lastMessageStartPosition =
        std::numeric_limits<size_t>::max();		// TODO revise this
    ensureBuffer(
        PVA_MESSAGE_HEADER_SIZE + ensureCapacity + _nextMessagePayloadOffset);
    _lastMessageStartPosition = _sendBuffer.getPosition();
    _sendBuffer.putByte(PVA_MAGIC);
    _sendBuffer.putByte(_clientServerFlag ? PVA_SERVER_PROTOCOL_REVISION : PVA_CLIENT_PROTOCOL_REVISION);
    _sendBuffer.putByte(
        (_lastSegmentedMessageType | _byteOrderFlag | _clientServerFlag));	// data message
    _sendBuffer.putByte(command);	// command
    _sendBuffer.putInt(payloadSize);

    // apply offset
    if (_nextMessagePayloadOffset > 0)
        _sendBuffer.setPosition(
            _sendBuffer.getPosition() + _nextMessagePayloadOffset);
}


void AbstractCodec::putControlMessage(
    epics::pvData::int8 command,
    epics::pvData::int32 data) {

    _lastMessageStartPosition =
        std::numeric_limits<size_t>::max();		// TODO revise this
    ensureBuffer(PVA_MESSAGE_HEADER_SIZE);
    _sendBuffer.putByte(PVA_MAGIC);
    _sendBuffer.putByte(_clientServerFlag ? PVA_SERVER_PROTOCOL_REVISION : PVA_CLIENT_PROTOCOL_REVISION);
    _sendBuffer.putByte((0x01 | _byteOrderFlag | _clientServerFlag));	// control message
    _sendBuffer.putByte(command);	// command
    _sendBuffer.putInt(data);		// data
}


void AbstractCodec::endMessage() {
    endMessage(false);
}


void AbstractCodec::endMessage(bool hasMoreSegments) {

    if (_lastMessageStartPosition != std::numeric_limits<size_t>::max())
    {
        std::size_t lastPayloadBytePosition = _sendBuffer.getPosition();

        // set paylaod size (non-aligned)
        std::size_t payloadSize =
            lastPayloadBytePosition -
            _lastMessageStartPosition - PVA_MESSAGE_HEADER_SIZE;

        _sendBuffer.putInt(_lastMessageStartPosition + 4, payloadSize);

        // set segmented bit
        if (hasMoreSegments) {
            // first segment
            if (_lastSegmentedMessageType == 0)
            {
                std::size_t flagsPosition = _lastMessageStartPosition + 2;
                epics::pvData::int8 type = _sendBuffer.getByte(flagsPosition);
                // set first segment bit
                _sendBuffer.putByte(flagsPosition, (type | 0x10));
                // first + last segment bit == in-between segment
                _lastSegmentedMessageType = type | 0x30;
                _lastSegmentedMessageCommand =
                    _sendBuffer.getByte(flagsPosition + 1);
            }
            _nextMessagePayloadOffset = 0;
        }
        else
        {
            // last segment
            if (_lastSegmentedMessageType != 0)
            {
                std::size_t flagsPosition = _lastMessageStartPosition + 2;
                // set last segment bit (by clearing first segment bit)
                _sendBuffer.putByte(flagsPosition,
                                     (_lastSegmentedMessageType & 0xEF));
                _lastSegmentedMessageType = 0;
            }
            _nextMessagePayloadOffset = 0;
        }

        // TODO
        /*
        // manage markers
        final int position = sendBuffer.position();
        final int bytesLeft = sendBuffer.remaining();
        if (position >= nextMarkerPosition && bytesLeft >=
        PVAConstants.PVA_MESSAGE_HEADER_SIZE)
        {
        sendBuffer.put(PVAConstants.PVA_MAGIC);
        sendBuffer.put(PVAConstants.PVA_VERSION);
        sendBuffer.put((byte)(0x01 | byteOrderFlag));	// control data
        sendBuffer.put((byte)0);	// marker
        sendBuffer.putInt((int)(totalBytesSent + position +
        PVAConstants.PVA_MESSAGE_HEADER_SIZE));
        nextMarkerPosition = position + markerPeriodBytes;
        }
        */
        _lastMessageStartPosition = std::numeric_limits<size_t>::max();
    }
}

void AbstractCodec::ensureBuffer(std::size_t size) {

    if (_sendBuffer.getRemaining() >= size)
        return;

    // too large for buffer...
    if (_maxSendPayloadSize < size) {
        std::ostringstream msg;
        msg << "requested for buffer size " <<
            size << ", but only " << _maxSendPayloadSize << " available.";
        std::string s = msg.str();
        LOG(logLevelWarn,
            "%s at %s:%d.,", msg.str().c_str(), __FILE__, __LINE__);
        throw std::invalid_argument(s);
    }

    while (_sendBuffer.getRemaining() < size)
        flush(false);
}

// assumes startMessage was called (or header is in place), because endMessage(true) is later called that peeks and sets _lastSegmentedMessageType
void AbstractCodec::flushSerializeBuffer() {
    flush(false);
}

void AbstractCodec::flushSendBuffer() {

    _sendBuffer.flip();

    try {
        send(&_sendBuffer);
    } catch (io_exception &) {
        try {
            if (isOpen())
                close();
        } catch (io_exception &) {
            // noop, best-effort close
        }
        throw connection_closed_exception("Failed to send buffer.");
    }

    _sendBuffer.clear();

    _lastMessageStartPosition = std::numeric_limits<size_t>::max();
}

void AbstractCodec::flush(bool lastMessageCompleted) {

    // automatic end
    endMessage(!lastMessageCompleted);

    // flush send buffer
    flushSendBuffer();

    // start with last header
    if (!lastMessageCompleted && _lastSegmentedMessageType != 0)
        startMessage(_lastSegmentedMessageCommand, 0);
}


// thows io_exception, connection_closed_exception
void AbstractCodec::processWrite() {

    switch (_writeMode)
    {
    case PROCESS_SEND_QUEUE:
        processSendQueue();
        break;
    case WAIT_FOR_READY_SIGNAL:
        _writeOpReady = true;
        break;
    }
}


void AbstractCodec::send(ByteBuffer *buffer)
{

    // On Windows, limiting the buffer size is important to prevent
    // poor throughput performances when transferring large amount of
    // data over non-blocking socket. See Microsoft KB article KB823764.
    // We do it also for other systems just to be safe.
    std::size_t maxBytesToSend = (size_t)-1;
    //  std::min<int32_t>(
    //  _socketSendBufferSize, _remoteTransportSocketReceiveBufferSize) / 2;

    std::size_t limit = buffer->getLimit();
    std::size_t bytesToSend = limit - buffer->getPosition();

    // limit sending
    if (bytesToSend > maxBytesToSend)
    {
        bytesToSend = maxBytesToSend;
        buffer->setLimit(buffer->getPosition() + bytesToSend);
    }

    int tries = 0;
    while (buffer->getRemaining() > 0)
    {

        //int p = buffer.position();
        int bytesSent = write(buffer);

        if (bytesSent < 0)
        {
            // connection lost
            close();
            throw connection_closed_exception("bytesSent < 0");
        }
        else if (bytesSent == 0)
        {
            sendBufferFull(tries++);
            continue;
        }

        atomic::add(_totalBytesSent, bytesSent);

        // readjust limit
        if (bytesToSend == maxBytesToSend)
        {
            bytesToSend = limit - buffer->getPosition();

            if(bytesToSend > maxBytesToSend)
                bytesToSend = maxBytesToSend;

            buffer->setLimit(buffer->getPosition() + bytesToSend);
        }
        tries = 0;
    }
}


void AbstractCodec::processSendQueue()
{

    {
        std::size_t senderProcessed = 0;
        while (senderProcessed++ < MAX_MESSAGE_SEND)
        {
            TransportSender::shared_pointer sender;
            _sendQueue.pop_front_try(sender);
            if (sender.get() == 0)
            {
                // flush
                if (_sendBuffer.getPosition() > 0)
                    flush(true);

                sendCompleted();	// do not schedule sending

                if (terminated())			// termination
                    break;
                // termination (we want to process even if shutdown)
                _sendQueue.pop_front(sender);
            }

            try {
                processSender(sender);
            } catch(...) {
                if (_sendBuffer.getPosition() > 0)
                    flush(true);
                sendCompleted();
                throw;
            }
        }
    }

    // flush
    if (_sendBuffer.getPosition() > 0)
        flush(true);
}


void AbstractCodec::enqueueSendRequest(
    TransportSender::shared_pointer const & sender) {
    _sendQueue.push_back(sender);
    scheduleSend();
}


void AbstractCodec::setSenderThread()
{
    _senderThread = epicsThreadGetIdSelf();
}


void AbstractCodec::processSender(
    TransportSender::shared_pointer const & sender)
{

    ScopedLock lock(sender);

    try {
        _lastMessageStartPosition = _sendBuffer.getPosition();

        size_t before = atomic::get(_totalBytesSent) + _sendBuffer.getPosition();

        sender->send(&_sendBuffer, this);

        // automatic end (to set payload size)
        endMessage(false);

        size_t after = atomic::get(_totalBytesSent) + _sendBuffer.getPosition();

        atomic::add(sender->bytesTX, after - before);
    }
    catch (connection_closed_exception & ) {
        throw;
    }
    catch (std::exception &e ) {

        std::ostringstream msg;
        msg << "an exception caught while processing a send message: "
            << e.what();
        LOG(logLevelWarn, "%s at %s:%d.",
            msg.str().c_str(), __FILE__, __LINE__);

        try {
            close();
        } catch (io_exception & ) {
            // noop
        }

        throw connection_closed_exception(msg.str());
    }
}


void AbstractCodec::enqueueSendRequest(
    TransportSender::shared_pointer const & sender,
    std::size_t requiredBufferSize) {

    if (_senderThread == epicsThreadGetIdSelf() &&
            _sendQueue.empty() &&
            _sendBuffer.getRemaining() >= requiredBufferSize)
    {
        processSender(sender);
        if (_sendBuffer.getPosition() > 0)
        {
            scheduleSend();
        }
    }
    else
        enqueueSendRequest(sender);
}


void AbstractCodec::setRecipient(osiSockAddr const & sendTo) {
    _sendTo = sendTo;
}


void AbstractCodec::setByteOrder(int byteOrder)
{
    _socketBuffer.setEndianess(byteOrder);
    // TODO sync
    _sendBuffer.setEndianess(byteOrder);
    _byteOrderFlag = EPICS_ENDIAN_BIG == byteOrder ? 0x80 : 0x00;
}


bool AbstractCodec::directSerialize(ByteBuffer* /*existingBuffer*/, const char* toSerialize,
                                    std::size_t elementCount, std::size_t elementSize)
{
    // TODO overflow check of "size_t count", overflow int32 field of payloadSize header field
    // TODO max message size in connection validation
    std::size_t count = elementCount * elementSize;

    // TODO find smart limit
    // check if direct mode actually pays off
    if (count < 64*1024)
        return false;

    //
    // first end current message, and write a header of next "directly serialized" message
    //

    // first end current message indicating the we will segment
    endMessage(true);

    // append segmented message header with payloadSize == count
    // TODO size_t to int32
    startMessage(_lastSegmentedMessageCommand, 0, static_cast<int32>(count));

    // flush
    flushSendBuffer();

    // TODO think if alignment is preserved after...

    //
    // send toSerialize buffer
    //
    ByteBuffer wrappedBuffer(const_cast<char*>(toSerialize), count);
    send(&wrappedBuffer);

    //
    // continue where we left before calling directSerialize
    //
    startMessage(_lastSegmentedMessageCommand, 0);

    return true;
}

bool AbstractCodec::directDeserialize(ByteBuffer *existingBuffer, char* deserializeTo,
                                      std::size_t elementCount, std::size_t elementSize)
{
    return false;
}

//
//
//  BlockingAbstractCodec
//
//
//

BlockingTCPTransportCodec::~BlockingTCPTransportCodec()
{
    REFTRACE_DECREMENT(num_instances);

    waitJoin();
}

void BlockingTCPTransportCodec::readPollOne() {
    throw std::logic_error("should not be called for blocking IO");
}


void BlockingTCPTransportCodec::writePollOne() {
    throw std::logic_error("should not be called for blocking IO");
}


void BlockingTCPTransportCodec::close() {

    if (_isOpen.getAndSet(false))
    {
        // always close in the same thread, same way, etc.
        // wakeup processSendQueue

        // clean resources (close socket)
        internalClose();

        // Break sender from queue wait
        BreakTransport::shared_pointer B(new BreakTransport);
        enqueueSendRequest(B);
    }
}

void BlockingTCPTransportCodec::waitJoin()
{
    assert(!_isOpen.get());
    _sendThread.exitWait();
    _readThread.exitWait();
}

void BlockingTCPTransportCodec::internalClose()
{
    {

        epicsSocketSystemCallInterruptMechanismQueryInfo info  =
            epicsSocketSystemCallInterruptMechanismQuery ();
        switch ( info )
        {
        case esscimqi_socketCloseRequired:
            epicsSocketDestroy ( _channel );
            break;
        case esscimqi_socketBothShutdownRequired:
        {
            /*int status =*/ ::shutdown ( _channel, SHUT_RDWR );
            /*
            if ( status ) {
                char sockErrBuf[64];
                epicsSocketConvertErrnoToString (
                    sockErrBuf, sizeof ( sockErrBuf ) );
            LOG(logLevelDebug,
                "TCP socket to %s failed to shutdown: %s.",
                inetAddressToString(_socketAddress).c_str(), sockErrBuf);
            }
            */
            epicsSocketDestroy ( _channel );
        }
        break;
        case esscimqi_socketSigAlarmRequired:
        // not supported anymore anyway
        default:
            epicsSocketDestroy(_channel);
        }
    }

    Transport::shared_pointer thisSharedPtr = this->shared_from_this();
    _context->getTransportRegistry()->remove(thisSharedPtr);

    if (IS_LOGGABLE(logLevelDebug))
    {
        LOG(logLevelDebug,
            "TCP socket to %s is to be closed.",
            _socketName.c_str());
    }
}

bool BlockingTCPTransportCodec::terminated() {
    return !isOpen();
}


bool BlockingTCPTransportCodec::isOpen() {
    return _isOpen.get();
}


// NOTE: must not be called from constructor (e.g. needs shared_from_this())
void BlockingTCPTransportCodec::start() {

    _readThread.start();

    _sendThread.start();

}


void BlockingTCPTransportCodec::receiveThread()
{
    /* This innocuous ref. is an important hack.
     * The code behind Transport::close() will cause
     * channels and operations to drop references
     * to this transport.  This ref. keeps it from
     * being destroyed way down the call stack, from
     * which it is apparently not possible to return
     * safely.  Rather than try to untangle this
     * knot, just keep this ref...
     */
    Transport::shared_pointer ptr(this->shared_from_this());

    // initially enable timeout for all clients to weed out
    // impersonators (security scanners?)
    setRxTimeout(true);

    while (this->isOpen())
    {
        try {
            this->processRead();
            continue;
        } catch (std::exception &e) {
            PRINT_EXCEPTION(e);
            LOG(logLevelError,
                "an exception caught while in receiveThread at %s:%d: %s",
                __FILE__, __LINE__, e.what());
        } catch (...) {
            LOG(logLevelError,
                "unknown exception caught while in receiveThread at %s:%d.",
                __FILE__, __LINE__);
        }
        // exception
        close();
    }
}


void BlockingTCPTransportCodec::sendThread()
{
    // cf. the comment in receiveThread()
    Transport::shared_pointer ptr(this->shared_from_this());

    this->setSenderThread();

    while (this->isOpen())
    {
        try {
            this->processWrite();
            continue;
        } catch (connection_closed_exception &cce) {
            // noop
        } catch (std::exception &e) {
            PRINT_EXCEPTION(e);
            LOG(logLevelWarn,
                "an exception caught while in sendThread at %s:%d: %s",
                __FILE__, __LINE__, e.what());
        } catch (...) {
            LOG(logLevelWarn,
                "unknown exception caught while in sendThread at %s:%d.",
                __FILE__, __LINE__);
        }
        // exception
        close();
    }
    _sendQueue.clear();
}

void BlockingTCPTransportCodec::setRxTimeout(bool ena)
{
    double timeout = !ena ? 0.0 : std::max(0.0, _context->getConfiguration()->getPropertyAsDouble("EPICS_PVA_CONN_TMO", 30.0));
#ifdef _WIN32
    DWORD timo = DWORD(timeout*1000); // in milliseconds
#else
    timeval timo;
    timo.tv_sec = unsigned(timeout);
    timo.tv_usec = (timeout-timo.tv_sec)*1e6;
#endif

    int ret = setsockopt(_channel, SOL_SOCKET, SO_RCVTIMEO, (char*)&timo, sizeof(timo));
    if(ret==-1) {
        int err = SOCKERRNO;
        static int lasterr;
        if(err!=lasterr) {
            errlogPrintf("%s: Unable to set RX timeout: %d\n", _socketName.c_str(), err);
            lasterr = err;
        }
    }
}

void BlockingTCPTransportCodec::sendBufferFull(int tries) {
    // TODO constants
    epicsThreadSleep(std::max<double>(tries * 0.1, 1));
}


//
//
//  BlockingTCPTransportCodec
//
//
//

size_t BlockingTCPTransportCodec::num_instances;

BlockingTCPTransportCodec::BlockingTCPTransportCodec(bool serverFlag, const Context::shared_pointer &context,
    SOCKET channel, const ResponseHandler::shared_pointer &responseHandler,
    size_t sendBufferSize,
    size_t receiveBufferSize, int16 priority)
    :AbstractCodec(
         serverFlag,
         sendBufferSize,
         receiveBufferSize,
         sendBufferSize,
         true)
    ,_readThread(epics::pvData::Thread::Config(this, &BlockingTCPTransportCodec::receiveThread)
                 .prio(epicsThreadPriorityCAServerLow)
                 .name("TCP-rx")
                 .stack(epicsThreadStackBig)
                 .autostart(false))
    ,_sendThread(epics::pvData::Thread::Config(this, &BlockingTCPTransportCodec::sendThread)
                 .prio(epicsThreadPriorityCAServerLow)
                 .name("TCP-tx")
                 .stack(epicsThreadStackBig)
                 .autostart(false))
    ,_channel(channel)
    ,_context(context), _responseHandler(responseHandler)
    ,_remoteTransportReceiveBufferSize(MAX_TCP_RECV)
    ,_priority(priority)
    ,_verified(false)
{
    REFTRACE_INCREMENT(num_instances);

    _isOpen.getAndSet(true);

    // get remote address
    osiSocklen_t saSize = sizeof(sockaddr);
    int retval = getpeername(_channel, &(_socketAddress.sa), &saSize);
    if(unlikely(retval<0)) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        LOG(logLevelError,
            "Error fetching socket remote address: %s.",
            errStr);
        _socketName = "<unknown>:0";
    } else {
        char ipAddrStr[24];
        ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
        _socketName = ipAddrStr;
    }
}


void BlockingTCPTransportCodec::invalidDataStreamHandler() {
    close();
}


int BlockingTCPTransportCodec::write(
    epics::pvData::ByteBuffer *src) {

    std::size_t remaining;
    while((remaining=src->getRemaining()) > 0) {

        int bytesSent = ::send(_channel,
                               &src->getBuffer()[src->getPosition()],
                               remaining, 0);

        // NOTE: do not log here, you might override SOCKERRNO relevant to recv() operation above

        // TODO winsock return 0 on disconnect (blocking socket) ?

        if(unlikely(bytesSent<0)) {

            int socketError = SOCKERRNO;

            // spurious EINTR check
            if (socketError==SOCK_EINTR)
                continue;
            else if (socketError==SOCK_ENOBUFS)
                return 0;
        }

        if (bytesSent > 0) {
            src->setPosition(src->getPosition() + bytesSent);
        }

        return bytesSent;

    }

    return 0;
}


int BlockingTCPTransportCodec::read(epics::pvData::ByteBuffer* dst) {

    std::size_t remaining;
    while((remaining=dst->getRemaining()) > 0) {

        // read
        std::size_t pos = dst->getPosition();

        int bytesRead = ::recv(_channel,
                             (char*)(dst->getBuffer()+pos), remaining, 0);

        // NOTE: do not log here, you might override SOCKERRNO relevant to recv() operation above

        if(unlikely(bytesRead==0)) {
            return -1;    // 0 means connection loss for blocking transport, notify codec by returning -1

        } else if(unlikely(bytesRead<0)) {
            int err = SOCKERRNO;

            if(err==SOCK_EINTR) {
                // interrupted by signal.  Retry
                continue;

            } else if(err==SOCK_EWOULDBLOCK || err==EAGAIN || err==SOCK_EINPROGRESS
                      || err==SOCK_ETIMEDOUT
                      || err==SOCK_ECONNABORTED || err==SOCK_ECONNRESET
                      ) {
                // different ways of saying timeout.
                // Linux: EAGAIN or EWOULDBLOCK, or EINPROGRESS
                // WIN32: WSAETIMEDOUT
                // others that RSRV checks for, but may not need to, ECONNABORTED, ECONNRESET

                // Note: with windows, after ETIMEOUT leaves the socket in an undefined state.
                //       so it must be closed.  (cf. SO_RCVTIMEO)

                return -1;

            } else {
                // some other (fatal) error
                if(_isOpen.get())
                    errlogPrintf("%s : Connection closed with RX socket error %d\n", _socketName.c_str(), err);
                return -1;
            }
        }

        dst->setPosition(dst->getPosition() + bytesRead);
        return bytesRead;
    }

    return 0;
}


bool BlockingTCPTransportCodec::verify(epics::pvData::int32 timeoutMs) {
    return _verifiedEvent.wait(timeoutMs/1000.0) && _verified;
}

void BlockingTCPTransportCodec::verified(epics::pvData::Status const & status) {
    epics::pvData::Lock lock(_mutex);

    if (IS_LOGGABLE(logLevelDebug) && !status.isOK())
    {
        LOG(logLevelDebug, "Failed to verify connection to %s: %s.", _socketName.c_str(), status.getMessage().c_str());
    }

    {
        Guard G(_mutex);
        _verified = status.isSuccess();
    }
    _verifiedEvent.signal();
}

void BlockingTCPTransportCodec::authNZMessage(epics::pvData::PVStructure::shared_pointer const & data) {
    AuthenticationSession::shared_pointer sess;
    {
        Guard G(_mutex);
        sess = _authSession;
    }
    if (sess)
        sess->messageReceived(data);
    else
    {
        char ipAddrStr[24];
        ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
        LOG(logLevelWarn, "authNZ message received from '%s' but no security plug-in session active.", ipAddrStr);
    }
}


class SecurityPluginMessageTransportSender : public TransportSender {
public:
    POINTER_DEFINITIONS(SecurityPluginMessageTransportSender);

    SecurityPluginMessageTransportSender(PVStructure::const_shared_pointer const & data) :
        _data(data)
    {
    }

    void send(ByteBuffer* buffer, TransportSendControl* control) {
        control->startMessage(CMD_AUTHNZ, 0);
        SerializationHelper::serializeFull(buffer, control, _data);
        // send immediately
        control->flush(true);
    }

private:
    PVStructure::const_shared_pointer _data;
};

void BlockingTCPTransportCodec::sendSecurityPluginMessage(epics::pvData::PVStructure::const_shared_pointer const & data) {
    SecurityPluginMessageTransportSender::shared_pointer spmts(new SecurityPluginMessageTransportSender(data));
    enqueueSendRequest(spmts);
}





BlockingServerTCPTransportCodec::BlockingServerTCPTransportCodec(
    Context::shared_pointer const & context,
    SOCKET channel,
    ResponseHandler::shared_pointer const & responseHandler,
    int32_t sendBufferSize,
    int32_t receiveBufferSize)
    :BlockingTCPTransportCodec(true, context, channel, responseHandler,
                               sendBufferSize, receiveBufferSize, PVA_DEFAULT_PRIORITY)
    ,_lastChannelSID(0)
    ,_verificationStatus(pvData::Status::fatal("Uninitialized error"))
    ,_verifyOrVerified(false)
{
    // NOTE: priority not yet known, default priority is used to
    //register/unregister
    // TODO implement priorities in Reactor... not that user will
    // change it.. still getPriority() must return "registered" priority!
}


BlockingServerTCPTransportCodec::~BlockingServerTCPTransportCodec() {
}


pvAccessID BlockingServerTCPTransportCodec::preallocateChannelSID() {

    Lock lock(_channelsMutex);
    // search first free (theoretically possible loop of death)
    pvAccessID sid = ++_lastChannelSID;
    while(_channels.find(sid)!=_channels.end())
        sid = ++_lastChannelSID;
    return sid;
}


void BlockingServerTCPTransportCodec::registerChannel(
    pvAccessID sid,
    ServerChannel::shared_pointer const & channel) {

    Lock lock(_channelsMutex);
    _channels[sid] = channel;

}


void BlockingServerTCPTransportCodec::unregisterChannel(pvAccessID sid) {

    Lock lock(_channelsMutex);
    _channels.erase(sid);
}


ServerChannel::shared_pointer
BlockingServerTCPTransportCodec::getChannel(pvAccessID sid) {

    Lock lock(_channelsMutex);

    std::map<pvAccessID, ServerChannel::shared_pointer>::iterator it =
        _channels.find(sid);

    if(it!=_channels.end()) return it->second;

    return ServerChannel::shared_pointer();
}


size_t BlockingServerTCPTransportCodec::getChannelCount() const {

    Lock lock(_channelsMutex);
    return _channels.size();
}

void BlockingServerTCPTransportCodec::getChannels(std::vector<ServerChannel::shared_pointer>& channels) const
{
    Lock lock(_channelsMutex);
    for(_channels_t::const_iterator it(_channels.begin()), end(_channels.end());
        it!=end; ++it)
    {
        channels.push_back(it->second);
    }
}

void BlockingServerTCPTransportCodec::send(ByteBuffer* buffer,
        TransportSendControl* control) {

    if (!_verifyOrVerified)
    {
        _verifyOrVerified = true;

        //
        // set byte order control message
        //

        ensureBuffer(PVA_MESSAGE_HEADER_SIZE);
        buffer->putByte(PVA_MAGIC);
        buffer->putByte(PVA_SERVER_PROTOCOL_REVISION);
        buffer->putByte(
            0x01 | 0x40 | ((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG)
                           ? 0x80 : 0x00));		// control + server + endian
        buffer->putByte(CMD_SET_ENDIANESS);		// set byte order
        buffer->putInt(0);


        //
        // send verification message
        //
        control->startMessage(CMD_CONNECTION_VALIDATION, 4+2);

        // receive buffer size
        buffer->putInt(static_cast<int32>(getReceiveBufferSize()));

        // server introspection registy max size
        // TODO
        buffer->putShort(0x7FFF);

        // list of authNZ plugin names advertised to this client

        AuthenticationRegistry::list_t plugins;
        AuthenticationRegistry::servers().snapshot(plugins); // copy
        std::vector<std::string> validSPNames;
        validSPNames.reserve(plugins.size()); // assume all will be valid

        PeerInfo info;
        info.transport = "pva";
        info.peer = _socketName;
        info.transportVersion = this->getRevision();

        // filter plugins which may be used by this peer
        for(AuthenticationRegistry::list_t::iterator it(plugins.begin()), end(plugins.end());
            it!=end; ++it)
        {
            info.authority = it->first;
            if(it->second->isValidFor(info))
                validSPNames.push_back(it->first);
        }

        SerializeHelper::writeSize(validSPNames.size(), buffer, this);
        for (vector<string>::const_iterator iter(validSPNames.begin()), end(validSPNames.end());
             iter != end; iter++)
        {
            SerializeHelper::serializeString(*iter, buffer, this);
        }

        {
            Guard G(_mutex);
            advertisedAuthPlugins.swap(validSPNames);
        }

        // send immediately
        control->flush(true);
    }
    else
    {
        //
        // send verified message
        //
        control->startMessage(CMD_CONNECTION_VALIDATED, 0);

        pvData::Status sts;
        {
            Lock lock(_mutex);
            sts = _verificationStatus;
        }
        sts.serialize(buffer, control);

        // send immediately
        control->flush(true);

    }
}

void BlockingServerTCPTransportCodec::destroyAllChannels() {
    Lock lock(_channelsMutex);
    if(_channels.size()==0) return;

    if (IS_LOGGABLE(logLevelDebug))
    {
        LOG(
            logLevelDebug,
            "Transport to %s still has %zu channel(s) active and closing...",
            _socketName.c_str(), _channels.size());
    }

    _channels_t temp;
    temp.swap(_channels);

    for(_channels_t::iterator it(temp.begin()), end(temp.end()); it!=end; ++it)
        it->second->destroy();
}

void BlockingServerTCPTransportCodec::internalClose() {
    Transport::shared_pointer thisSharedPtr = shared_from_this();
    BlockingTCPTransportCodec::internalClose();
    destroyAllChannels();
}

void BlockingServerTCPTransportCodec::authenticationCompleted(epics::pvData::Status const & status,
                                                              const std::tr1::shared_ptr<PeerInfo>& peer)
{
    if (IS_LOGGABLE(logLevelDebug))
    {
        LOG(logLevelDebug, "Authentication completed with status '%s' for PVA client: %s.", Status::StatusTypeName[status.getType()], _socketName.c_str());
    }

    if(peer)
        AuthorizationRegistry::plugins().run(peer);

    bool isVerified;
    {
        Guard G(_mutex);
        isVerified = _verified;
        if(status.isSuccess())
            _peerInfo = peer;
        else
            _peerInfo.reset();
    }

    if (!isVerified)
        verified(status);
    else if (!status.isSuccess())
    {
        string errorMessage = "Re-authentication failed: " + status.getMessage();
        if (!status.getStackDump().empty())
            errorMessage += "\n" + status.getStackDump();
        LOG(logLevelInfo, "%s", errorMessage.c_str());

        close();
    }
}

void BlockingServerTCPTransportCodec::authNZInitialize(const std::string& securityPluginName,
                                                       const epics::pvData::PVStructure::shared_pointer& data)
{
    AuthenticationPlugin::shared_pointer plugin(AuthenticationRegistry::servers().lookup(securityPluginName));
    // attempting the force use of an un-advertised/non-existant plugin is treated as a protocol error.
    // We cheat here by assuming the the registry doesn't often change after server start,
    // and don't test if securityPluginName is in advertisedAuthPlugins
    if(!plugin)
        throw std::runtime_error(_socketName+" failing attempt to select non-existant auth. plugin "+securityPluginName);

    PeerInfo::shared_pointer info(new PeerInfo);
    info->peer = _socketName;
    info->transport = "pva";
    info->transportVersion = getRevision();
    info->authority = securityPluginName;

    if (!plugin->isValidFor(*info))
        verified(pvData::Status::error("invalid security plug-in name"));

    if (IS_LOGGABLE(logLevelDebug))
    {
        LOG(logLevelDebug, "Accepted security plug-in '%s' for PVA client: %s.", securityPluginName.c_str(), _socketName.c_str());
    }

    AuthenticationSession::shared_pointer sess(plugin->createSession(info, shared_from_this(), data));

    Guard G(_mutex);
    _authSessionName = securityPluginName;
    _authSession.swap(sess);
}





BlockingClientTCPTransportCodec::BlockingClientTCPTransportCodec(
    Context::shared_pointer const & context,
    SOCKET channel,
    ResponseHandler::shared_pointer const & responseHandler,
    int32_t sendBufferSize,
    int32_t receiveBufferSize,
    ClientChannelImpl::shared_pointer const & client,
    epics::pvData::int8 /*remoteTransportRevision*/,
    float heartbeatInterval,
    int16_t priority ) :
    BlockingTCPTransportCodec(false, context, channel, responseHandler,
                              sendBufferSize, receiveBufferSize, priority),
    _connectionTimeout(heartbeatInterval),
    _verifyOrEcho(true),
    sendQueued(true) // don't start sending echo until after auth complete
{
    // initialize owners list, send queue
    acquire(client);

    // use immediate for clients
    //setFlushStrategy(DELAYED);
}

void BlockingClientTCPTransportCodec::start()
{
    TimerCallbackPtr tcb = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
    // add some randomness to our timer phase
    double R = float(rand())/RAND_MAX; // [0, 1]
    // shape a bit
    R = R*0.5 + 0.5; // [0.5, 1.0]
    _context->getTimer()->schedulePeriodic(tcb, _connectionTimeout/2.0*R, _connectionTimeout/2.0);
    BlockingTCPTransportCodec::start();
}

BlockingClientTCPTransportCodec::~BlockingClientTCPTransportCodec() {
}









void BlockingClientTCPTransportCodec::callback()
{
    {
        Guard G(_mutex);
        if(sendQueued) return;
        sendQueued = true;
    }
    // send echo
    TransportSender::shared_pointer transportSender = std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
    enqueueSendRequest(transportSender);
}

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from code at %s:%d.", __FILE__, __LINE__); }

bool BlockingClientTCPTransportCodec::acquire(ClientChannelImpl::shared_pointer const & client) {
    Lock lock(_mutex);
    if(isClosed()) return false;

    if (IS_LOGGABLE(logLevelDebug))
    {
        LOG(logLevelDebug, "Acquiring transport to %s.", _socketName.c_str());
    }

    _owners[client->getID()] = ClientChannelImpl::weak_pointer(client);
    //_owners.insert(ClientChannelImpl::weak_pointer(client));

    return true;
}

// _mutex is held when this method is called
void BlockingClientTCPTransportCodec::internalClose() {
    BlockingTCPTransportCodec::internalClose();

    TimerCallbackPtr tcb = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
    _context->getTimer()->cancel(tcb);

    // _owners cannot change when transport is closed

    // Notifies clients about disconnect.

    // check if still acquired
    size_t refs = _owners.size();
    if(refs>0) {

        if (IS_LOGGABLE(logLevelDebug))
        {
            LOG(
                logLevelDebug,
                "Transport to %s still has %zu client(s) active and closing...",
                _socketName.c_str(), refs);
        }

        TransportClientMap_t::iterator it = _owners.begin();
        for(; it!=_owners.end(); it++) {
            ClientChannelImpl::shared_pointer client = it->second.lock();
            if (client)
            {
                EXCEPTION_GUARD(client->transportClosed());
            }
        }

    }

    _owners.clear();
}

//void BlockingClientTCPTransportCodec::release(ClientChannelImpl::shared_pointer const & client) {
void BlockingClientTCPTransportCodec::release(pvAccessID clientID) {
    Lock lock(_mutex);
    if(isClosed()) return;

    if (IS_LOGGABLE(logLevelDebug))
    {
        LOG(logLevelDebug, "Releasing TCP transport to %s.", _socketName.c_str());
    }

    _owners.erase(clientID);
    //_owners.erase(ClientChannelImpl::weak_pointer(client));

    // not used anymore, close it
    // TODO consider delayed destruction (can improve performance!!!)
    if(_owners.size()==0) {
        lock.unlock();
        close();
    }
}

void BlockingClientTCPTransportCodec::send(ByteBuffer* buffer,
                                           TransportSendControl* control)
{
    bool voe;
    {
        Guard G(_mutex);
        sendQueued = false;
        voe = _verifyOrEcho;
        _verifyOrEcho = false;
    }

    if(voe) {
        /*
         * send verification response message
         */

        control->startMessage(CMD_CONNECTION_VALIDATION, 4+2+2);

        // receive buffer size
        buffer->putInt(static_cast<int32>(getReceiveBufferSize()));

        // max introspection registry size
        // TODO
        buffer->putShort(0x7FFF);

        // QoS (aka connection priority)
        buffer->putShort(getPriority());

        std::string pluginName;
        AuthenticationSession::shared_pointer session;
        {
            Guard G(_mutex);
            pluginName = _authSessionName;
            session = _authSession;
        }

        if (session)
        {
            // selected authNZ plug-in name
            SerializeHelper::serializeString(_authSessionName, buffer, control);

            // optional authNZ plug-in initialization data
            SerializationHelper::serializeFull(buffer, control, session->initializationData());
        }
        else
        {
            //TODO: allowed?
            // emptry authNZ plug-in name
            SerializeHelper::serializeString("", buffer, control);

            // no authNZ plug-in initialization data
            SerializationHelper::serializeNullField(buffer, control);
        }

        // send immediately
        control->flush(true);
    }
    else {
        control->startMessage(CMD_ECHO, 0);
        // send immediately
        control->flush(true);
    }

}


void BlockingClientTCPTransportCodec::authNZInitialize(const std::vector<std::string>& offeredSecurityPlugins)
{
    AuthenticationRegistry& plugins = AuthenticationRegistry::clients();
    std::string selectedName;
    AuthenticationPlugin::shared_pointer plugin;

    // because of a missing break; the original SecurityPlugin effectively treated the offered list as being
    // in order of increasing preference (last is preferred).
    // we continue with this because, hey isn't compatibility fun...

    for(std::vector<std::string>::const_reverse_iterator it(offeredSecurityPlugins.rbegin()), end(offeredSecurityPlugins.rend());
        it!=end; ++it)
    {
        plugin = plugins.lookup(*it);
        if(plugin) {
            selectedName = *it;
            break;
        }
    }

    if(!plugin) {
        // mis-match and legacy.  some early servers (java?) don't advertise any plugins.
        // treat this as anonymous
        selectedName = "anonymous";
        plugin = plugins.lookup(selectedName);
        assert(plugin); // fallback required
    }

    {
        PeerInfo::shared_pointer info(new PeerInfo);
        info->peer = _socketName; // this is the server name
        info->transport = "pva";
        info->transportVersion = getRevision();
        info->authority = selectedName;

        AuthenticationSession::shared_pointer sess(plugin->createSession(info, shared_from_this(), pvData::PVStructure::shared_pointer()));

        Guard G(_mutex);
        _authSessionName = selectedName;
        _authSession = sess;
    }

    TransportSender::shared_pointer transportSender = std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
    enqueueSendRequest(transportSender);
}

void BlockingClientTCPTransportCodec::authenticationCompleted(epics::pvData::Status const & status,
                                                              const std::tr1::shared_ptr<PeerInfo>& peer)
{
    // noop for client side (server will send ConnectionValidation message)
}

void BlockingClientTCPTransportCodec::verified(epics::pvData::Status const & status)
{
    AuthenticationSession::shared_pointer sess;
    {
        Guard G(_mutex);
        sess = _authSession;
    }
    if(sess)
        sess->authenticationComplete(status);
    this->BlockingTCPTransportCodec::verified(status);
}

}
}
}
