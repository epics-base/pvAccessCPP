/**
* Copyright - See the COPYRIGHT that is included with this distribution.
* pvAccessCPP is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
*/

#ifndef CODEC_H_
#define CODEC_H_

#include <set>
#include <map>
#include <deque>

#ifdef epicsExportSharedSymbols
#   define abstractCodecEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <shareLib.h>
#include <osdSock.h>
#include <osiSock.h>
#include <epicsTime.h>
#include <epicsThread.h>

#include <pv/byteBuffer.h>
#include <pv/pvType.h>
#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/event.h>
#include <pv/likely.h>
#include <pv/logger.h>

#ifdef abstractCodecEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef abstractCodecEpicsExportSharedSymbols
#endif

#include <pv/pvaConstants.h>
#include <pv/remote.h>
#include <pv/transportRegistry.h>
#include <pv/introspectionRegistry.h>
#include <pv/namedLockPattern.h>
#include <pv/inetAddressUtil.h>

namespace epics {
  namespace pvAccess {

    // TODO replace mutex with atomic (CAS) operations
    template<typename T> 
    class AtomicValue
    {
    public:
      AtomicValue(): _value(0) {};

      T getAndSet(T value) 
      { 
        mutex.lock(); 
        T tmp = _value; _value = value; 
        mutex.unlock(); 
        return tmp; 
      }

      T get() { mutex.lock(); T tmp = _value; mutex.unlock(); return tmp; }

    private:
      T _value;
      epics::pvData::Mutex mutex;
    };


    template<typename T> 
    class queue {
    public:

      queue(void) { }
      //TODO 
      /*queue(queue const &T) = delete;
      queue(queue &&T) = delete;
      queue& operator=(const queue &T) = delete;
      */
      ~queue(void) 
      {    
        LOG(logLevelTrace, 
          "queue::~queue DESTROY  (threadId: %u)", epicsThreadGetIdSelf());
      }


      bool empty(void) 
      { 
        LOG(logLevelTrace, 
          "queue::empty enter:  (threadId: %u)", epicsThreadGetIdSelf());
        epics::pvData::Lock lock(_queueMutex);
        return _queue.empty();
      }

      void clean()
      { 
        LOG(logLevelTrace, "queue::clean enter:  (threadId: %u)", 
          epicsThreadGetIdSelf());

        epics::pvData::Lock lock(_queueMutex);
        _queue.clear();
      }


      void wakeup() 
      { 

        LOG(logLevelTrace, "queue::wakeup enter:  (threadId: %u)", 
          epicsThreadGetIdSelf());

        if (!_wakeup.getAndSet(true))
        {
          LOG(logLevelTrace, 
            "queue::wakeup signaling on _queueEvent:  (threadId: %u)", 
            epicsThreadGetIdSelf());
          _queueEvent.signal();
        }
      }


      void put(T const & elem) 
      { 
        LOG(logLevelTrace, 
          "queue::put enter  (threadId: %u)", epicsThreadGetIdSelf());

        {
          epics::pvData::Lock lock(_queueMutex);
          _queue.push_back(elem);
        }

        _queueEvent.signal();
      }


