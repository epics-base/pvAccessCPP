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

#include <shareLib.h>
#include <osiSock.h>
#include <epicsTime.h>
#include <epicsThread.h>
#include <epicsVersion.h>
#include <epicsAtomic.h>

#include <pv/byteBuffer.h>
#include <pv/pvType.h>
#include <pv/lock.h>
#include <pv/timer.h>
#include <pv/event.h>
#include <pv/likely.h>

#include <pv/pvaConstants.h>
#include <pv/remote.h>
#include <pv/security.h>
#include <pv/transportRegistry.h>
#include <pv/introspectionRegistry.h>
#include <pv/inetAddressUtil.h>

/* C++11 keywords
 @code
 struct Base {
   virtual void foo();
 };
 struct Class : public Base {
   virtual void foo() OVERRIDE FINAL FINAL;
 };
 @endcode
 */
#ifndef FINAL
#  if __cplusplus>=201103L
#    define FINAL final
#  else
#    define FINAL
#  endif
#endif
#ifndef OVERRIDE
#  if __cplusplus>=201103L
#    define OVERRIDE override
#  else
#    define OVERRIDE
#  endif
#endif


namespace epics {
namespace pvAccess {

class ServerChannel;

namespace detail {

template<typename T>
class AtomicValue
{
    T val;
public:
    AtomicValue() :val(0) {}
    inline T getAndSet(T newval)
    {
        int oldval;
        // epicsAtomic doesn't have unconditional swap
        do {
            oldval = epics::atomic::get(val);
        } while(epics::atomic::compareAndSwap(val, oldval, newval)!=oldval);
        return oldval;
    }
    inline T get() {
        return epics::atomic::get(val);
    }
};
// treat bool as int
template<>
class AtomicValue<bool>
{
    AtomicValue<int> realval;
public:
    inline bool getAndSet(bool newval)
    {
        return this->realval.getAndSet(newval?1:0)!=0;
    }
    inline bool get() {
        return !!this->realval.get();
    }
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


class epicsShareClass AbstractCodec :
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
        bool serverFlag,
        size_t sendBufferSize,
        size_t receiveBufferSize,
        int32_t socketSendBufferSize,
        bool blockingProcessQueue);

    virtual void processControlMessage() = 0;
    virtual void processApplicationMessage() = 0;
    virtual const osiSockAddr* getLastReadBufferSocketAddress() = 0;
    virtual void invalidDataStreamHandler() = 0;
    virtual void readPollOne()=0;
    virtual void writePollOne() = 0;
    virtual void scheduleSend() = 0;
    virtual void sendCompleted() = 0;
    virtual bool terminated() = 0;
    virtual int write(epics::pvData::ByteBuffer* src) = 0;
    virtual int read(epics::pvData::ByteBuffer* dst) = 0;
    virtual bool isOpen() = 0;


    virtual ~AbstractCodec()
    {
    }

    virtual void alignBuffer(std::size_t alignment) OVERRIDE FINAL;
    virtual void ensureData(std::size_t size) OVERRIDE FINAL;
    virtual void alignData(std::size_t alignment) OVERRIDE FINAL;
    virtual void startMessage(
            epics::pvData::int8 command,
            std::size_t ensureCapacity = 0,
            epics::pvData::int32 payloadSize = 0) OVERRIDE FINAL;
    void putControlMessage(
            epics::pvData::int8 command,
            epics::pvData::int32 data);
    virtual void endMessage() OVERRIDE FINAL;
    virtual void ensureBuffer(std::size_t size) OVERRIDE FINAL;
    virtual void flushSerializeBuffer() OVERRIDE FINAL;
    virtual void flush(bool lastMessageCompleted) OVERRIDE FINAL;
    void processWrite();
    void processRead();
    void processSendQueue();
    virtual void enqueueSendRequest(TransportSender::shared_pointer const & sender) OVERRIDE FINAL;
    void enqueueSendRequest(TransportSender::shared_pointer const & sender,
                            std::size_t requiredBufferSize);
    void setSenderThread();
    virtual void setRecipient(osiSockAddr const & sendTo) OVERRIDE FINAL;
    virtual void setByteOrder(int byteOrder) OVERRIDE FINAL;

    static std::size_t alignedValue(std::size_t value, std::size_t alignment);

    virtual bool directSerialize(
            epics::pvData::ByteBuffer * /*existingBuffer*/,
            const char* /*toSerialize*/,
            std::size_t /*elementCount*/, std::size_t /*elementSize*/) OVERRIDE;


