/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef RESPONSEHANDLERS_H_
#define RESPONSEHANDLERS_H_

#include <list>

#include <pv/timer.h>

#include <pv/serverContext.h>
#include <pv/remote.h>
#include <pv/serverChannelImpl.h>
#include <pv/baseChannelRequester.h>
#include <pv/securityImpl.h>

namespace epics {
namespace pvAccess {

/**
 */
class AbstractServerResponseHandler : public ResponseHandler {
protected:
    ServerContextImpl::shared_pointer _context;
public:
    AbstractServerResponseHandler(ServerContextImpl::shared_pointer const & context, std::string description) :
        ResponseHandler(context.get(), description), _context(context) {
    }

    virtual ~AbstractServerResponseHandler() {}
};

/**
 * Bad request handler.
 */
class ServerBadResponse : public AbstractServerResponseHandler {
public:
    ServerBadResponse(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Bad request") {
    }

    virtual ~ServerBadResponse() {
    }

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

/**
 * Connection validation message handler.
 */
class ServerConnectionValidationHandler : public AbstractServerResponseHandler {
public:
    ServerConnectionValidationHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Connection validation") {
    }
    virtual ~ServerConnectionValidationHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

/**
 * NOOP response.
 */
class ServerNoopResponse : public AbstractServerResponseHandler {
public:
    ServerNoopResponse(ServerContextImpl::shared_pointer const & context, std::string description) :
        AbstractServerResponseHandler(context, description) {
    }
    virtual ~ServerNoopResponse() {}
};

/**
 * Echo request handler.
 */
class ServerEchoHandler : public AbstractServerResponseHandler {
public:
    ServerEchoHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Echo request") {
    }
    virtual ~ServerEchoHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class EchoTransportSender : public TransportSender {
public:
    EchoTransportSender(osiSockAddr* echoFrom, size_t payloadSize, epics::pvData::ByteBuffer& payloadBuffer) {
        memcpy(&_echoFrom, echoFrom, sizeof(osiSockAddr));
        toEcho.resize(payloadSize);
        payloadBuffer.getArray(&toEcho[0], payloadSize);
    }

    virtual ~EchoTransportSender() {}

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL {
        control->startMessage(CMD_ECHO, toEcho.size(), toEcho.size());
        control->setRecipient(_echoFrom);
        buffer->putArray<char>(&toEcho[0], toEcho.size());
    }

private:
    osiSockAddr _echoFrom;
    std::vector<char> toEcho;
};

/****************************************************************************************/
/**
 * Search channel request handler.
 */
// TODO object pool!!!
class ServerSearchHandler : public AbstractServerResponseHandler
{
public:
    static const std::string SUPPORTED_PROTOCOL;

    ServerSearchHandler(ServerContextImpl::shared_pointer const & context);
    virtual ~ServerSearchHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};


class ServerChannelFindRequesterImpl:
    public ChannelFindRequester,
    public TransportSender,
    public epics::pvData::TimerCallback,
    public std::tr1::enable_shared_from_this<ServerChannelFindRequesterImpl>
{
public:
    ServerChannelFindRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                   const PeerInfo::const_shared_pointer& peer,
                                   epics::pvData::int32 expectedResponseCount);
    virtual ~ServerChannelFindRequesterImpl() {}
    void clear();
    ServerChannelFindRequesterImpl* set(std::string _name, epics::pvData::int32 searchSequenceId,
                                        epics::pvData::int32 cid, osiSockAddr const & sendTo, bool responseRequired, bool serverSearch);
    virtual void channelFindResult(const epics::pvData::Status& status, ChannelFind::shared_pointer const & channelFind, bool wasFound) OVERRIDE FINAL;

    virtual std::tr1::shared_ptr<const PeerInfo> getPeerInfo() OVERRIDE FINAL;
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;

