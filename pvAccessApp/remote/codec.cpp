/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/
#ifdef _WIN32
#define NOMINMAX
#endif


#include <epicsThread.h>
#include <osiSock.h>

#include <sys/types.h>
#include <sstream>
#include <stdexcept>
#include <limits>

#define epicsExportSharedSymbols
#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/namedLockPattern.h>
#include <pv/hexDump.h>
#include <pv/logger.h>
#include <pv/codec.h>

using namespace epics::pvData;
using namespace epics::pvAccess;


namespace epics {
  namespace pvAccess {
    namespace detail {

    const std::size_t AbstractCodec::MAX_MESSAGE_PROCESS = 100;
    const std::size_t AbstractCodec::MAX_MESSAGE_SEND = 100;
    const std::size_t AbstractCodec::MAX_ENSURE_SIZE = 1024;
    const std::size_t AbstractCodec::MAX_ENSURE_DATA_SIZE = MAX_ENSURE_SIZE/2;
    const std::size_t AbstractCodec::MAX_ENSURE_BUFFER_SIZE = MAX_ENSURE_SIZE;
    const std::size_t AbstractCodec::MAX_ENSURE_DATA_BUFFER_SIZE = 1024;

    AbstractCodec::AbstractCodec(
      std::tr1::shared_ptr<epics::pvData::ByteBuffer> const & receiveBuffer,
      std::tr1::shared_ptr<epics::pvData::ByteBuffer> const & sendBuffer,
      int32_t socketSendBufferSize, 
      bool blockingProcessQueue): 
    //PROTECTED
    _readMode(NORMAL), _version(0), _flags(0), _command(0), _payloadSize(0),
      _remoteTransportSocketReceiveBufferSize(MAX_TCP_RECV), _totalBytesSent(0),
      _blockingProcessQueue(false), _senderThread(0),
      _writeMode(PROCESS_SEND_QUEUE),
      _writeOpReady(false),_lowLatency(false),
      _socketBuffer(receiveBuffer),
      _sendBuffer(sendBuffer),
      //PRIVATE
      _storedPayloadSize(0), _storedPosition(0), _startPosition(0), 
      _maxSendPayloadSize(0), 
      _lastMessageStartPosition(std::numeric_limits<size_t>::max()),_lastSegmentedMessageType(0),
      _lastSegmentedMessageCommand(0), _nextMessagePayloadOffset(0), 
      _byteOrderFlag(EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00),
      _socketSendBufferSize(0)
    {
      if (receiveBuffer->getSize() < 2*MAX_ENSURE_SIZE)
        throw std::invalid_argument(
        "receiveBuffer.capacity() < 2*MAX_ENSURE_SIZE");

      // require aligned buffer size 
      //(not condition, but simplifies alignment code)

      if (receiveBuffer->getSize() % PVA_ALIGNMENT != 0)
        throw std::invalid_argument(
        "receiveBuffer.capacity() % PVAConstants.PVA_ALIGNMENT != 0");

      if (sendBuffer->getSize() < 2*MAX_ENSURE_SIZE)
        throw std::invalid_argument("sendBuffer() < 2*MAX_ENSURE_SIZE");

      // require aligned buffer size 
      //(not condition, but simplifies alignment code)
      if (sendBuffer->getSize() % PVA_ALIGNMENT != 0)
        throw std::invalid_argument(
        "sendBuffer() % PVAConstants.PVA_ALIGNMENT != 0");

      // initialize to be empty
      _socketBuffer->setPosition(_socketBuffer->getLimit());
      _startPosition = _socketBuffer->getPosition();

      // clear send
      _sendBuffer->clear();

      // start msg + control
      _maxSendPayloadSize = 
        _sendBuffer->getSize() - 2*PVA_MESSAGE_HEADER_SIZE;	
      _socketSendBufferSize = socketSendBufferSize;
      _blockingProcessQueue = blockingProcessQueue;
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

      // magic code
      int8_t magicCode = _socketBuffer->getByte();

      // version
      _version = _socketBuffer->getByte(); 

      // flags
      _flags = _socketBuffer->getByte();

      // command
      _command = _socketBuffer->getByte();

      // read payload size
      _payloadSize = _socketBuffer->getInt();

      // check magic code
      if (magicCode != PVA_MAGIC)
      {
        LOG(logLevelError, 
          "Invalid header received from the client at %s:%d: %s,"
          " disconnecting...", 
          __FILE__, __LINE__, inetAddressToString(*getLastReadBufferSocketAddress()).c_str());
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

              LOG(logLevelWarn, 
                "Not-a-frst segmented message received in normal mode"
                " from the client at %s:%d: %s, disconnecting...",
                  __FILE__, __LINE__, inetAddressToString(*getLastReadBufferSocketAddress()).c_str());
              invalidDataStreamHandler();
              throw invalid_data_stream_exception(
                "not-a-first segmented message received in normal mode");
            }

            _storedPayloadSize = _payloadSize;
            _storedPosition = _socketBuffer->getPosition();
            _storedLimit = _socketBuffer->getLimit();
            _socketBuffer->setLimit(std::min<std::size_t>
              (_storedPosition + _storedPayloadSize, _storedLimit));
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
          std::size_t newPosition =
            alignedValue(
            _storedPosition + _storedPayloadSize, PVA_ALIGNMENT);

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
              newPosition - _socketBuffer->getPosition();

            if (bytesNotRead < PVA_ALIGNMENT)
            {
              // make alignment bytes as real payload to enable SPLIT
              // no end-of-socket or segmented scenario can happen
              // due to aligned buffer size
              _storedPayloadSize += bytesNotRead;
              // reveal currently existing padding
              _socketBuffer->setLimit(_storedLimit);
              ensureData(bytesNotRead);
              _storedPayloadSize -= bytesNotRead;
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
          _socketBuffer->setLimit(_storedLimit);
          _socketBuffer->setPosition(newPosition);
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
              "Not-a-first segmented message expected from the client at"
              " %s:%d: %s, disconnecting...",
              __FILE__, __LINE__, inetAddressToString(*getLastReadBufferSocketAddress()).c_str());
            invalidDataStreamHandler();
            throw new invalid_data_stream_exception(
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
        std::size_t remainingBytes = _socketBuffer->getRemaining();
        if (remainingBytes >= requiredBytes) {
          return true;
        }

        // assumption: remainingBytes < MAX_ENSURE_DATA_BUFFER_SIZE &&
        //			   requiredBytes < (socketBuffer.capacity() - PVA_ALIGNMENT)

        //
        // copy unread part to the beginning of the buffer
        // to make room for new data (as much as we can read) 
        // NOTE: requiredBytes is expected to be small (order of 10 bytes) 
        //

        // a new start position, we are careful to preserve alignment
        _startPosition = 
          MAX_ENSURE_SIZE + _socketBuffer->getPosition() % PVA_ALIGNMENT;

        std::size_t endPosition = _startPosition + remainingBytes;

        for (std::size_t i = _startPosition; i < endPosition; i++)
          _socketBuffer->putByte(i, _socketBuffer->getByte());

        // update buffer to the new position
        _socketBuffer->setLimit(_socketBuffer->getSize());
        _socketBuffer->setPosition(endPosition);

        // read at least requiredBytes bytes
        std::size_t requiredPosition = _startPosition + requiredBytes;
        while (_socketBuffer->getPosition() < requiredPosition)
        {
          int bytesRead = read(_socketBuffer.get());

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
              _socketBuffer->setLimit(_socketBuffer->getPosition());
              _socketBuffer->setPosition(_startPosition);

              return false;
            }
          }
        }

