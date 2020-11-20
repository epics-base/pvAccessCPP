/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef REMOTE_H_
#define REMOTE_H_

#ifdef epicsExportSharedSymbols
#   define remoteEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <map>
#include <string>

#include <osiSock.h>

#include <pv/serialize.h>
#include <pv/pvType.h>
#include <pv/byteBuffer.h>
#include <pv/timer.h>
#include <pv/pvData.h>
#include <pv/sharedPtr.h>

#ifdef remoteEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#       undef remoteEpicsExportSharedSymbols
#endif

#include <pv/pvaConstants.h>
#include <pv/configuration.h>
#include <pv/fairQueue.h>
#include <pv/pvaDefs.h>

/// TODO only here because of the Lockable
#include <pv/pvAccess.h>

namespace epics {
namespace pvAccess {

class TransportRegistry;
class ClientChannelImpl;

enum QoS {
    /**
     * Default behavior.
     */
    QOS_DEFAULT = 0x00,
    /**
     * Require reply (acknowledgment for reliable operation).
     */
    QOS_REPLY_REQUIRED = 0x01,
    /**
     * Best-effort option (no reply).
     */
    QOS_BESY_EFFORT = 0x02,
    /**
     * Process option.
     */
    QOS_PROCESS = 0x04,
    /**
     * Initialize option.
     */
    QOS_INIT = 0x08,
    /**
     * Destroy option.
     */
    QOS_DESTROY = 0x10,
    /**
     * Share data option.
     */
    QOS_SHARE = 0x20,
    /**
     * Get.
     */
    QOS_GET = 0x40,
    /**
     * Get-put.
     */
    QOS_GET_PUT = 0x80
};

enum ApplicationCommands {
    CMD_BEACON = 0,
    CMD_CONNECTION_VALIDATION = 1,
    CMD_ECHO = 2,
    CMD_SEARCH = 3,
    CMD_SEARCH_RESPONSE = 4,
    CMD_AUTHNZ = 5,
    CMD_ACL_CHANGE = 6,
    CMD_CREATE_CHANNEL = 7,
    CMD_DESTROY_CHANNEL = 8,
    CMD_CONNECTION_VALIDATED = 9,
    CMD_GET = 10,
    CMD_PUT = 11,
    CMD_PUT_GET = 12,
    CMD_MONITOR = 13,
    CMD_ARRAY = 14,
    CMD_DESTROY_REQUEST = 15,
    CMD_PROCESS = 16,
    CMD_GET_FIELD = 17,
    CMD_MESSAGE = 18,
    CMD_MULTIPLE_DATA = 19,
    CMD_RPC = 20,
    CMD_CANCEL_REQUEST = 21,
    CMD_ORIGIN_TAG = 22
};

enum ControlCommands {
    CMD_SET_MARKER = 0,
    CMD_ACK_MARKER = 1,
    CMD_SET_ENDIANESS = 2
};

void hackAroundRTEMSSocketInterrupt();

/**
 * Interface defining transport send control.
 */
class TransportSendControl : public epics::pvData::SerializableControl {
public:
    POINTER_DEFINITIONS(TransportSendControl);

    virtual ~TransportSendControl() {}

    virtual void startMessage(epics::pvData::int8 command, std::size_t ensureCapacity, epics::pvData::int32 payloadSize = 0) = 0;
    virtual void endMessage() = 0;

    virtual void flush(bool lastMessageCompleted) = 0;

    virtual void setRecipient(osiSockAddr const & sendTo) = 0;
};

/**
 * Interface defining transport sender (instance sending data over transport).
 */
class TransportSender : public Lockable, public fair_queue<TransportSender>::entry {
public:
    POINTER_DEFINITIONS(TransportSender);

    TransportSender() :bytesTX(0u), bytesRX(0u) {}
    virtual ~TransportSender() {}

    /**
     * Called by transport.
     * By this call transport gives callee ownership over the buffer.
     * Calls on <code>TransportSendControl</code> instance must be made from
     * calling thread. Moreover, ownership is valid only for the time of call
     * of this method.
     * NOTE: these limitations allow efficient implementation.
     */
    virtual void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) = 0;

    size_t bytesTX;
    size_t bytesRX;
};

class ClientChannelImpl;
class SecuritySession;

/**
 * Interface defining transport (connection).
 */
class epicsShareClass Transport : public epics::pvData::DeserializableControl {
public:
    POINTER_DEFINITIONS(Transport);

    static size_t num_instances;

    Transport();
    virtual ~Transport();

    /**
     * Acquires transport.
     * @param client client (channel) acquiring the transport
     * @return <code>true</code> if transport was granted, <code>false</code> otherwise.
     */
    virtual bool acquire(std::tr1::shared_ptr<ClientChannelImpl> const & client) = 0;

    /**
     * Releases transport.
     * @param client client (channel) releasing the transport
     */
    virtual void release(pvAccessID clientId) = 0;

    /**
     * Get protocol type (tcp, udp, ssl, etc.).
     * @return protocol type.
     */
    virtual std::string getType() const = 0;

    virtual const osiSockAddr& getRemoteAddress() const = 0;