    virtual void callback() OVERRIDE FINAL;
    virtual void timerStopped() OVERRIDE FINAL;

private:
    ServerGUID _guid;
    std::string _name;
    epics::pvData::int32 _searchSequenceId;
    epics::pvData::int32 _cid;
    osiSockAddr _sendTo;
    bool _responseRequired;
    bool _wasFound;
    const ServerContextImpl::shared_pointer _context;
    const PeerInfo::const_shared_pointer _peer;
    mutable epics::pvData::Mutex _mutex;
    const epics::pvData::int32 _expectedResponseCount;
    epics::pvData::int32 _responseCount;
    bool _serverSearch;
};

/****************************************************************************************/
/**
 * Create channel request handler.
 */
class ServerCreateChannelHandler : public AbstractServerResponseHandler
{
public:
    ServerCreateChannelHandler(ServerContextImpl::shared_pointer const & context)
        :AbstractServerResponseHandler(context, "Create channel request")
    {}
    virtual ~ServerCreateChannelHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;

private:
    // Name of the magic "server" PV used to implement channelList() and server info
    static const std::string SERVER_CHANNEL_NAME;

    void disconnect(Transport::shared_pointer const & transport);
};

namespace detail {
class BlockingServerTCPTransportCodec;
}

class ServerChannelRequesterImpl :
    public ChannelRequester,
    public TransportSender,
    public std::tr1::enable_shared_from_this<ServerChannelRequesterImpl>
{
    friend class ServerCreateChannelHandler;
public:
    typedef std::tr1::shared_ptr<ServerChannelRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelRequesterImpl> const_shared_pointer;
protected:
    ServerChannelRequesterImpl(Transport::shared_pointer const & transport,
                               const std::string channelName,
                               const pvAccessID cid);
public:
    virtual ~ServerChannelRequesterImpl() {}
    static ChannelRequester::shared_pointer create(ChannelProvider::shared_pointer const & provider,
            Transport::shared_pointer const & transport, const std::string channelName,
            const pvAccessID cid);
    virtual void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel) OVERRIDE FINAL;
    virtual void channelStateChange(Channel::shared_pointer const & c, const Channel::ConnectionState isConnected) OVERRIDE FINAL;
    virtual std::tr1::shared_ptr<const PeerInfo> getPeerInfo() OVERRIDE FINAL;
    virtual std::string getRequesterName() OVERRIDE FINAL;
    virtual void message(std::string const & message, epics::pvData::MessageType messageType) OVERRIDE FINAL;
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
private:
    ServerChannel::weak_pointer _serverChannel;
    std::tr1::weak_ptr<detail::BlockingServerTCPTransportCodec> _transport;
    const std::string _channelName;
    const pvAccessID _cid;
    bool _created;
    epics::pvData::Status _status;
    epics::pvData::Mutex _mutex;
};

/****************************************************************************************/
/**
 * Destroy channel request handler.
 */
class ServerDestroyChannelHandler : public AbstractServerResponseHandler
{
public:
    ServerDestroyChannelHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Destroy channel request") {
    }
    virtual ~ServerDestroyChannelHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};


class ServerDestroyChannelHandlerTransportSender : public TransportSender
{
public:
    ServerDestroyChannelHandlerTransportSender(pvAccessID cid, pvAccessID sid): _cid(cid), _sid(sid) {
    }

    virtual ~ServerDestroyChannelHandlerTransportSender() {}
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL {
        control->startMessage((epics::pvData::int8)CMD_DESTROY_CHANNEL, 2*sizeof(epics::pvData::int32)/sizeof(epics::pvData::int8));
        buffer->putInt(_sid);
        buffer->putInt(_cid);
    }

private:
    pvAccessID _cid;
    pvAccessID _sid;
};

/****************************************************************************************/
/**
 * Get request handler.
 */
class ServerGetHandler : public AbstractServerResponseHandler
{
public:
    ServerGetHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Get request") {
    }
    virtual ~ServerGetHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class ServerChannelGetRequesterImpl :
    public BaseChannelRequester,
    public ChannelGetRequester,
    public std::tr1::enable_shared_from_this<ServerChannelGetRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerChannelGetRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelGetRequesterImpl> const_shared_pointer;