      T take(int timeOut) 
      { 

        LOG(logLevelTrace, 
          "queue::take enter timeOut:%d  (threadId: %u)", 
          timeOut, epicsThreadGetIdSelf());

        while (true)
        {

          bool isEmpty = empty();

          if (isEmpty)
          {

            if (timeOut < 0) {
              epics::pvAccess::LOG(logLevelTrace, 
                "queue::take exit timeOut:%d  (threadId: %u)", 
                timeOut, epicsThreadGetIdSelf());

              return T();
            }

            while (isEmpty)
            {

              if (timeOut == 0) {
                
                LOG(logLevelTrace, 
                  "queue::take going to wait timeOut:%d  (threadId: %u)", 
                  timeOut, epicsThreadGetIdSelf());

                _queueEvent.wait();
              }
              else {
                
                LOG(logLevelTrace, 
                  "queue::take going to wait timeOut:%d  (threadId: %u)", 
                  timeOut, epicsThreadGetIdSelf());

                _queueEvent.wait(timeOut);
              }

              LOG(logLevelTrace, 
                "queue::take waking up timeOut:%d  (threadId: %u)", 
                timeOut, epicsThreadGetIdSelf());

              isEmpty = empty();
              if (isEmpty)
              {
                if (timeOut > 0) {	// TODO spurious wakeup, but not critical
                  LOG(logLevelTrace, 
                    "queue::take exit after being woken up timeOut:%d"
                    "  (threadId: %u)", 
                    timeOut, epicsThreadGetIdSelf());
                  return T();
                }
                else // if (timeout == 0)	cannot be negative
                {
                  if (_wakeup.getAndSet(false)) {
                    
                    LOG(logLevelTrace, 
                      "queue::take exit after being woken up timeOut:%d"
                      "  (threadId: %u)", 
                      timeOut, epicsThreadGetIdSelf());

                    return T();
                  }
                }
              }
            }
          }
          else
          {
            
            LOG(logLevelTrace, 
              "queue::take obtaining lock for front element timeOut:%d"
              "  (threadId: %u)", 
              timeOut, epicsThreadGetIdSelf());

            epics::pvData::Lock lock(_queueMutex);
            T sender = _queue.front();
            _queue.pop_front();
            
            LOG(logLevelTrace, 
              "queue::take exit with sender timeOut:%d  (threadId: %u)", 
              timeOut, epicsThreadGetIdSelf());

            return sender;
          }
        }
      }

    private:

      std::deque<T> _queue;
      epics::pvData::Event _queueEvent;
      epics::pvData::Mutex _queueMutex;
      AtomicValue<bool> _wakeup;
      epics::pvData::Mutex _stdMutex;
    };


    class io_exception: public std::runtime_error {
    public:
      explicit io_exception(const std::string &s): std::runtime_error(s) {}
    };


    class invalid_data_stream_exception: public std::runtime_error {
    public:
      explicit invalid_data_stream_exception(
        const std::string &s): std::runtime_error(s) {}
    };


    class connection_closed_exception: public std::runtime_error {
    public:
      explicit connection_closed_exception(const std::string &s): std::runtime_error(s) {}
    };


    enum ReadMode { NORMAL, SPLIT, SEGMENTED };

    enum WriteMode { PROCESS_SEND_QUEUE, WAIT_FOR_READY_SIGNAL };