        // set pointers (aka flip)
        _socketBuffer->setLimit(_socketBuffer->getPosition());
        _socketBuffer->setPosition(_startPosition);

        return true;
    }


    void AbstractCodec::ensureData(std::size_t size) {

      // enough of data?
      if (_socketBuffer->getRemaining() >= size)
        return;

      // to large for buffer...
      if (size > MAX_ENSURE_DATA_SIZE)	{// half for SPLIT, half for SEGMENTED
        std::ostringstream msg;
        msg << "requested for buffer size " << size 
          << ", but maximum " << MAX_ENSURE_DATA_SIZE << " is allowed.";       
        LOG(logLevelWarn, 
          "%s at %s:%d,", msg.str().c_str(), __FILE__, __LINE__);  
        std::string s = msg.str();
        throw std::invalid_argument(s);
      }

      try
      {

        // subtract what was already processed
        std::size_t pos = _socketBuffer->getPosition();
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
          ReadMode storedMode = _readMode; _readMode = SPLIT;
          readToBuffer(size, true);
          _readMode = storedMode;
          _storedPosition = _socketBuffer->getPosition();
          _storedLimit = _socketBuffer->getLimit();
          _socketBuffer->setLimit(
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
          std::size_t remainingBytes = _socketBuffer->getRemaining();
          for (std::size_t i = 0; i < remainingBytes; i++)
            _socketBuffer->putByte(i, _socketBuffer->getByte());

          // restore limit (there might be some data already present
          //and readToBuffer needs to know real limit)
          _socketBuffer->setLimit(_storedLimit);

          // remember alignment offset of end of the message (to be restored)
          std::size_t storedAlignmentOffset =
            _socketBuffer->getPosition() % PVA_ALIGNMENT;

          // skip post-message alignment bytes
          if (storedAlignmentOffset > 0)
          {
            std::size_t toSkip = PVA_ALIGNMENT - storedAlignmentOffset;
            readToBuffer(toSkip, true);
            std::size_t currentPos = _socketBuffer->getPosition();
            _socketBuffer->setPosition(currentPos + toSkip);
          }

          // we expect segmented message, we expect header
          // that (and maybe some control packets) needs to be "removed"
          // so that we get combined payload
          ReadMode storedMode = _readMode; _readMode = SEGMENTED;
          processRead();
          _readMode = storedMode;

          // make sure we have all the data (maybe we run into SPLIT)
          readToBuffer(size - remainingBytes + storedAlignmentOffset, true);

          // skip storedAlignmentOffset bytes (sender should padded start of
          //segmented message)
          // SPLIT cannot mess with this, since start of the message,
          //i.e. current position, is always aligned 
          _socketBuffer->setPosition(
            _socketBuffer->getPosition() + storedAlignmentOffset);

          // copy before position (i.e. start of the payload)
          for (int32_t i = remainingBytes - 1,
            j = _socketBuffer->getPosition() - 1; i >= 0; i--, j--)
            _socketBuffer->putByte(j, _socketBuffer->getByte(i));

          _startPosition = _socketBuffer->getPosition() - remainingBytes;
          _socketBuffer->setPosition(_startPosition);

          _storedPayloadSize += remainingBytes - storedAlignmentOffset;
          _storedPosition = _startPosition;
          _storedLimit = _socketBuffer->getLimit();
          _socketBuffer->setLimit(
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
      std::size_t pos = _socketBuffer->getPosition();
      std::size_t newpos = (pos + k) & (~k);
      if (pos == newpos)
        return;

      std::size_t diff = _socketBuffer->getLimit() - newpos;
      if (diff > 0)
      {
        _socketBuffer->setPosition(newpos);
        return;
      }

      ensureData(diff);

      // position has changed, recalculate
      newpos = (_socketBuffer->getPosition() + k) & (~k);
      _socketBuffer->setPosition(newpos);
    }


    void AbstractCodec::alignBuffer(std::size_t alignment) {

      std::size_t k = (alignment - 1);
      std::size_t pos = _sendBuffer->getPosition();
      std::size_t newpos = (pos + k) & (~k);
      if (pos == newpos)
        return;

      // there is always enough of space
      // since sendBuffer capacity % PVA_ALIGNMENT == 0
      _sendBuffer->setPosition(newpos);
    }


    void AbstractCodec::startMessage(
      epics::pvData::int8 command, 
      std::size_t ensureCapacity) {

        _lastMessageStartPosition = 
          std::numeric_limits<size_t>::max();		// TODO revise this
        ensureBuffer(
          PVA_MESSAGE_HEADER_SIZE + ensureCapacity + _nextMessagePayloadOffset);
        _lastMessageStartPosition = _sendBuffer->getPosition();
        _sendBuffer->putByte(PVA_MAGIC);
        _sendBuffer->putByte(PVA_VERSION);
        _sendBuffer->putByte(
          (_lastSegmentedMessageType | _byteOrderFlag));	// data + endian
        _sendBuffer->putByte(command);	// command
        _sendBuffer->putInt(0);		// temporary zero payload

        // apply offset
        if (_nextMessagePayloadOffset > 0)
          _sendBuffer->setPosition(
          _sendBuffer->getPosition() + _nextMessagePayloadOffset);
    }


    void AbstractCodec::putControlMessage(
      epics::pvData::int8 command,  
      epics::pvData::int32 data) {

        _lastMessageStartPosition = 
          std::numeric_limits<size_t>::max();		// TODO revise this
        ensureBuffer(PVA_MESSAGE_HEADER_SIZE);
        _sendBuffer->putByte(PVA_MAGIC);
        _sendBuffer->putByte(PVA_VERSION);
        _sendBuffer->putByte((0x01 | _byteOrderFlag));	// control + endian
        _sendBuffer->putByte(command);	// command
        _sendBuffer->putInt(data);		// data
    }


    void AbstractCodec::endMessage() {
      endMessage(false);
    }


    void AbstractCodec::endMessage(bool hasMoreSegments) {

      if (_lastMessageStartPosition != std::numeric_limits<size_t>::max())
      {
        std::size_t lastPayloadBytePosition = _sendBuffer->getPosition();

        // align
        alignBuffer(PVA_ALIGNMENT);

        // set paylaod size (non-aligned)
        std::size_t payloadSize = 
          lastPayloadBytePosition - 
          _lastMessageStartPosition - PVA_MESSAGE_HEADER_SIZE;

        _sendBuffer->putInt(_lastMessageStartPosition + 4, payloadSize); 

        // set segmented bit
        if (hasMoreSegments) {
          // first segment
          if (_lastSegmentedMessageType == 0)
          {
            std::size_t flagsPosition = _lastMessageStartPosition + 2;
            epics::pvData::int8 type = _sendBuffer->getByte(flagsPosition);
            // set first segment bit
            _sendBuffer->putByte(flagsPosition, (type | 0x10));
            // first + last segment bit == in-between segment
            _lastSegmentedMessageType = type | 0x30;
            _lastSegmentedMessageCommand = 
              _sendBuffer->getByte(flagsPosition + 1);
          }
          _nextMessagePayloadOffset = lastPayloadBytePosition % PVA_ALIGNMENT;
        }
        else
        {
          // last segment
          if (_lastSegmentedMessageType != 
            std::numeric_limits<size_t>::max())
          {
            std::size_t flagsPosition = _lastMessageStartPosition + 2;
            // set last segment bit (by clearing first segment bit)
            _sendBuffer->putByte(flagsPosition, 
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

      if (_sendBuffer->getRemaining() >= size)
        return;

      // too large for buffer...
      if (_maxSendPayloadSize < size) {
        std::ostringstream msg;
        msg << "requested for buffer size " << 
          size << ", but only " << _maxSendPayloadSize << " available.";
        std::string s = msg.str();
        LOG(logLevelWarn, 
          "%s at %s:%d,", msg.str().c_str(), __FILE__, __LINE__);  
        throw std::invalid_argument(s);
      }

      while (_sendBuffer->getRemaining() < size)
        flush(false);
    }

    // assumes startMessage was called (or header is in place), because endMessage(true) is later called that peeks and sets _lastSegmentedMessageType
    void AbstractCodec::flushSerializeBuffer() {
      flush(false);
    }


    void AbstractCodec::flush(bool lastMessageCompleted) {

      // automatic end
      endMessage(!lastMessageCompleted);

      _sendBuffer->flip();

      try {
        send(_sendBuffer.get());
      } catch (io_exception &) {
        try {
          if (isOpen())
            close();
        } catch (io_exception &) {
          // noop, best-effort close
        }
        throw connection_closed_exception("Failed to send buffer.");
      }

      _sendBuffer->clear();

      _lastMessageStartPosition = std::numeric_limits<size_t>::max();

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
      // data. See Microsoft KB article KB823764.
      // We do it also for other systems just to be safe.
      std::size_t maxBytesToSend = 
        std::min<int32_t>(
        _socketSendBufferSize, _remoteTransportSocketReceiveBufferSize) / 2;

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

        /*
        if (IS_LOGGABLE(logLevelTrace)) {
          hexDump(std::string("AbstractCodec::send WRITE"),
            (const int8 *)buffer->getArray(), 
            buffer->getPosition(), buffer->getRemaining());
        }
        */

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

        _totalBytesSent += bytesSent;

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
          TransportSender::shared_pointer sender = _sendQueue.take(-1);
          if (sender.get() == 0)
          {
            // flush
            if (_sendBuffer->getPosition() > 0)
              flush(true);

            sendCompleted();	// do not schedule sending

            if (_blockingProcessQueue) {
              if (terminated())			// termination
                break;
              sender = _sendQueue.take(0);
              // termination (we want to process even if shutdown)
              if (sender.get() == 0)		
                break;
            }
            else
              return;
          }

          processSender(sender);
        }
      }

      // flush
      if (_sendBuffer->getPosition() > 0)
        flush(true);
    }


    void AbstractCodec::clearSendQueue()
    {
      _sendQueue.clean();
    }


    void AbstractCodec::enqueueSendRequest(
      TransportSender::shared_pointer const & sender) {
        _sendQueue.put(sender);
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
        _lastMessageStartPosition = _sendBuffer->getPosition();

        sender->send(_sendBuffer.get(), this);

        // automatic end (to set payload size)
        endMessage(false);
      }
      catch (std::exception &e ) {

        std::ostringstream msg;
        msg << "an exception caught while processing a send message: " 
          << e.what();       
        LOG(logLevelDebug, "%s at %s:%d",
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
          _sendBuffer->getRemaining() >= requiredBufferSize)
        {
          processSender(sender);
          if (_sendBuffer->getPosition() > 0)
          {
            if (_lowLatency)
              flush(true);
            else
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
      _socketBuffer->setEndianess(byteOrder);
      // TODO sync
      _sendBuffer->setEndianess(byteOrder);
      _byteOrderFlag = EPICS_ENDIAN_BIG == byteOrder ? 0x80 : 0x00;
    }



    // 
    //
    //  BlockingAbstractCodec
    //
    //
    //

    void BlockingAbstractCodec::readPollOne() {
      throw std::logic_error("should not be called for blocking IO");
    }


    void BlockingAbstractCodec::writePollOne() {
      throw std::logic_error("should not be called for blocking IO");
    }


    void BlockingAbstractCodec::close() {

      if (_isOpen.getAndSet(false))
      {
        // always close in the same thread, same way, etc.
        // wakeup processSendQueue

        // clean resources
        internalClose(true);

        _sendQueue.wakeup();

        // post close
        internalPostClose(true);
      }
    }

    void BlockingAbstractCodec::internalClose(bool /*force*/) {
    }

    void BlockingAbstractCodec::internalPostClose(bool /*force*/) {
    }

    bool BlockingAbstractCodec::terminated() {
      return !isOpen();
    }


    bool BlockingAbstractCodec::isOpen() {
      return _isOpen.get();
    }


    // NOTE: must not be called from constructor (e.g. needs shared_from_this())
    void BlockingAbstractCodec::start() {

      _readThread = epicsThreadCreate(
        "BlockingAbstractCodec-readThread",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(
        epicsThreadStackMedium),
        BlockingAbstractCodec::receiveThread,
        this);

      _sendThread = epicsThreadCreate(
        "BlockingAbstractCodec-_sendThread",
        epicsThreadPriorityMedium,
        epicsThreadGetStackSize(
        epicsThreadStackMedium),
        BlockingAbstractCodec::sendThread,
        this);

    }


    void BlockingAbstractCodec::receiveThread(void *param) 
    {

      BlockingAbstractCodec *bac = static_cast<BlockingAbstractCodec *>(param);
      Transport::shared_pointer ptr = bac->shared_from_this();

      while (bac->isOpen())
      {
        try {
          bac->processRead();
        } catch (connection_closed_exception &cce) {
            // noop
        } catch (std::exception &e) {
          LOG(logLevelWarn, 
            "an exception caught while in sendThread at %s:%d: %s",
            __FILE__, __LINE__, e.what());  
        } catch (...) {
          LOG(logLevelError, 
            "unknown exception caught while in sendThread at %s:%d",
            __FILE__, __LINE__);  
        }
      }

      bac->_shutdownEvent.signal();
    }


    void BlockingAbstractCodec::sendThread(void *param)
    {

        BlockingAbstractCodec *bac = static_cast<BlockingAbstractCodec *>(param);
      Transport::shared_pointer ptr = bac->shared_from_this();

      bac->setSenderThread();

      while (bac->isOpen())
      {
        try {
          bac->processWrite();
        } catch (connection_closed_exception &cce) {
            // noop
        } catch (std::exception &e) {
          LOG(logLevelWarn, 
            "an exception caught while in sendThread at %s:%d: %s",
            __FILE__, __LINE__, e.what());  
        } catch (...) {
          LOG(logLevelError, 
            "unknown exception caught while in sendThread at %s:%d",
            __FILE__, __LINE__);  
        }
      }

      // wait read thread to die
      bac->_shutdownEvent.wait();

      // call internal destroy
      bac->internalDestroy();

    }


    void BlockingAbstractCodec::sendBufferFull(int tries) {
      // TODO constants
      epicsThreadSleep(std::max<double>(tries * 0.1, 1));
    }


    // 
    //
    //  BlockingSocketAbstractCodec
    //
    //
    //


    BlockingSocketAbstractCodec::BlockingSocketAbstractCodec(
      SOCKET channel,
      int32_t sendBufferSize,
      int32_t receiveBufferSize): 
    BlockingAbstractCodec(
      std::tr1::shared_ptr<epics::pvData::ByteBuffer>(new ByteBuffer((std::max<std::size_t>((std::size_t)(
      MAX_TCP_RECV + MAX_ENSURE_DATA_BUFFER_SIZE), receiveBufferSize) + 
      (PVA_ALIGNMENT - 1)) & (~(PVA_ALIGNMENT - 1)))), 
      std::tr1::shared_ptr<epics::pvData::ByteBuffer>(new ByteBuffer((std::max<std::size_t>((std::size_t)( MAX_TCP_RECV +
      MAX_ENSURE_DATA_BUFFER_SIZE), receiveBufferSize) + (PVA_ALIGNMENT - 1))
      & (~(PVA_ALIGNMENT - 1)))), sendBufferSize), 
      _channel(channel) 
    {
      // get remote address
      osiSocklen_t saSize = sizeof(sockaddr);
      int retval = getpeername(_channel, &(_socketAddress.sa), &saSize);
      if(unlikely(retval<0)) {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        LOG(logLevelError,
          "Error fetching socket remote address: %s",
          errStr);
      }

      // set receive timeout so that we do not have problems at 
      //shutdown (recvfrom would block)
      struct timeval timeout;
      memset(&timeout, 0, sizeof(struct timeval));
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      // TODO remove this and implement use epicsSocketSystemCallInterruptMechanismQuery
      if (unlikely(::setsockopt (_channel, SOL_SOCKET, SO_RCVTIMEO, 
        (char*)&timeout, sizeof(timeout)) < 0))
      {
        char errStr[64];
        epicsSocketConvertErrnoToString(errStr, sizeof(errStr));
        LOG(logLevelError,
          "Failed to set SO_RCVTIMEO for TDP socket %s: %s.",
          inetAddressToString(_socketAddress).c_str(), errStr);
      }

    }

    // must be called only once, when there will be no operation on socket (e.g. just before tx/rx thread exists)
    void BlockingSocketAbstractCodec::internalDestroy() {

      if(_channel != INVALID_SOCKET) {
        epicsSocketDestroy(_channel);
        _channel = INVALID_SOCKET;
      }

    }


    void BlockingSocketAbstractCodec::invalidDataStreamHandler() {
      close();
    }


    int BlockingSocketAbstractCodec::write(
      epics::pvData::ByteBuffer *src) {

        std::size_t remaining;
        while((remaining=src->getRemaining()) > 0) {

          int bytesSent = ::send(_channel,
            &src->getArray()[src->getPosition()],
            remaining, 0);

          // NOTE: do not log here, you might override SOCKERRNO relevant to recv() operation above

          if(unlikely(bytesSent<0)) {

            int socketError = SOCKERRNO;

            // spurious EINTR check                     
            if (socketError==SOCK_EINTR)
              continue;
          }

          if (bytesSent > 0) {
            src->setPosition(src->getPosition() + bytesSent);
          }

          return bytesSent;

        }

        return 0;
    }


    std::size_t BlockingSocketAbstractCodec::getSocketReceiveBufferSize() 
      const  {

      osiSocklen_t intLen = sizeof(int);
      int socketRecvBufferSize;
      int retval = getsockopt(_channel, SOL_SOCKET, SO_RCVBUF, 
        (char *)&socketRecvBufferSize, &intLen);
      
      if(retval<0) {
          if (IS_LOGGABLE(logLevelDebug))
          {
            char strBuffer[64];
            epicsSocketConvertErrnoToString(strBuffer, sizeof(strBuffer));
            LOG(logLevelDebug, "Error getting SO_SNDBUF: %s", strBuffer);
          }
      }

      return socketRecvBufferSize;
    }


    int BlockingSocketAbstractCodec::read(epics::pvData::ByteBuffer* dst) {

      std::size_t remaining;
      while((remaining=dst->getRemaining()) > 0) {

        // read
        std::size_t pos = dst->getPosition();
        
        int bytesRead = recv(_channel, 
          (char*)(dst->getArray()+pos), remaining, 0);

        // NOTE: do not log here, you might override SOCKERRNO relevant to recv() operation above

        /*
        if (IS_LOGGABLE(logLevelTrace)) {
          hexDump(std::string("READ"), 
            (const int8 *)(dst->getArray()+pos), bytesRead);
        }
        */

        if(unlikely(bytesRead<=0)) {

          if (bytesRead<0)
          {
            int socketError = SOCKERRNO;

            // interrupted or timeout
            if (socketError == EINTR || 
              socketError == EAGAIN ||
              socketError == EWOULDBLOCK)
              continue;
          }

          return -1;    // 0 means connection loss for blocking transport, notify codec by returning -1
        }
        
        dst->setPosition(dst->getPosition() + bytesRead);
        return bytesRead;
      }

      return 0;
    }


    BlockingServerTCPTransportCodec::BlockingServerTCPTransportCodec(
      Context::shared_pointer const & context, 
      SOCKET channel,
      std::auto_ptr<ResponseHandler>& responseHandler, 
      int32_t sendBufferSize, 
      int32_t receiveBufferSize) :
    BlockingTCPTransportCodec(context, channel, responseHandler, 
      sendBufferSize, receiveBufferSize, PVA_DEFAULT_PRIORITY),
      _lastChannelSID(0)
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


    int BlockingServerTCPTransportCodec::getChannelCount() {

      Lock lock(_channelsMutex);
      return static_cast<int>(_channels.size());
    }


    void BlockingServerTCPTransportCodec::send(ByteBuffer* buffer,
      TransportSendControl* control) {

        //
        // set byte order control message 
        //

        ensureBuffer(PVA_MESSAGE_HEADER_SIZE);
        buffer->putByte(PVA_MAGIC);
        buffer->putByte(PVA_VERSION);
        buffer->putByte(
          0x01 | ((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG)
          ? 0x80 : 0x00));		// control + big endian
        buffer->putByte(2);		// set byte order
        buffer->putInt(0);		


        //
        // send verification message
        //
        control->startMessage(CMD_CONNECTION_VALIDATION, 2*sizeof(int32));

        // receive buffer size
        buffer->putInt(static_cast<int32>(getReceiveBufferSize()));

        // socket receive buffer size
        buffer->putInt(static_cast<int32>(getSocketReceiveBufferSize()));

        // send immediately
        control->flush(true);
  }
  
  void BlockingServerTCPTransportCodec::destroyAllChannels() {
    Lock lock(_channelsMutex);
    if(_channels.size()==0) return;

    if (IS_LOGGABLE(logLevelDebug))
    {
        char ipAddrStr[64];
        ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
        LOG(
          logLevelDebug,
          "Transport to %s still has %zd channel(s) active and closing...",
          ipAddrStr, _channels.size());
    }

    std::map<pvAccessID, ServerChannel::shared_pointer>::iterator it = _channels.begin();
    for(; it!=_channels.end(); it++)
        it->second->destroy();

    _channels.clear();
  }

  void BlockingServerTCPTransportCodec::internalClose(bool force) {
    Transport::shared_pointer thisSharedPtr = shared_from_this();
    BlockingTCPTransportCodec::internalClose(force);
    destroyAllChannels();
  }






  
  BlockingClientTCPTransportCodec::BlockingClientTCPTransportCodec(
    Context::shared_pointer const & context,
    SOCKET channel,
    std::auto_ptr<ResponseHandler>& responseHandler,
    int32_t sendBufferSize, 
    int32_t receiveBufferSize,
    TransportClient::shared_pointer const & client,
    epics::pvData::int8 /*remoteTransportRevision*/,
    float beaconInterval,
    int16_t priority ) :
    BlockingTCPTransportCodec(context, channel, responseHandler, 
      sendBufferSize, receiveBufferSize, priority),
    _connectionTimeout(beaconInterval*1000),
    _unresponsiveTransport(false),
    _verifyOrEcho(true),
    _verified(false)
  {
    // initialize owners list, send queue
    acquire(client);

    // use immediate for clients
    //setFlushStrategy(DELAYED);

    // setup connection timeout timer (watchdog) - moved to start() method
    epicsTimeGetCurrent(&_aliveTimestamp);
  }

  void BlockingClientTCPTransportCodec::start()
  {
    TimerCallbackPtr tcb = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
    _context->getTimer()->schedulePeriodic(tcb, _connectionTimeout, _connectionTimeout);
    BlockingTCPTransportCodec::start();
  }
        
  BlockingClientTCPTransportCodec::~BlockingClientTCPTransportCodec() {
  }
    
 
 
 
 
 
 
 
 
        void BlockingClientTCPTransportCodec::callback() {
            epicsTimeStamp currentTime;
            epicsTimeGetCurrent(&currentTime);

            _mutex.lock();
            // no exception expected here
            double diff = epicsTimeDiffInSeconds(&currentTime, &_aliveTimestamp);
            _mutex.unlock();
            
            if(diff>2*_connectionTimeout) {
                unresponsiveTransport();
            }
            // use some k (3/4) to handle "jitter"
            else if(diff>=((3*_connectionTimeout)/4)) {
                // send echo
                TransportSender::shared_pointer transportSender = std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
                enqueueSendRequest(transportSender);
            }
        }

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from code at %s:%d.", __FILE__, __LINE__); }

        void BlockingClientTCPTransportCodec::unresponsiveTransport() {
            Lock lock(_mutex);
            if(!_unresponsiveTransport) {
                _unresponsiveTransport = true;

                TransportClientMap_t::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient::shared_pointer client = it->second.lock();
                    if (client)
                    {
                        EXCEPTION_GUARD(client->transportUnresponsive());
                    }
                }
            }
        }

        bool BlockingClientTCPTransportCodec::acquire(TransportClient::shared_pointer const & client) {
            Lock lock(_mutex);
            if(isClosed()) return false;
            
            if (IS_LOGGABLE(logLevelDebug))
            {
                char ipAddrStr[48];
                ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
                LOG(logLevelDebug, "Acquiring transport to %s.", ipAddrStr);
            }

            _owners[client->getID()] = TransportClient::weak_pointer(client);
            //_owners.insert(TransportClient::weak_pointer(client));

            return true;
        }

        // _mutex is held when this method is called
        void BlockingClientTCPTransportCodec::internalClose(bool forced) {
            BlockingTCPTransportCodec::internalClose(forced);

            TimerCallbackPtr tcb = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
            _context->getTimer()->cancel(tcb);
        }

        void BlockingClientTCPTransportCodec::internalPostClose(bool forced) {
            BlockingTCPTransportCodec::internalPostClose(forced);

            // _owners cannot change when transport is closed
            closedNotifyClients();
        }

        /**
         * Notifies clients about disconnect.
         */
        void BlockingClientTCPTransportCodec::closedNotifyClients() {

            // check if still acquired
            size_t refs = _owners.size();
            if(refs>0) {

                if (IS_LOGGABLE(logLevelDebug))
                {
                    char ipAddrStr[48];
                    ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
                    LOG(
                            logLevelDebug,
                            "Transport to %s still has %d client(s) active and closing...",
                            ipAddrStr, refs);
                }

                TransportClientMap_t::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient::shared_pointer client = it->second.lock();
                    if (client)
                    {
                        EXCEPTION_GUARD(client->transportClosed());
                    }
                }
                
            }

            _owners.clear();
        }

        //void BlockingClientTCPTransportCodec::release(TransportClient::shared_pointer const & client) {
        void BlockingClientTCPTransportCodec::release(pvAccessID clientID) {
            Lock lock(_mutex);
            if(isClosed()) return;
            
            if (IS_LOGGABLE(logLevelDebug))
            {
                char ipAddrStr[48];
                ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
                LOG(logLevelDebug, "Releasing transport to %s.", ipAddrStr);
            }

            _owners.erase(clientID);
            //_owners.erase(TransportClient::weak_pointer(client));
            
            // not used anymore, close it
            // TODO consider delayed destruction (can improve performance!!!)
            if(_owners.size()==0) close();		// TODO close(false)
        }

        void BlockingClientTCPTransportCodec::aliveNotification() {
            Lock guard(_mutex);
            epicsTimeGetCurrent(&_aliveTimestamp);
            if(_unresponsiveTransport) responsiveTransport();
        }

            bool BlockingClientTCPTransportCodec::verify(epics::pvData::int32 timeoutMs) {
                return _verifiedEvent.wait(timeoutMs/1000.0);
            }

            void BlockingClientTCPTransportCodec::verified() {
                epics::pvData::Lock lock(_verifiedMutex);
                _verified = true;
                _verifiedEvent.signal();
            }

        void BlockingClientTCPTransportCodec::responsiveTransport() {
            Lock lock(_mutex);
            if(_unresponsiveTransport) {
                _unresponsiveTransport = false;

                Transport::shared_pointer thisSharedPtr = shared_from_this();
                TransportClientMap_t::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient::shared_pointer client = it->second.lock();
                    if (client)
                    {
                        EXCEPTION_GUARD(client->transportResponsive(thisSharedPtr));
                    }
                }
            }
        }

        void BlockingClientTCPTransportCodec::changedTransport() {
        	_outgoingIR.reset();

            Lock lock(_mutex);
            TransportClientMap_t::iterator it = _owners.begin();
            for(; it!=_owners.end(); it++) {
                TransportClient::shared_pointer client = it->second.lock();
                if (client)
                {
                    EXCEPTION_GUARD(client->transportChanged());
                }
            }
        }

        void BlockingClientTCPTransportCodec::send(ByteBuffer* buffer,
                TransportSendControl* control) {
            if(_verifyOrEcho) {
                /*
                 * send verification response message
                 */

                control->startMessage(CMD_CONNECTION_VALIDATION, 2*sizeof(int32)+sizeof(int16));

                // receive buffer size
                buffer->putInt(static_cast<int32>(getReceiveBufferSize()));

                // socket receive buffer size
                buffer->putInt(static_cast<int32>(getSocketReceiveBufferSize()));

                // connection priority
                buffer->putShort(getPriority());

                // send immediately
                control->flush(true);

                _verifyOrEcho = false;
            }
            else {
                control->startMessage(CMD_ECHO, 0);
                // send immediately
                control->flush(true);
            }

        }
 
 
    }
  }
}