protected:
    ServerChannelGetRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                  std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                  Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static ChannelGetRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport,
            epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual ~ServerChannelGetRequesterImpl() {}
    virtual void channelGetConnect(const epics::pvData::Status& status, ChannelGet::shared_pointer const & channelGet,
                                   epics::pvData::Structure::const_shared_pointer const & structure) OVERRIDE FINAL;
    virtual void getDone(const epics::pvData::Status& status, ChannelGet::shared_pointer const & channelGet,
                         epics::pvData::PVStructure::shared_pointer const & pvStructure,
                         epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    ChannelGet::shared_pointer getChannelGet();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return getChannelGet(); }

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
private:
    // Note: this forms a reference loop, which is broken in destroy()
    ChannelGet::shared_pointer _channelGet;
    epics::pvData::PVStructure::shared_pointer _pvStructure;
    epics::pvData::BitSet::shared_pointer _bitSet;
    epics::pvData::Status _status;
};


/****************************************************************************************/
/**
 * Put request handler.
 */
class ServerPutHandler : public AbstractServerResponseHandler
{
public:
    ServerPutHandler(ServerContextImpl::shared_pointer context) :
        AbstractServerResponseHandler(context, "Put request") {
    }
    virtual ~ServerPutHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class ServerChannelPutRequesterImpl :
    public BaseChannelRequester,
    public ChannelPutRequester,
    public std::tr1::enable_shared_from_this<ServerChannelPutRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerChannelPutRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelPutRequesterImpl> const_shared_pointer;
protected:
    ServerChannelPutRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                  std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                  Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static ChannelPutRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);

    virtual ~ServerChannelPutRequesterImpl() {}
    virtual void channelPutConnect(const epics::pvData::Status& status, ChannelPut::shared_pointer const & channelPut,
                                   epics::pvData::Structure::const_shared_pointer const & structure) OVERRIDE FINAL;
    virtual void putDone(const epics::pvData::Status& status, ChannelPut::shared_pointer const & channelPut) OVERRIDE FINAL;
    virtual void getDone(const epics::pvData::Status& status, ChannelPut::shared_pointer const & channelPut,
                         epics::pvData::PVStructure::shared_pointer const & pvStructure,
                         epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    ChannelPut::shared_pointer getChannelPut();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return getChannelPut(); }

    epics::pvData::BitSet::shared_pointer getPutBitSet();
    epics::pvData::PVStructure::shared_pointer getPutPVStructure();
    void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
private:
    // Note: this forms a reference loop, which is broken in destroy()
    ChannelPut::shared_pointer _channelPut;
    epics::pvData::BitSet::shared_pointer _bitSet;
    epics::pvData::PVStructure::shared_pointer _pvStructure;
    epics::pvData::Status _status;
};

/****************************************************************************************/
/**
 * Put request handler.
 */
class ServerPutGetHandler : public AbstractServerResponseHandler
{
public:
    ServerPutGetHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Put-get request") {
    }

    virtual ~ServerPutGetHandler() {}
    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class ServerChannelPutGetRequesterImpl :
    public BaseChannelRequester,
    public ChannelPutGetRequester,
    public std::tr1::enable_shared_from_this<ServerChannelPutGetRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerChannelPutGetRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelPutGetRequesterImpl> const_shared_pointer;