    class AbstractCodec :
      public TransportSendControl,
      public Transport
    {
    public:

      static const std::size_t MAX_MESSAGE_PROCESS;
      static const std::size_t MAX_MESSAGE_SEND;
      static const std::size_t MAX_ENSURE_SIZE;
      static const std::size_t MAX_ENSURE_DATA_SIZE;
      static const std::size_t MAX_ENSURE_BUFFER_SIZE;
      static const std::size_t MAX_ENSURE_DATA_BUFFER_SIZE;

      AbstractCodec(
        std::tr1::shared_ptr<epics::pvData::ByteBuffer> const & receiveBuffer,
        std::tr1::shared_ptr<epics::pvData::ByteBuffer> const & sendBuffer,
        int32_t socketSendBufferSize, 
        bool blockingProcessQueue); 

      virtual void processControlMessage() = 0;
      virtual void processApplicationMessage() = 0; 
      virtual osiSockAddr getLastReadBufferSocketAddress() = 0;
      virtual void invalidDataStreamHandler() = 0;
      virtual void readPollOne()=0; 
      virtual void writePollOne() = 0; 
      virtual void scheduleSend() = 0;
      virtual void sendCompleted() = 0;
      virtual bool terminated() = 0;
      virtual int write(epics::pvData::ByteBuffer* src) = 0;
      virtual int read(epics::pvData::ByteBuffer* dst) = 0;
      virtual bool isOpen() = 0;
      virtual void close() = 0;


      virtual ~AbstractCodec()
      { 
        LOG(logLevelTrace, 
          "AbstractCodec::~AbstractCodec DESTROY  (threadId: %u)", 
          epicsThreadGetIdSelf());
      }

      void alignBuffer(std::size_t alignment);
      void ensureData(std::size_t size);
      void alignData(std::size_t alignment);
      void startMessage(
        epics::pvData::int8 command, 
        std::size_t ensureCapacity);
      void putControlMessage(
        epics::pvData::int8 command,  
        epics::pvData::int32 data);
      void endMessage();
      void ensureBuffer(std::size_t size);
      void flushSerializeBuffer();
      void flush(bool lastMessageCompleted);
      void processWrite(); 
      void processRead(); 
      void processSendQueue();
      void clearSendQueue();
      void enqueueSendRequest(TransportSender::shared_pointer const & sender);
      void enqueueSendRequest(TransportSender::shared_pointer const & sender, 
        std::size_t requiredBufferSize);
      void setSenderThread();
      void setRecipient(osiSockAddr const & sendTo);
      void setByteOrder(int byteOrder);

      static std::size_t alignedValue(std::size_t value, std::size_t alignment);

    protected:

      virtual void sendBufferFull(int tries)  = 0; 
      void send(epics::pvData::ByteBuffer *buffer); 


      ReadMode _readMode;
      int8_t _version;
      int8_t _flags;
      int8_t _command;
      int32_t _payloadSize; // TODO why not size_t?
      epics::pvData::int32 _remoteTransportSocketReceiveBufferSize;
      int64_t _totalBytesSent;
      bool _blockingProcessQueue;
      //TODO initialize union
      osiSockAddr _sendTo;
      epicsThreadId _senderThread;
      WriteMode _writeMode;
      bool _writeOpReady;
      bool _lowLatency;

      std::tr1::shared_ptr<epics::pvData::ByteBuffer> _socketBuffer;
      std::tr1::shared_ptr<epics::pvData::ByteBuffer> _sendBuffer;

      epics::pvAccess::queue<TransportSender::shared_pointer> _sendQueue;

    private:

      void processHeader(); 
      void processReadNormal(); 
      void postProcessApplicationMessage();
      void processReadSegmented();
      bool readToBuffer(std::size_t requiredBytes, bool persistent);
      void endMessage(bool hasMoreSegments);
      void processSender(
        epics::pvAccess::TransportSender::shared_pointer const & sender);

      std::size_t _storedPayloadSize;
      std::size_t _storedPosition;
      std::size_t _storedLimit;
      std::size_t _startPosition;

      std::size_t _maxSendPayloadSize;
      std::size_t _lastMessageStartPosition;
      std::size_t _lastSegmentedMessageType;
      int8_t _lastSegmentedMessageCommand;
      std::size_t _nextMessagePayloadOffset;

      epics::pvData::int8 _byteOrderFlag;
      int32_t _socketSendBufferSize; 
    };


    class BlockingAbstractCodec: 
      public AbstractCodec,
      public std::tr1::enable_shared_from_this<BlockingAbstractCodec>
    {

    public: 

      POINTER_DEFINITIONS(BlockingAbstractCodec);

      BlockingAbstractCodec(
        std::tr1::shared_ptr<epics::pvData::ByteBuffer> const & receiveBuffer,
        std::tr1::shared_ptr<epics::pvData::ByteBuffer> const & sendBuffer,
        int32_t socketSendBufferSize): 
      AbstractCodec(receiveBuffer, sendBuffer, socketSendBufferSize, true), 
        _readThread(0), _sendThread(0) { _isOpen.getAndSet(true);}

      void readPollOne();
      void writePollOne();
      void scheduleSend() {}
      void sendCompleted() {}
      void close();
      bool terminated();
      bool isOpen();
      void start();

      static void receiveThread(void* param);
      static void sendThread(void* param);

    protected:
      void sendBufferFull(int tries);
      virtual void internalDestroy() = 0;

      /**
       * Called to any resources just before closing transport
       * @param[in] force   flag indicating if forced (e.g. forced
       * disconnect) is required
       */
      virtual void internalClose(bool force);

      /**
       * Called to any resources just after closing transport and without any locks held on transport
       * @param[in] force   flag indicating if forced (e.g. forced
       * disconnect) is required
       */
      virtual void internalPostClose(bool force);

    private:
      AtomicValue<bool> _isOpen;
      volatile epicsThreadId _readThread;
      volatile epicsThreadId _sendThread;
      epics::pvData::Event _shutdownEvent;
    };


