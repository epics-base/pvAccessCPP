/*
 * remote.h
 *
 *  Created on: Dec 21, 2010
 *      Author: user
 */

#ifndef REMOTE_H_
#define REMOTE_H_

#include "caConstants.h"
#include "transportRegistry.h"
#include "introspectionRegistry.h"
#include "configuration.h"

#include <serialize.h>
#include <pvType.h>
#include <byteBuffer.h>
#include <timer.h>
#include <pvData.h>

#include <osiSock.h>
#include <osdSock.h>

namespace epics {
    namespace pvAccess {

        class TransportRegistry;

        enum ProtocolType {
            TCP, UDP, SSL
        };

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

        typedef int32 pvAccessID;

        enum MessageCommands {
            CMD_BEACON = 0, CMD_CONNECTION_VALIDATION = 1, CMD_ECHO = 2,
            CMD_SEARCH = 3, CMD_SEARCH_RESPONSE = 4,
            CMD_INTROSPECTION_SEARCH = 5, CMD_CREATE_CHANNEL = 7,
            CMD_DESTROY_CHANNEL = 8, CMD_GET = 10, CMD_PUT = 11,
            CMD_PUT_GET = 12, CMD_MONITOR = 13, CMD_ARRAY = 14,
            CMD_CANCEL_REQUEST = 15, CMD_PROCESS = 16, CMD_GET_FIELD = 17,
            CMD_MESSAGE = 18, CMD_MULTIPLE_DATA = 19, CMD_RPC = 20,
        };

        /**
         * Interface defining transport send control.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         */
        class TransportSendControl : public epics::pvData::SerializableControl {
        public:
            virtual ~TransportSendControl() {
            }

            virtual void startMessage(int8 command, int ensureCapacity) =0;
            virtual void endMessage() =0;

            virtual void flush(bool lastMessageCompleted) =0;

            virtual void setRecipient(const osiSockAddr& sendTo) =0;
        };
        
        /**
         * Reference counting instance.
         */
        class ReferenceCountingInstance {
        public:
            virtual void acquire() =0;
            virtual void release() =0;
        };

        /**
         * Interface defining transport sender (instance sending data over transport).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: TransportSender.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class TransportSender : virtual public ReferenceCountingInstance {
        public:
            virtual ~TransportSender() {
            }

            /**
             * Called by transport.
             * By this call transport gives callee ownership over the buffer.
             * Calls on <code>TransportSendControl</code> instance must be made from
             * calling thread. Moreover, ownership is valid only for the time of call
             * of this method.
             * NOTE: these limitations allows efficient implementation.
             */
            virtual void send(epics::pvData::ByteBuffer* buffer,
                    TransportSendControl* control) =0;

            virtual void lock() =0;
            virtual void unlock() =0;
        };

        /**
         * Interface defining transport (connection).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: Transport.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class Transport : public epics::pvData::DeserializableControl {
        public:
            virtual ~Transport() {
            }

            /**
             * Get remote address.
             * @return remote address.
             */
            virtual const osiSockAddr* getRemoteAddress() const =0;

            /**
             * Get protocol type (tcp, udp, ssl, etc.).
             * @return protocol type.
             */
            virtual const String getType() const =0;

            /**
             * Get context transport is living in.
             * @return context transport is living in.
             */
            //public Context getContext();

            /**
             * Transport protocol major revision.
             * @return protocol major revision.
             */
            virtual int8 getMajorRevision() const {
                return CA_MAJOR_PROTOCOL_REVISION;
            }

            /**
             * Transport protocol minor revision.
             * @return protocol minor revision.
             */
            virtual int8 getMinorRevision() const {
                return CA_MINOR_PROTOCOL_REVISION;
            }

            /**
             * Get receive buffer size.
             * @return receive buffer size.
             */
            virtual int getReceiveBufferSize() const =0;

            /**
             * Get socket receive buffer size.
             * @return socket receive buffer size.
             */
            virtual int getSocketReceiveBufferSize() const =0;

            /**
             * Transport priority.
             * @return protocol priority.
             */
            virtual int16 getPriority() const =0;