protected:
    ServerChannelPutGetRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                     std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                     Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static ChannelPutGetRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual ~ServerChannelPutGetRequesterImpl() {}

    virtual void channelPutGetConnect(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & channelPutGet,
                                      epics::pvData::Structure::const_shared_pointer const & putStructure,
                                      epics::pvData::Structure::const_shared_pointer const & getStructure) OVERRIDE FINAL;
    virtual void getGetDone(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & channelPutGet,
                            epics::pvData::PVStructure::shared_pointer const & pvStructure,
                            epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL;
    virtual void getPutDone(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & channelPutGet,
                            epics::pvData::PVStructure::shared_pointer const & pvStructure,
                            epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL;
    virtual void putGetDone(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & channelPutGet,
                            epics::pvData::PVStructure::shared_pointer const & pvStructure,
                            epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    ChannelPutGet::shared_pointer getChannelPutGet();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return getChannelPutGet(); }

    epics::pvData::PVStructure::shared_pointer getPutGetPVStructure();
    epics::pvData::BitSet::shared_pointer getPutGetBitSet();

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
private:
    // Note: this forms a reference loop, which is broken in destroy()
    ChannelPutGet::shared_pointer _channelPutGet;
    epics::pvData::PVStructure::shared_pointer _pvPutStructure;
    epics::pvData::BitSet::shared_pointer _pvPutBitSet;
    epics::pvData::PVStructure::shared_pointer _pvGetStructure;
    epics::pvData::BitSet::shared_pointer _pvGetBitSet;
    epics::pvData::Status _status;
};


/****************************************************************************************/
/**
 * Monitor request handler.
 */
class ServerMonitorHandler : public AbstractServerResponseHandler
{
public:
    ServerMonitorHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Monitor request") {
    }
    virtual ~ServerMonitorHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};


class ServerMonitorRequesterImpl :
    public BaseChannelRequester,
    public MonitorRequester,
    public std::tr1::enable_shared_from_this<ServerMonitorRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerMonitorRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerMonitorRequesterImpl> const_shared_pointer;
protected:
    ServerMonitorRequesterImpl(ServerContextImpl::shared_pointer const & context,
                               std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                               Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual ~ServerMonitorRequesterImpl() {}

    virtual void monitorConnect(const epics::pvData::Status& status,
                                Monitor::shared_pointer const & monitor,
                                epics::pvData::StructureConstPtr const & structure) OVERRIDE FINAL;
    virtual void unlisten(Monitor::shared_pointer const & monitor) OVERRIDE FINAL;
    virtual void monitorEvent(Monitor::shared_pointer const & monitor) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    Monitor::shared_pointer getChannelMonitor();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return std::tr1::shared_ptr<ChannelRequest>(); }

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
    void ack(size_t cnt);
private:
    // Note: this forms a reference loop, which is broken in destroy()
    Monitor::shared_pointer _channelMonitor;
    epics::pvData::StructureConstPtr _structure;
    epics::pvData::Status _status;
    // when _pipeline==true
    // _window_open + _window_closed.size() are together the congestion control window.
    // _window_open are the number of elements which we can send w/o further ack's
    size_t _window_open;
    // The elements we have sent, but have not been acknowledged
    typedef std::list<epics::pvData::MonitorElementPtr> window_t;
    window_t _window_closed;
    bool _unlisten;
    bool _pipeline; // const after activate()
};


/****************************************************************************************/
/**
 * Array request handler.
 */
class ServerArrayHandler : public AbstractServerResponseHandler
{
public:
    ServerArrayHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Array request") {
    }
    virtual ~ServerArrayHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class ServerChannelArrayRequesterImpl :
    public BaseChannelRequester,
    public ChannelArrayRequester,
    public std::tr1::enable_shared_from_this<ServerChannelArrayRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerChannelArrayRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelArrayRequesterImpl> const_shared_pointer;
protected:
    ServerChannelArrayRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                    std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                    Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static ChannelArrayRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual ~ServerChannelArrayRequesterImpl() {}

    virtual void channelArrayConnect(const epics::pvData::Status& status,
                                     ChannelArray::shared_pointer const & channelArray,
                                     epics::pvData::Array::const_shared_pointer const & array) OVERRIDE FINAL;
    virtual void getArrayDone(const epics::pvData::Status& status,
                              ChannelArray::shared_pointer const & channelArray,
                              epics::pvData::PVArray::shared_pointer const & pvArray) OVERRIDE FINAL;
    virtual void putArrayDone(const epics::pvData::Status& status,
                              ChannelArray::shared_pointer const & channelArray) OVERRIDE FINAL;
    virtual void setLengthDone(const epics::pvData::Status& status,
                               ChannelArray::shared_pointer const & channelArray) OVERRIDE FINAL;
    virtual void getLengthDone(const epics::pvData::Status& status,
                               ChannelArray::shared_pointer const & channelArray,
                               std::size_t length) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    ChannelArray::shared_pointer getChannelArray();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return getChannelArray(); }

    epics::pvData::PVArray::shared_pointer getPVArray();
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;