    class  BlockingSocketAbstractCodec:
        public BlockingAbstractCodec
    {

    public: 

      BlockingSocketAbstractCodec(
        SOCKET channel,
        int32_t sendBufferSize,
        int32_t receiveBufferSize);

      int read(epics::pvData::ByteBuffer* dst);
      int write(epics::pvData::ByteBuffer* src);
      osiSockAddr getLastReadBufferSocketAddress()  { return _socketAddress;}
      void invalidDataStreamHandler();
      std::size_t getSocketReceiveBufferSize() const;

    protected:

      void internalDestroy();

      SOCKET _channel;
      osiSockAddr _socketAddress;
    };


    class  BlockingTCPTransportCodec :
      public BlockingSocketAbstractCodec
    
    {      

    public:

      epics::pvData::String getType() const  {
        return epics::pvData::String("TCP");
      }


      void internalDestroy()  {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::internalDestroy() enter: (threadId: %u)", 
          epicsThreadGetIdSelf());

        BlockingSocketAbstractCodec::internalDestroy();
        Transport::shared_pointer thisSharedPtr = this->shared_from_this();
        _context->getTransportRegistry()->remove(thisSharedPtr);
      }


      void changedTransport() {}


      void processControlMessage()  { 

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::processControlMessage()"
          "enter: (threadId: %u)", 
          epicsThreadGetIdSelf());

        if (_command == 2)
        {
          // check 7-th bit
          setByteOrder(_flags < 0 ? EPICS_ENDIAN_BIG : EPICS_ENDIAN_LITTLE);
        }
      }


      void processApplicationMessage()  {

        LOG(logLevelTrace, 
            "BlockingTCPTransportCodec::processApplicationMessage() enter:"
            " (threadId: %u)", 
          epicsThreadGetIdSelf());

        _responseHandler->handleResponse(&_socketAddress, shared_from_this(), 
          _version, _command, _payloadSize, _socketBuffer.get());
      }


      const osiSockAddr* getRemoteAddress() const  {
        return &_socketAddress;
      }


      epics::pvData::int8 getRevision() const  {
        return PVA_PROTOCOL_REVISION;
      }


      std::size_t getReceiveBufferSize() const  {
        return _socketBuffer->getSize();
      }


      epics::pvData::int16 getPriority() const  {
        return _priority;
      }


      void setRemoteRevision(epics::pvData::int8 revision)  {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::setRemoteRevision() enter:"
          " revision: %d (threadId: %u)", 
          revision, epicsThreadGetIdSelf());

