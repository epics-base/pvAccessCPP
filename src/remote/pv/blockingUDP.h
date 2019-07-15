/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef BLOCKINGUDP_H_
#define BLOCKINGUDP_H_

#ifdef epicsExportSharedSymbols
#   define blockingUDPEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <shareLib.h>
#include <osiSock.h>
#include <epicsThread.h>

#include <pv/noDefaultMethods.h>
#include <pv/byteBuffer.h>
#include <pv/lock.h>
#include <pv/event.h>
#include <pv/pvIntrospect.h>

#ifdef blockingUDPEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef blockingUDPEpicsExportSharedSymbols
#endif

#include <shareLib.h>

#include <pv/remote.h>
#include <pv/pvaConstants.h>
#include <pv/inetAddressUtil.h>

namespace epics {
namespace pvAccess {

class ClientChannelImpl;
class BlockingUDPConnector;

enum InetAddressType { inetAddressType_all, inetAddressType_unicast, inetAddressType_broadcast_multicast };

class BlockingUDPTransport :
    public Transport,
    public TransportSendControl,
    public epicsThreadRunable
{
    EPICS_NOT_COPYABLE(BlockingUDPTransport)
public:
    POINTER_DEFINITIONS(BlockingUDPTransport);

    static size_t num_instances;

private:
    std::tr1::weak_ptr<BlockingUDPTransport> internal_this;
    friend class BlockingUDPConnector;
    BlockingUDPTransport(bool serverFlag,
                         ResponseHandler::shared_pointer const & responseHandler,
                         SOCKET channel, osiSockAddr &bindAddress,
                         short remoteTransportRevision);
public:

    virtual ~BlockingUDPTransport();

    virtual bool isClosed() OVERRIDE FINAL {
        return _closed.get();
    }

    virtual const osiSockAddr& getRemoteAddress() const OVERRIDE FINAL {
        return _remoteAddress;
    }

    virtual const std::string& getRemoteName() const OVERRIDE FINAL {
        return _remoteName;
    }

    virtual std::string getType() const OVERRIDE FINAL {
        return std::string("udp");
    }

    virtual std::size_t getReceiveBufferSize() const OVERRIDE FINAL {
        return _receiveBuffer.getSize();
    }

    virtual epics::pvData::int16 getPriority() const OVERRIDE FINAL {
        return PVA_DEFAULT_PRIORITY;
    }

    virtual void setRemoteTransportReceiveBufferSize(
        std::size_t /*receiveBufferSize*/) OVERRIDE FINAL {
        // noop for UDP (limited by 64k; MAX_UDP_SEND for PVA)
    }

    virtual void setRemoteTransportSocketReceiveBufferSize(
        std::size_t /*socketReceiveBufferSize*/) OVERRIDE FINAL {
        // noop for UDP (limited by 64k; MAX_UDP_SEND for PVA)
    }

    virtual bool verify(epics::pvData::int32 /*timeoutMs*/) OVERRIDE FINAL {
        // noop
        return true;
    }

    virtual void verified(epics::pvData::Status const & /*status*/) OVERRIDE FINAL {
        // noop
    }

    virtual void authNZMessage(epics::pvData::PVStructure::shared_pointer const & data) OVERRIDE FINAL {
        // noop
    }

    // NOTE: this is not yet used for UDP
    virtual void setByteOrder(int byteOrder) OVERRIDE FINAL  {
        // called from receive thread... or before processing
        _receiveBuffer.setEndianess(byteOrder);

        // sync?!
        _sendBuffer.setEndianess(byteOrder);
    }

    virtual void enqueueSendRequest(TransportSender::shared_pointer const & sender) OVERRIDE FINAL;

    virtual void flushSendQueue() OVERRIDE FINAL;

    void start();

    virtual void close() OVERRIDE FINAL;

    virtual void ensureData(std::size_t size) OVERRIDE FINAL;

    virtual void alignData(std::size_t alignment) OVERRIDE FINAL {
        _receiveBuffer.align(alignment);
    }

    virtual bool directSerialize(epics::pvData::ByteBuffer* /*existingBuffer*/, const char* /*toSerialize*/,
                                 std::size_t /*elementCount*/, std::size_t /*elementSize*/) OVERRIDE FINAL
    {
        return false;
    }

    virtual bool directDeserialize(epics::pvData::ByteBuffer* /*existingBuffer*/, char* /*deserializeTo*/,
                                   std::size_t /*elementCount*/, std::size_t /*elementSize*/) OVERRIDE FINAL
    {
        return false;
    }

    virtual void startMessage(epics::pvData::int8 command, std::size_t ensureCapacity, epics::pvData::int32 payloadSize = 0) OVERRIDE FINAL;
    virtual void endMessage() OVERRIDE FINAL;

    virtual void flush(bool /*lastMessageCompleted*/) OVERRIDE FINAL {
        // noop since all UDP requests are sent immediately
    }

    virtual void setRecipient(const osiSockAddr& sendTo) OVERRIDE FINAL {
        _sendToEnabled = true;
        _sendTo = sendTo;
    }

    void setLocalMulticastAddress(const osiSockAddr& sendTo) {
        _localMulticastAddressEnabled = true;
        _localMulticastAddress = sendTo;
    }

    bool hasLocalMulticastAddress() const {
        return _localMulticastAddressEnabled;
    }

    const osiSockAddr& getLocalMulticastAddress() const {
        return _localMulticastAddress;
    }

    virtual void flushSerializeBuffer() OVERRIDE FINAL {
        // noop
    }

    virtual void ensureBuffer(std::size_t /*size*/) OVERRIDE FINAL {
        // noop
    }

    virtual void alignBuffer(std::size_t alignment) OVERRIDE FINAL {
        _sendBuffer.align(alignment);
    }

    virtual void cachedSerialize(
        const std::tr1::shared_ptr<const epics::pvData::Field>& field, epics::pvData::ByteBuffer* buffer) OVERRIDE FINAL
    {
        // no cache
        field->serialize(buffer, this);
    }

    virtual std::tr1::shared_ptr<const epics::pvData::Field>
    cachedDeserialize(epics::pvData::ByteBuffer* buffer) OVERRIDE FINAL
    {
        // no cache
        // TODO
        return epics::pvData::getFieldCreate()->deserialize(buffer, this);
    }

    virtual bool acquire(std::tr1::shared_ptr<ClientChannelImpl> const & /*client*/) OVERRIDE FINAL
    {
        return false;
    }

    virtual void release(pvAccessID /*clientId*/) OVERRIDE FINAL {}

    /**
     * Set ignore list.
     * @param address list of ignored addresses.
     */
    void setIgnoredAddresses(const InetAddrVector& addresses) {
        _ignoredAddresses = addresses;
    }

    /**
     * Get list of ignored addresses.
     * @return ignored addresses.
     */
    const InetAddrVector& getIgnoredAddresses() const {
        return _ignoredAddresses;
    }

    /**
     * Set tapped NIF list.
     * @param NIF address list to tap.
     */
    void setTappedNIF(const InetAddrVector& addresses) {
        _tappedNIF = addresses;
    }

    /**
     * Get list of tapped NIF addresses.
     * @return tapped NIF addresses.
     */
    const InetAddrVector& getTappedNIF() const {
        return _tappedNIF;
    }

    bool send(const char* buffer, size_t length, const osiSockAddr& address);

    bool send(epics::pvData::ByteBuffer* buffer, const osiSockAddr& address);

    bool send(epics::pvData::ByteBuffer* buffer, InetAddressType target = inetAddressType_all);

    /**
     * Get list of send addresses.
     * @return send addresses.
     */
    const InetAddrVector& getSendAddresses() {
        return _sendAddresses;
    }

    /**
     * Get bind address.
     * @return bind address.
     */
    const osiSockAddr* getBindAddress() const {
        return &_bindAddress;
    }

    bool isBroadcastAddress(const osiSockAddr* address, const InetAddrVector& broadcastAddresses)
    {
        for (size_t i = 0; i < broadcastAddresses.size(); i++)
            if (broadcastAddresses[i].ia.sin_addr.s_addr == address->ia.sin_addr.s_addr)
                return true;
        return false;
    }

    // consumes arguments
    void setSendAddresses(InetAddrVector& addresses, std::vector<bool>& address_types) {
        _sendAddresses.swap(addresses);
        _isSendAddressUnicast.swap(address_types);
    }

    void join(const osiSockAddr & mcastAddr, const osiSockAddr & nifAddr);

    void setMutlicastNIF(const osiSockAddr & nifAddr, bool loopback);

protected:
    AtomicBoolean _closed;

    /**
     * Response handler.
     */
    ResponseHandler::shared_pointer _responseHandler;

    virtual void run() OVERRIDE FINAL;

private:
    bool processBuffer(Transport::shared_pointer const & transport, osiSockAddr& fromAddress, epics::pvData::ByteBuffer* receiveBuffer);

    void close(bool waitForThreadToComplete);

    // Context only used for logging in this class

    /**
     * Corresponding channel.
     */
    SOCKET _channel;

    /* When provided, this transport is used for replies (passed to handler)
     * instead of *this.  This feature is used in the situation where broadcast
     * traffic is received on one socket, but a different socket must be used
     * for unicast replies.
     *
    Transport::shared_pointer _replyTransport;
    */

    /**
     * Bind address.
     */
    osiSockAddr _bindAddress;

    /**
     * Remote address.
     */
    osiSockAddr _remoteAddress;
    std::string _remoteName;

    /**
     * Send addresses.
     */
    InetAddrVector _sendAddresses;

    std::vector<bool> _isSendAddressUnicast;

    /**
     * Ignore addresses.
     */
    InetAddrVector _ignoredAddresses;

    /**
     * Tapped NIF addresses.
     */
    InetAddrVector _tappedNIF;

    /**
     * Send address.
     */
    osiSockAddr _sendTo;
    bool _sendToEnabled;

    /**
     * Local multicast address.
     */
    osiSockAddr _localMulticastAddress;
    bool _localMulticastAddressEnabled;

    /**
     * Receive buffer.
     */
    epics::pvData::ByteBuffer _receiveBuffer;

    /**
     * Send buffer.
     */
    epics::pvData::ByteBuffer _sendBuffer;

    /**
     * Last message start position.
     */
    int _lastMessageStartPosition;

    /**
     * Used for process sync.
     */
    epics::pvData::Mutex _mutex;
    epics::pvData::Mutex _sendMutex;

    /**
     * Thread ID
     */
    epics::auto_ptr<epicsThread> _thread;

    epics::pvData::int8 _clientServerWithEndianFlag;

};

class BlockingUDPConnector{
public:
    POINTER_DEFINITIONS(BlockingUDPConnector);

    BlockingUDPConnector(bool serverFlag) :_serverFlag(serverFlag) {}

    /**
     * NOTE: transport client is ignored for broadcast (UDP).
     */
    BlockingUDPTransport::shared_pointer connect(
            ResponseHandler::shared_pointer const & responseHandler,
            osiSockAddr& bindAddress,
            epics::pvData::int8 transportRevision);

private:

    /**
     * Client/server flag.
     */
    bool _serverFlag;

    EPICS_NOT_COPYABLE(BlockingUDPConnector)
};

typedef std::vector<BlockingUDPTransport::shared_pointer> BlockingUDPTransportVector;

void initializeUDPTransports(
    bool serverFlag,
    BlockingUDPTransportVector& udpTransports,
    const IfaceNodeVector& ifaceList,
    const ResponseHandler::shared_pointer& responseHandler,
    BlockingUDPTransport::shared_pointer& sendTransport,
    epics::pvData::int32& listenPort,
    bool autoAddressList,
    const std::string& addressList,
    const std::string& ignoreAddressList);


}
}

#endif /* BLOCKINGUDP_H_ */