private:
    // Note: this forms a reference loop, which is broken in destroy()
    ChannelArray::shared_pointer _channelArray;
    epics::pvData::PVArray::shared_pointer _pvArray;

    std::size_t _length;
    epics::pvData::Status _status;
};

/****************************************************************************************/
/**
 * Destroy request handler.
 */
class ServerDestroyRequestHandler : public AbstractServerResponseHandler
{
public:
    ServerDestroyRequestHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Destroy request") {
    }
    virtual ~ServerDestroyRequestHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
private:

    void failureResponse(Transport::shared_pointer const & transport, pvAccessID ioid, const epics::pvData::Status& errorStatus);
};


/****************************************************************************************/
/**
 * Cancel request handler.
 */
class ServerCancelRequestHandler : public AbstractServerResponseHandler
{
public:
    ServerCancelRequestHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Cancel request") {
    }
    virtual ~ServerCancelRequestHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
private:

    void failureResponse(Transport::shared_pointer const & transport, pvAccessID ioid, const epics::pvData::Status& errorStatus);
};


/****************************************************************************************/
/**
 * Process request handler.
 */
class ServerProcessHandler : public AbstractServerResponseHandler
{
public:
    ServerProcessHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Process request") {
    }
    virtual ~ServerProcessHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class ServerChannelProcessRequesterImpl :
    public BaseChannelRequester,
    public ChannelProcessRequester,
    public std::tr1::enable_shared_from_this<ServerChannelProcessRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerChannelProcessRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelProcessRequesterImpl> const_shared_pointer;
protected:
    ServerChannelProcessRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                      std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                      Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static ChannelProcessRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport, epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual ~ServerChannelProcessRequesterImpl() {}

    virtual void channelProcessConnect(const epics::pvData::Status& status, ChannelProcess::shared_pointer const & channelProcess) OVERRIDE FINAL;
    virtual void processDone(const epics::pvData::Status& status, ChannelProcess::shared_pointer const & channelProcess) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;

    ChannelProcess::shared_pointer getChannelProcess();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return getChannelProcess(); }

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;

private:
    // Note: this forms a reference loop, which is broken in destroy()
    ChannelProcess::shared_pointer _channelProcess;
    epics::pvData::Status _status;
};

/****************************************************************************************/
/**
 *  Get field request handler.
 */
class ServerGetFieldHandler : public AbstractServerResponseHandler
{
public:
    ServerGetFieldHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Get field request") {
    }
    virtual ~ServerGetFieldHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
private:
    void getFieldFailureResponse(Transport::shared_pointer const & transport, const pvAccessID ioid, const epics::pvData::Status& errorStatus);
};

class ServerGetFieldRequesterImpl :
    public BaseChannelRequester,
    public GetFieldRequester,
    public std::tr1::enable_shared_from_this<ServerGetFieldRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerGetFieldRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerGetFieldRequesterImpl> const_shared_pointer;

    ServerGetFieldRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                Transport::shared_pointer const & transport);

    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return std::tr1::shared_ptr<ChannelRequest>(); }

    virtual ~ServerGetFieldRequesterImpl() {}
    virtual void getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr const & field) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
private:
    bool done;
    epics::pvData::Status _status;
    epics::pvData::FieldConstPtr _field;
};