    virtual bool directDeserialize(epics::pvData::ByteBuffer * /*existingBuffer*/,
                                   char* /*deserializeTo*/,
                                   std::size_t /*elementCount*/, std::size_t /*elementSize*/) OVERRIDE;

    bool sendQueueEmpty() const {
        return _sendQueue.empty();
    }

    epics::pvData::int8 getRevision() const {
        epicsGuard<epicsMutex> G(_mutex);
        int8_t myver = _clientServerFlag ? PVA_SERVER_PROTOCOL_REVISION : PVA_CLIENT_PROTOCOL_REVISION;
        return myver < _version ? myver : _version;
    }

protected:

    virtual void sendBufferFull(int tries) = 0;
    void send(epics::pvData::ByteBuffer *buffer);
    void flushSendBuffer();

    virtual void setRxTimeout(bool ena) {}

    ReadMode _readMode;
    int8_t _version;
    int8_t _flags;
    int8_t _command;
    int32_t _payloadSize; // TODO why not size_t?
    epics::pvData::int32 _remoteTransportSocketReceiveBufferSize;
    //TODO initialize union
    osiSockAddr _sendTo;
    epicsThreadId _senderThread;
    WriteMode _writeMode;
    bool _writeOpReady;

    epics::pvData::ByteBuffer _socketBuffer;
    epics::pvData::ByteBuffer _sendBuffer;

    fair_queue<TransportSender> _sendQueue;

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

    const std::size_t _maxSendPayloadSize;
    std::size_t _lastMessageStartPosition;
    std::size_t _lastSegmentedMessageType;
    int8_t _lastSegmentedMessageCommand;
    std::size_t _nextMessagePayloadOffset;