            /**
             * Set remote transport protocol minor revision.
             * @param minor protocol minor revision.
             */
            virtual void setRemoteMinorRevision(int8 minor) =0;

            /**
             * Set remote transport receive buffer size.
             * @param receiveBufferSize receive buffer size.
             */
            virtual void setRemoteTransportReceiveBufferSize(
                    int receiveBufferSize) =0;

            /**
             * Set remote transport socket receive buffer size.
             * @param socketReceiveBufferSize remote socket receive buffer size.
             */
            virtual void setRemoteTransportSocketReceiveBufferSize(
                    int socketReceiveBufferSize) =0;

            /**
             * Notification transport that is still alive.
             */
            virtual void aliveNotification() =0;

            /**
             * Notification that transport has changed.
             */
            virtual void changedTransport() =0;

            /**
             * Get introspection registry for transport.
             * @return <code>IntrospectionRegistry</code> instance.
             */
            virtual IntrospectionRegistry* getIntrospectionRegistry() =0;

            /**
             * Close transport.
             * @param force flag indicating force-full (e.g. remote disconnect) close.
             */
            virtual void close(bool force) =0;

            /**
             * Check connection status.
             * @return <code>true</code> if connected.
             */
            virtual bool isClosed() =0;

            /**
             * Get transport verification status.
             * @return verification flag.
             */
            virtual bool isVerified() =0;

            /**
             * Notify transport that it is has been verified.
             */
            virtual void verified() =0;