class ServerGetFieldHandlerTransportSender : public TransportSender
{
public:
    ServerGetFieldHandlerTransportSender(const pvAccessID ioid,const epics::pvData::Status& status, Transport::shared_pointer const & /*transport*/):
        _ioid(ioid), _status(status) {

    }
    virtual ~ServerGetFieldHandlerTransportSender() {}

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL {
        control->startMessage((epics::pvData::int8)CMD_GET_FIELD, sizeof(epics::pvData::int32)/sizeof(epics::pvData::int8));
        buffer->putInt(_ioid);
        _status.serialize(buffer, control);
    }

private:
    const pvAccessID _ioid;
    const epics::pvData::Status _status;
};



/****************************************************************************************/
/**
 * RPC handler.
 */
class ServerRPCHandler : public AbstractServerResponseHandler
{
public:
    ServerRPCHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "RPC request") {
    }
    virtual ~ServerRPCHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
};

class ServerChannelRPCRequesterImpl :
    public BaseChannelRequester,
    public ChannelRPCRequester,
    public std::tr1::enable_shared_from_this<ServerChannelRPCRequesterImpl>
{
public:
    typedef std::tr1::shared_ptr<ServerChannelRPCRequesterImpl> shared_pointer;
    typedef std::tr1::shared_ptr<const ServerChannelRPCRequesterImpl> const_shared_pointer;
protected:
    ServerChannelRPCRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                  std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
                                  Transport::shared_pointer const & transport);
    void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
public:
    static ChannelRPCRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
            std::tr1::shared_ptr<ServerChannel> const & channel, const pvAccessID ioid,
            Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
    virtual ~ServerChannelRPCRequesterImpl() {}

    virtual void channelRPCConnect(const epics::pvData::Status& status, ChannelRPC::shared_pointer const & channelRPC) OVERRIDE FINAL;
    virtual void requestDone(const epics::pvData::Status& status,
                             ChannelRPC::shared_pointer const & channelRPC,
                             epics::pvData::PVStructure::shared_pointer const & pvResponse) OVERRIDE FINAL;
    virtual void destroy() OVERRIDE FINAL;
    /**
     * @return the channelRPC
     */
    ChannelRPC::shared_pointer getChannelRPC();
    virtual std::tr1::shared_ptr<ChannelRequest> getOperation() OVERRIDE FINAL { return getChannelRPC(); }

    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) OVERRIDE FINAL;
private:
    // Note: this forms a reference loop, which is broken in destroy()
    ChannelRPC::shared_pointer _channelRPC;
    epics::pvData::PVStructure::shared_pointer _pvResponse;
    epics::pvData::Status _status;
};


/**
 * PVAS request handler - main handler which dispatches requests to appropriate handlers.
 */
class ServerResponseHandler : public ResponseHandler {
public:
    ServerResponseHandler(ServerContextImpl::shared_pointer const & context);

    virtual ~ServerResponseHandler() {}

    virtual void handleResponse(osiSockAddr* responseFrom,
                                Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                                std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer) OVERRIDE FINAL;
private:
    ServerBadResponse handle_bad;

    ServerNoopResponse handle_beacon;
    ServerConnectionValidationHandler handle_validation;
    ServerEchoHandler handle_echo;
    ServerSearchHandler handle_search;
    AuthNZHandler handle_authnz;
    ServerCreateChannelHandler handle_create;
    ServerDestroyChannelHandler handle_destroy;
    ServerGetHandler handle_get;
    ServerPutHandler handle_put;
    ServerPutGetHandler handle_putget;
    ServerMonitorHandler handle_monitor;
    ServerArrayHandler handle_array;
    ServerDestroyRequestHandler handle_close;
    ServerProcessHandler handle_process;
    ServerGetFieldHandler handle_getfield;
    ServerRPCHandler handle_rpc;
    ServerCancelRequestHandler handle_cancel;
    /**
     * Table of response handlers for each command ID.
     */
    std::vector<ResponseHandler*> m_handlerTable;

};

}
}

#endif /* RESPONSEHANDLERS_H_ */