        _remoteTransportRevision = revision;
      }


      void setRemoteTransportReceiveBufferSize(
        std::size_t remoteTransportReceiveBufferSize)  {

        LOG(logLevelTrace,    
          "BlockingTCPTransportCodec::setRemoteTransportReceiveBufferSize()"
          " enter: remoteTransportReceiveBufferSize:%u (threadId: %u)", 
          remoteTransportReceiveBufferSize, epicsThreadGetIdSelf());

        _remoteTransportReceiveBufferSize = remoteTransportReceiveBufferSize;
      }


      void setRemoteTransportSocketReceiveBufferSize(
        std::size_t socketReceiveBufferSize)  {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::"
          "setRemoteTransportSocketReceiveBufferSize()"
          "enter: socketReceiveBufferSize:%u (threadId: %u)", 
          socketReceiveBufferSize, epicsThreadGetIdSelf());

        _remoteTransportSocketReceiveBufferSize = socketReceiveBufferSize;
      }


      std::tr1::shared_ptr<const epics::pvData::Field>
        cachedDeserialize(epics::pvData::ByteBuffer* buffer) 
      {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::cachedDeserialize() enter:"
          "  (threadId: %u)", 
          epicsThreadGetIdSelf());

        return _incomingIR.deserialize(buffer, this);
      }


      void cachedSerialize(
        const std::tr1::shared_ptr<const epics::pvData::Field>& field, 
        epics::pvData::ByteBuffer* buffer) 
      {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::cachedSerialize() enter:"
          " (threadId: %u)", 
          epicsThreadGetIdSelf());

        _outgoingIR.serialize(field, buffer, this);
      }


      bool directSerialize(
        epics::pvData::ByteBuffer * /*existingBuffer*/, 
        const char* /*toSerialize*/,
        std::size_t /*elementCount*/, std::size_t /*elementSize*/)  
      {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::directSerialize() enter: (threadId: %u)", 
          epicsThreadGetIdSelf());

        return false;
      }


      bool directDeserialize(epics::pvData::ByteBuffer * /*existingBuffer*/, 
        char* /*deserializeTo*/,
        std::size_t /*elementCount*/, std::size_t /*elementSize*/)  { 

          LOG(logLevelTrace, 
            "BlockingTCPTransportCodec::directDeserialize() enter:"
            "  (threadId: %u)", 
            epicsThreadGetIdSelf());

          return false;
      }


      void flushSendQueue() { };


      bool isClosed()  {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::isClosed() enter:  (threadId: %u)", 
          epicsThreadGetIdSelf());

        return !isOpen();
      }


      void activate() {

        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec::activate() enter:  (threadId: %u)", 
          epicsThreadGetIdSelf());

        Transport::shared_pointer thisSharedPtr = shared_from_this();
        _context->getTransportRegistry()->put(thisSharedPtr);
        
        start();
      }

    protected:

      BlockingTCPTransportCodec(
        Context::shared_pointer const & context, 
        SOCKET channel,
        std::auto_ptr<ResponseHandler>& responseHandler, 
        int32_t sendBufferSize, 
        int32_t receiveBufferSize,
        epics::pvData::int16 priority
        ): 
      BlockingSocketAbstractCodec(channel, sendBufferSize, receiveBufferSize), 
        _context(context), _responseHandler(responseHandler), 
        _remoteTransportReceiveBufferSize(MAX_TCP_RECV),
        _remoteTransportRevision(0), _priority(priority)  
      { 
        LOG(logLevelTrace, 
          "BlockingTCPTransportCodec constructed:  (threadId: %u)", 
          epicsThreadGetIdSelf());
      }

      Context::shared_pointer _context;

      IntrospectionRegistry _incomingIR;
      IntrospectionRegistry _outgoingIR;

    private:

      std::auto_ptr<ResponseHandler> _responseHandler;
      size_t _remoteTransportReceiveBufferSize;
      epics::pvData::int8 _remoteTransportRevision;
      epics::pvData::int16 _priority;
    };


    class BlockingServerTCPTransportCodec : 
      public BlockingTCPTransportCodec,
      public ChannelHostingTransport,
      public TransportSender {

    public:
      POINTER_DEFINITIONS(BlockingServerTCPTransportCodec);

    protected:
      BlockingServerTCPTransportCodec(
        Context::shared_pointer const & context, 
        SOCKET channel,
        std::auto_ptr<ResponseHandler>& responseHandler, 
        int32_t sendBufferSize, 
        int32_t receiveBufferSize );

    public:
      static shared_pointer create(
        Context::shared_pointer const & context, 
        SOCKET channel,
        std::auto_ptr<ResponseHandler>& responseHandler,
        int sendBufferSize, 
        int receiveBufferSize)
      {
        shared_pointer thisPointer(
          new BlockingServerTCPTransportCodec(
            context, channel, responseHandler, 
            sendBufferSize, receiveBufferSize)
          );
        thisPointer->activate();
        return thisPointer;
      }

    public:

      bool acquire(std::tr1::shared_ptr<TransportClient> const & client)
      {

        LOG(logLevelTrace, 
          "BlockingServerTCPTransportCodec::acquire() enter:"
          " client is set: %d  (threadId: %u)", 
          (client.get() != 0), epicsThreadGetIdSelf());

        return false;
      }

      void release(pvAccessID /*clientId*/) {}

      pvAccessID preallocateChannelSID();

      void depreallocateChannelSID(pvAccessID /*sid*/) {
        // noop
      }

      void registerChannel(
        pvAccessID sid, 
        ServerChannel::shared_pointer const & channel);

      void unregisterChannel(pvAccessID sid);

      ServerChannel::shared_pointer getChannel(pvAccessID sid);

      int getChannelCount();

      epics::pvData::PVField::shared_pointer getSecurityToken() {

        LOG(logLevelTrace, 
          "BlockingServerTCPTransportCodec::getSecurityToken() enter:"
          "  (threadId: %u)", 
          epicsThreadGetIdSelf());

        return epics::pvData::PVField::shared_pointer();
      }

      void lock() {
        // noop
      }

      void unlock() {
        // noop
      }

      bool verify(epics::pvData::int32 timeoutMs) {

        LOG(logLevelTrace, 
          "BlockingServerTCPTransportCodec::verify() enter: "
          "timeoutMs:%d  (threadId: %u)", 
          timeoutMs, epicsThreadGetIdSelf());

        TransportSender::shared_pointer transportSender = 
          std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
        enqueueSendRequest(transportSender);
        verified();
        return true;
      }

      void verified() {
      }

      void aliveNotification() {
        // noop on server-side
      }

      void send(epics::pvData::ByteBuffer* buffer,
        TransportSendControl* control);

      virtual ~BlockingServerTCPTransportCodec();

    protected:

      void destroyAllChannels();
      virtual void internalClose(bool force);

    private:

      /**
      * Last SID cache.
      */
      pvAccessID _lastChannelSID;

      /**
      * Channel table (SID -> channel mapping).
      */
      std::map<pvAccessID, ServerChannel::shared_pointer> _channels;

      epics::pvData::Mutex _channelsMutex;

    };
    
    class BlockingClientTCPTransportCodec :
      public BlockingTCPTransportCodec,
      public TransportSender,
      public epics::pvData::TimerCallback {

    public:
      POINTER_DEFINITIONS(BlockingClientTCPTransportCodec);

    protected:
      BlockingClientTCPTransportCodec(
        Context::shared_pointer const & context,
        SOCKET channel,
        std::auto_ptr<ResponseHandler>& responseHandler,
        int32_t sendBufferSize, 
        int32_t receiveBufferSize,
        TransportClient::shared_pointer const & client,
        epics::pvData::int8 remoteTransportRevision,
        float beaconInterval,
        int16_t priority);

    public:
      static shared_pointer create(
        Context::shared_pointer const & context,
        SOCKET channel,
        std::auto_ptr<ResponseHandler>& responseHandler,
        int32_t sendBufferSize, 
        int32_t receiveBufferSize,
        TransportClient::shared_pointer const & client,
        int8_t remoteTransportRevision,
        float beaconInterval,
        int16_t priority )
      {
        shared_pointer thisPointer(
          new BlockingClientTCPTransportCodec(
            context, channel, responseHandler,
            sendBufferSize, receiveBufferSize,
            client, remoteTransportRevision,
            beaconInterval, priority)
          );
        thisPointer->activate();
        return thisPointer;
      }

    public:

      void start();
    
      virtual ~BlockingClientTCPTransportCodec();
    
      virtual void timerStopped() {
        // noop
      }
    
      virtual void callback();
    
      bool acquire(TransportClient::shared_pointer const & client);

      void release(pvAccessID clientId);
    
      void changedTransport();
    
      void lock() {
        // noop
      }
    
      void unlock() {
        // noop
      }
    
      bool verify(epics::pvData::int32 timeoutMs);

      void verified();

      void aliveNotification();

      void send(epics::pvData::ByteBuffer* buffer,
        TransportSendControl* control);
    
    protected:
    
      virtual void internalClose(bool force);
      virtual void internalPostClose(bool force);
    
    private:
    
      /**
       * Owners (users) of the transport.
       */
      // TODO consider using TR1 hash map
      typedef std::map<pvAccessID, TransportClient::weak_pointer> TransportClientMap_t;
      TransportClientMap_t _owners;
    
      /**
       * Connection timeout (no-traffic) flag.
       */
      double _connectionTimeout;
    
      /**
       * Unresponsive transport flag.
       */
      bool _unresponsiveTransport;
    
      /**
       * Timestamp of last "live" event on this transport.
       */
      epicsTimeStamp _aliveTimestamp;
    
      bool _verifyOrEcho;
    
      /**
       * Unresponsive transport notify.
       */
      void unresponsiveTransport();
    
      /**
       * Notifies clients about disconnect.
       */
      void closedNotifyClients();
    
      /**
       * Responsive transport notify.
       */
      void responsiveTransport();
      
      
      epics::pvData::Mutex _mutex;

      bool _verified;   
      epics::pvData::Mutex _verifiedMutex;
      epics::pvData::Event _verifiedEvent;
      
    };
    
  }
}

#endif /* CODEC_H_ */