            /**
             * Enqueue send request.
             * @param sender
             */
            virtual void enqueueSendRequest(TransportSender* sender) =0;

        };

        class Channel;

        /**
         * Not public IF, used by Transports, etc.
         */
        class Context : public ReferenceCountingInstance {
        public:
            virtual ~Context() {
            }
            /**
             * Get timer.
             * @return timer.
             */
            virtual Timer* getTimer() = 0;

            /**
             * Get transport (virtual circuit) registry.
             * @return transport (virtual circuit) registry.
             */
            virtual TransportRegistry* getTransportRegistry() = 0;

            virtual Channel* getChannel(pvAccessID id) = 0;

            virtual Transport* getSearchTransport() = 0;

            virtual Configuration* getConfiguration() = 0;

        };

        /**
         * Interface defining response handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ResponseHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class ResponseHandler {
        public:
            virtual ~ResponseHandler() {}
            
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
            handleResponse(osiSockAddr* responseFrom, Transport* transport,
                    int8 version, int8 command, int payloadSize,
                    epics::pvData::ByteBuffer* payloadBuffer) =0;
        };

        /**
         * Base (abstract) channel access response handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: AbstractResponseHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class AbstractResponseHandler : public ResponseHandler {
        public:
            /**
             * @param description
             */
            AbstractResponseHandler(Context* context, String description) :
                _description(description), 
                _debug(context->getConfiguration()->getPropertyAsBoolean("PVACCESS_DEBUG", false)) {
            }

            virtual ~AbstractResponseHandler() {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        protected:
            /**
             * Response hanlder description.
             */
            String _description;

            /**
             * Debug flag.
             */
            bool _debug;
        };

        /**
         * Client (user) of the transport.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: TransportClient.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class TransportClient : public ReferenceCountingInstance {
        public:
            virtual ~TransportClient() {
            }

            /**
             * Notification of unresponsive transport (e.g. no heartbeat detected) .
             */
            virtual void transportUnresponsive() =0;

            /**
             * Notification of responsive transport (e.g. heartbeat detected again),
             * called to discard <code>transportUnresponsive</code> notification.
             * @param transport responsive transport.
             */
            virtual void transportResponsive(Transport* transport) =0;

            /**
             * Notification of network change (server restarted).
             */
            virtual void transportChanged() =0;

            /**
             * Notification of forcefully closed transport.
             */
            virtual void transportClosed() =0;

        };

        /**
         * Interface defining socket connector (Connector-Transport pattern).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: Connector.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class Connector {
        public:
            virtual ~Connector() {
            }

            /**
             * Connect.
             * @param[in] client    client requesting connection (transport).
             * @param[in] address           address of the server.
             * @param[in] responseHandler   reponse handler.
             * @param[in] transportRevision transport revision to be used.
             * @param[in] priority process priority.
             * @return transport instance.
             * @throws ConnectionException
             */
            virtual Transport* connect(TransportClient* client,
                    ResponseHandler* responseHandler, osiSockAddr& address,
                    short transportRevision, int16 priority) =0;

        };

        /**
         * Interface defining reference counting transport IF.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ReferenceCountingTransport.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class ReferenceCountingTransport {
        public:
            virtual ~ReferenceCountingTransport() {
            }

            /**
             * Acquires transport.
             * @param client client (channel) acquiring the transport
             * @return <code>true</code> if transport was granted, <code>false</code> otherwise.
             */
            virtual bool acquire(TransportClient* client) =0;

            /**
             * Releases transport.
             * @param client client (channel) releasing the transport
             */
            virtual void release(TransportClient* client) =0;
        };

        class ServerChannel {
        public:
            virtual ~ServerChannel() {
            }
            /**
             * Get channel SID.
             * @return channel SID.
             */
            virtual pvAccessID getSID() =0;

            /**
             * Destroy server channel.
             * This method MUST BE called if overriden.
             */
            virtual void destroy() =0;
        };

        /**
         * Interface defining a transport that hosts channels.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ChannelHostingTransport.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class ChannelHostingTransport {
        public:
            virtual ~ChannelHostingTransport() {
            }

            /**
             * Get security token.
             * @return security token, can be <code>null</code>.
             */
            virtual epics::pvData::PVField* getSecurityToken() =0;

            /**
             * Preallocate new channel SID.
             * @return new channel server id (SID).
             */
            virtual pvAccessID preallocateChannelSID() =0;

            /**
             * De-preallocate new channel SID.
             * @param sid preallocated channel SID.
             */
            virtual void depreallocateChannelSID(pvAccessID sid) =0;

            /**
             * Register a new channel.
             * @param sid preallocated channel SID.
             * @param channel channel to register.
             */
            virtual void
            registerChannel(pvAccessID sid, ServerChannel* channel) =0;

            /**
             * Unregister a new channel (and deallocates its handle).
             * @param sid SID
             */
            virtual void unregisterChannel(pvAccessID sid) =0;

            /**
             * Get channel by its SID.
             * @param sid channel SID
             * @return channel with given SID, <code>null</code> otherwise
             */
            virtual ServerChannel* getChannel(pvAccessID sid) =0;

            /**
             * Get channel count.
             * @return channel count.
             */
            virtual int getChannelCount() =0;
        };

        /**
         * A request that expects an response.
         * Responses identified by its I/O ID.
         * This interface needs to be extended (to provide method called on response).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         */
        class ResponseRequest : public ReferenceCountingInstance {
        public:
            virtual ~ResponseRequest() {}

            /**
             * Get I/O ID.
             * @return ioid
             */
            virtual pvAccessID getIOID() = 0;

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
            virtual void reportStatus(const epics::pvData::Status& status) = 0;

            /**
             * Get request requester.
             * @return request requester.
             */
            virtual epics::pvData::Requester* getRequester() = 0;
        };
        
        /**
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: DataResponse.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class DataResponse : public ResponseRequest {
        public:
            virtual ~DataResponse() {}

        	/**
        	 * Notification response.
        	 * @param transport
        	 * @param version
        	 * @param payloadBuffer
        	 */
        	virtual void response(Transport* transport, int8 version, ByteBuffer* payloadBuffer) = 0;
        
        }; 

        /**
         * A request that expects an response multiple responses.
         * Responses identified by its I/O ID. 
         * This interface needs to be extended (to provide method called on response).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: SubscriptionRequest.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class SubscriptionRequest /*: public ResponseRequest*/ {
        public:
            virtual ~SubscriptionRequest() {}
            
        	/**
        	 * Update (e.g. after some time of unresponsiveness) - report current value.
        	 */
        	virtual void updateSubscription() = 0;
        	
        	/**
        	 * Rescubscribe (e.g. when server was restarted)
        	 * @param transport new transport to be used.
        	 */
        	virtual void resubscribeSubscription(Transport* transport) = 0;
        };


    }
}

#endif /* REMOTE_H_ */