    epics::pvData::int8 _byteOrderFlag;
protected:
    const epics::pvData::int8 _clientServerFlag;
private:

public:
    mutable epics::pvData::Mutex _mutex;
};


class BlockingTCPTransportCodec:
    public AbstractCodec,
    public AuthenticationPluginControl,
    public std::tr1::enable_shared_from_this<BlockingTCPTransportCodec>
{

public:

    POINTER_DEFINITIONS(BlockingTCPTransportCodec);

    static size_t num_instances;

    BlockingTCPTransportCodec(
            bool serverFlag,
            Context::shared_pointer const & context,
            SOCKET channel,
            ResponseHandler::shared_pointer const & responseHandler,
            size_t sendBufferSize,
            size_t receiveBufferSize,
            epics::pvData::int16 priority);
    virtual ~BlockingTCPTransportCodec();

    virtual void readPollOne() OVERRIDE FINAL;
    virtual void writePollOne() OVERRIDE FINAL;
    virtual void scheduleSend() OVERRIDE FINAL {}
    virtual void sendCompleted() OVERRIDE FINAL {}
    virtual void close() OVERRIDE FINAL;
    virtual void waitJoin() OVERRIDE FINAL;
    virtual bool terminated() OVERRIDE FINAL;
    virtual bool isOpen() OVERRIDE FINAL;
    virtual void start();

    virtual int read(epics::pvData::ByteBuffer* dst) OVERRIDE FINAL;
    virtual int write(epics::pvData::ByteBuffer* src) OVERRIDE FINAL;
    virtual const osiSockAddr* getLastReadBufferSocketAddress() OVERRIDE FINAL  {
        return &_socketAddress;
    }
    virtual void invalidDataStreamHandler() OVERRIDE FINAL;

    virtual std::string getType() const OVERRIDE FINAL {
        return std::string("tcp");
    }

    virtual void processControlMessage() OVERRIDE FINAL {
        if (_command == CMD_SET_ENDIANESS)
        {
            // check 7-th bit
            setByteOrder(_flags < 0 ? EPICS_ENDIAN_BIG : EPICS_ENDIAN_LITTLE);
        }
    }


    virtual void processApplicationMessage() OVERRIDE FINAL {
        _responseHandler->handleResponse(&_socketAddress, shared_from_this(),
                                         _version, _command, _payloadSize, &_socketBuffer);
    }


    virtual const osiSockAddr& getRemoteAddress() const OVERRIDE FINAL {
        return _socketAddress;
    }

    virtual const std::string& getRemoteName() const OVERRIDE FINAL {
        return _socketName;
    }


    virtual std::size_t getReceiveBufferSize() const OVERRIDE FINAL {
        return _socketBuffer.getSize();
    }


    virtual epics::pvData::int16 getPriority() const OVERRIDE FINAL {
        return _priority;
    }


    virtual void setRemoteTransportReceiveBufferSize(
        std::size_t remoteTransportReceiveBufferSize) OVERRIDE FINAL {
        _remoteTransportReceiveBufferSize = remoteTransportReceiveBufferSize;
    }


    virtual void setRemoteTransportSocketReceiveBufferSize(
        std::size_t socketReceiveBufferSize) OVERRIDE FINAL {
        _remoteTransportSocketReceiveBufferSize = socketReceiveBufferSize;
    }


    std::tr1::shared_ptr<const epics::pvData::Field>
    virtual cachedDeserialize(epics::pvData::ByteBuffer* buffer) OVERRIDE FINAL
    {
        return _incomingIR.deserialize(buffer, this);
    }


    virtual void cachedSerialize(
        const std::tr1::shared_ptr<const epics::pvData::Field>& field,
        epics::pvData::ByteBuffer* buffer) OVERRIDE FINAL
    {
        _outgoingIR.serialize(field, buffer, this);
    }


    virtual void flushSendQueue() OVERRIDE FINAL { }


    virtual bool isClosed() OVERRIDE FINAL {
        return !isOpen();
    }


    void activate() {
        Transport::shared_pointer thisSharedPtr = shared_from_this();
        _context->getTransportRegistry()->install(thisSharedPtr);

        start();
    }

    virtual bool verify(epics::pvData::int32 timeoutMs) OVERRIDE;

    virtual void verified(epics::pvData::Status const & status) OVERRIDE;

    virtual void authNZMessage(epics::pvData::PVStructure::shared_pointer const & data) OVERRIDE FINAL;

    virtual void sendSecurityPluginMessage(epics::pvData::PVStructure::const_shared_pointer const & data) OVERRIDE FINAL;

private:
    void receiveThread();
    void sendThread();

protected:
    virtual void setRxTimeout(bool ena) OVERRIDE FINAL;

    virtual void sendBufferFull(int tries) OVERRIDE FINAL;

    /**
     * Called from close(). after start of shutdown (isOpen()==false)
     * but before worker thread shutdown.
     */
    virtual void internalClose();

private:
    AtomicValue<bool> _isOpen;
    epics::pvData::Thread _readThread, _sendThread;
    const SOCKET _channel;
protected:
    osiSockAddr _socketAddress;
    std::string _socketName;
protected:
    Context::shared_pointer _context;

    IntrospectionRegistry _incomingIR;
    IntrospectionRegistry _outgoingIR;

    // active authentication exchange, if any
    std::string _authSessionName;
    AuthenticationSession::shared_pointer _authSession;
public:
    // final info, after authentication complete.
    PeerInfo::const_shared_pointer _peerInfo;

private:

    ResponseHandler::shared_pointer _responseHandler;
    size_t _remoteTransportReceiveBufferSize;
    epics::pvData::int16 _priority;

protected:
    bool _verified;
    epics::pvData::Event _verifiedEvent;
};

class BlockingServerTCPTransportCodec :
    public BlockingTCPTransportCodec,
    public TransportSender {

public:
    POINTER_DEFINITIONS(BlockingServerTCPTransportCodec);

protected:
    BlockingServerTCPTransportCodec(
        Context::shared_pointer const & context,
        SOCKET channel,
        ResponseHandler::shared_pointer const & responseHandler,
        int32_t sendBufferSize,
        int32_t receiveBufferSize );

public:
    static shared_pointer create(
        Context::shared_pointer const & context,
        SOCKET channel,
        ResponseHandler::shared_pointer const & responseHandler,
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

    virtual bool acquire(std::tr1::shared_ptr<ClientChannelImpl> const & /*client*/) OVERRIDE FINAL
    {
        return false;
    }

    virtual void release(pvAccessID /*clientId*/) OVERRIDE FINAL {}

    pvAccessID preallocateChannelSID();

    void depreallocateChannelSID(pvAccessID /*sid*/) {}

    void registerChannel(
            pvAccessID sid,
            std::tr1::shared_ptr<ServerChannel> const & channel);

    void unregisterChannel(pvAccessID sid);

    std::tr1::shared_ptr<ServerChannel> getChannel(pvAccessID sid);

    void getChannels(std::vector<std::tr1::shared_ptr<ServerChannel> >& channels) const;

    size_t getChannelCount() const;

    virtual bool verify(epics::pvData::int32 timeoutMs) OVERRIDE FINAL {

        TransportSender::shared_pointer transportSender =
            std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
        enqueueSendRequest(transportSender);

        bool verifiedStatus = BlockingTCPTransportCodec::verify(timeoutMs);

        enqueueSendRequest(transportSender);

        return verifiedStatus;
    }

    virtual void verified(epics::pvData::Status const & status) OVERRIDE FINAL {
        {
            epicsGuard<epicsMutex> G(_mutex);
            _verificationStatus = status;
        }
        BlockingTCPTransportCodec::verified(status);
    }

    void authNZInitialize(const std::string& securityPluginName,
                          const epics::pvData::PVStructure::shared_pointer& data);

    virtual void authenticationCompleted(epics::pvData::Status const & status,
                                         const std::tr1::shared_ptr<PeerInfo>& peer) OVERRIDE FINAL;

    virtual void send(epics::pvData::ByteBuffer* buffer,
                      TransportSendControl* control) OVERRIDE FINAL;

    virtual ~BlockingServerTCPTransportCodec() OVERRIDE FINAL;

protected:

    void destroyAllChannels();
    virtual void internalClose() OVERRIDE FINAL;

private:

    /**
    * Last SID cache.
    */
    pvAccessID _lastChannelSID;

    typedef std::map<pvAccessID, std::tr1::shared_ptr<ServerChannel> > _channels_t;
    /**
    * Channel table (SID -> channel mapping).
    */
    _channels_t _channels;

    mutable epics::pvData::Mutex _channelsMutex;

    epics::pvData::Status _verificationStatus;

    bool _verifyOrVerified;

    std::vector<std::string> advertisedAuthPlugins;

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
        ResponseHandler::shared_pointer const & responseHandler,
        int32_t sendBufferSize,
        int32_t receiveBufferSize,
        std::tr1::shared_ptr<ClientChannelImpl> const & client,
        epics::pvData::int8 remoteTransportRevision,
        float heartbeatInterval,
        int16_t priority);

public:
    static shared_pointer create(
        Context::shared_pointer const & context,
        SOCKET channel,
        ResponseHandler::shared_pointer const & responseHandler,
        int32_t sendBufferSize,
        int32_t receiveBufferSize,
        std::tr1::shared_ptr<ClientChannelImpl> const & client,
        int8_t remoteTransportRevision,
        float heartbeatInterval,
        int16_t priority )
    {
        shared_pointer thisPointer(
            new BlockingClientTCPTransportCodec(
                context, channel, responseHandler,
                sendBufferSize, receiveBufferSize,
                client, remoteTransportRevision,
                heartbeatInterval, priority)
        );
        thisPointer->activate();
        return thisPointer;
    }

public:

    virtual void start() OVERRIDE FINAL;

    virtual ~BlockingClientTCPTransportCodec() OVERRIDE FINAL;

    virtual void timerStopped() OVERRIDE FINAL {
        // noop
    }

    virtual void callback() OVERRIDE FINAL;

    virtual bool acquire(std::tr1::shared_ptr<ClientChannelImpl> const & client) OVERRIDE FINAL;

    virtual void release(pvAccessID clientId) OVERRIDE FINAL;

    virtual void send(epics::pvData::ByteBuffer* buffer,
                      TransportSendControl* control) OVERRIDE FINAL;

    void authNZInitialize(const std::vector<std::string>& offeredSecurityPlugins);

    virtual void authenticationCompleted(epics::pvData::Status const & status,
                                         const std::tr1::shared_ptr<PeerInfo>& peer) OVERRIDE FINAL;

    virtual void verified(epics::pvData::Status const & status) OVERRIDE FINAL;
protected:

    virtual void internalClose() OVERRIDE FINAL;

private:

    /**
     * Owners (users) of the transport.
     */
    // TODO consider using TR1 hash map
    typedef std::map<pvAccessID, std::tr1::weak_ptr<ClientChannelImpl> > TransportClientMap_t;
    TransportClientMap_t _owners;

    /**
     * Connection timeout (no-traffic) flag.
     */
    const double _connectionTimeout;

    bool _verifyOrEcho;

    // are we queued to send verify or echo?
    bool sendQueued;

    /**
     * Notifies clients about disconnect.
     */
    void closedNotifyClients();
};

}
}
}

#endif /* CODEC_H_ */