    virtual const std::string& getRemoteName() const = 0;

    // TODO getContext?

    /**
     * Get receive buffer size.
     * @return receive buffer size.
     */
    virtual std::size_t getReceiveBufferSize() const = 0;

    /**
     * Transport priority.
     * @return protocol priority.
     */
    virtual epics::pvData::int16 getPriority() const = 0;

    /**
     * Set remote transport receive buffer size.
     * @param receiveBufferSize receive buffer size.
     */
    virtual void setRemoteTransportReceiveBufferSize(std::size_t receiveBufferSize) = 0;

    /**
     * Set remote transport socket receive buffer size.
     * @param socketReceiveBufferSize remote socket receive buffer size.
     */
    virtual void setRemoteTransportSocketReceiveBufferSize(std::size_t socketReceiveBufferSize) = 0;

    /**
     * Set byte order.
     * @param byteOrder byte order to set.
     */
    // TODO enum
    virtual void setByteOrder(int byteOrder) = 0;

    /**
     * Enqueue send request.
     * @param sender
     */
    virtual void enqueueSendRequest(TransportSender::shared_pointer const & sender) = 0;

    /**
     * Flush send queue (sent messages).
     */
    virtual void flushSendQueue() = 0;

    /**
     * Notify transport that it is has been verified.
     * @param status vefification status;
     */
    virtual void verified(epics::pvData::Status const & status) = 0;

    /**
     * Waits (if needed) until transport is verified, i.e. verified() method is being called.
     * @param timeoutMs timeout to wait for verification, infinite if 0.
     */
    virtual bool verify(epics::pvData::int32 timeoutMs) = 0;

    /**
     * Close transport.
     */
    virtual void close() = 0;

    //! Call after close() to wait for any worker threads to exit
    virtual void waitJoin() {}

    /**
     * Check connection status.
     * @return <code>true</code> if connected.
     */
    virtual bool isClosed() = 0;

    /**
     * Pass data to the active security plug-in session.
     * @param data the data (any data), can be <code>null</code>.
     */
    virtual void authNZMessage(epics::pvData::PVStructure::shared_pointer const & data) = 0;

    size_t _totalBytesSent;
    size_t _totalBytesRecv;
};

class Channel;
class SecurityPlugin;
class AuthenticationRegistry;

/**
 * Not public IF, used by Transports, etc.
 */
class Context {
public:
    POINTER_DEFINITIONS(Context);

    virtual ~Context() {}

    virtual epics::pvData::Timer::shared_pointer getTimer() = 0;

    virtual TransportRegistry* getTransportRegistry() = 0;




    virtual Configuration::const_shared_pointer getConfiguration() = 0;

    ///
    /// due to ClientContextImpl
    ///

    virtual void newServerDetected() = 0;

    virtual std::tr1::shared_ptr<Channel> getChannel(pvAccessID id) = 0;
    virtual Transport::shared_pointer getSearchTransport() = 0;
};

/**
 * Interface defining response handler.
 */
class ResponseHandler {
public:
    POINTER_DEFINITIONS(ResponseHandler);

    static size_t num_instances;

    ResponseHandler(Context* context, const std::string& description);
    virtual ~ResponseHandler();

    /**
     * Handle response.
     * @param[in] responseFrom  remote address of the responder, <code>0</code> if unknown.
     * @param[in] transport response source transport.
     * @param[in] version message version.
     * @param[in] payloadSize size of this message data available in the <code>payloadBuffer</code>.
     * @param[in] payloadBuffer message payload data.
     *                      Note that this might not be the only message in the buffer.
     *                      Code must not manipulate buffer.
     */
    virtual void
    handleResponse(osiSockAddr* responseFrom, Transport::shared_pointer const & transport,
                   epics::pvData::int8 version, epics::pvData::int8 command, std::size_t payloadSize,
                   epics::pvData::ByteBuffer* payloadBuffer);

protected:
    /**
     * Response hanlder description.
     */
    std::string _description;

    /**
     * Debug flag.
     */
    epics::pvData::int32 _debugLevel;
};

/**
 * A request that expects an response.
 * Responses identified by its I/O ID.
 */
class ResponseRequest : public TransportSender {
public:
    POINTER_DEFINITIONS(ResponseRequest);

    virtual ~ResponseRequest() {}

    /**
     * Get I/O ID.
     * @return ioid
     */
    virtual pvAccessID getIOID() const = 0;

    /**
     * Timeout notification.
     */
    virtual void timeout() = 0;

    /**
     * Cancel response request (always to be called to complete/destroy).
     */
    virtual void cancel() = 0;

    /**
     * Report status to clients (e.g. disconnected).
     * @param status to report.
     */
    virtual void reportStatus(Channel::ConnectionState status) = 0;

    /**
     * used by MessageHandler and reportChannelStateChange().
     *
     * May return NULL
     */
    virtual std::tr1::shared_ptr<ChannelBaseRequester> getRequester() = 0;

    /**
     * Notification response.
     * @param transport
     * @param version
     * @param payloadBuffer
     */
    virtual void response(Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::ByteBuffer* payloadBuffer) = 0;

};


}
}

#endif /* REMOTE_H_ */
