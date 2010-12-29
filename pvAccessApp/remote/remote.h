/*
 * remote.h
 *
 *  Created on: Dec 21, 2010
 *      Author: user
 */

#ifndef REMOTE_H_
#define REMOTE_H_

#include <serialize.h>
#include <pvType.h>
#include <byteBuffer.h>

#include <osiSock.h>
#include <osdSock.h>

namespace epics {
    namespace pvAccess {

        using namespace epics::pvData;

        enum ProtocolType {
            TCP, UDP, SSL
        };

        /**
         * Interface defining transport send control.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         */
        class TransportSendControl : public SerializableControl {
        public:
            virtual void startMessage(int8 command, int ensureCapacity) =0;
            virtual void endMessage() =0;

            virtual void flush(bool lastMessageCompleted) =0;

            virtual void setRecipient(const osiSockAddr* sendTo) =0;
        };

        /**
         * Interface defining transport sender (instance sending data over transport).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: TransportSender.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class TransportSender {
        public:
            /**
             * Called by transport.
             * By this call transport gives callee ownership over the buffer.
             * Calls on <code>TransportSendControl</code> instance must be made from
             * calling thread. Moreover, ownership is valid only for the time of call
             * of this method.
             * NOTE: these limitations allows efficient implementation.
             */
            virtual void
            send(ByteBuffer* buffer, TransportSendControl* control) =0;

            virtual void lock() =0;
            virtual void unlock() =0;
        };

        /**
         * Interface defining transport (connection).
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: Transport.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class Transport : public DeserializableControl {
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
            virtual int8 getMajorRevision() const =0;

            /**
             * Transport protocol minor revision.
             * @return protocol minor revision.
             */
            virtual int8 getMinorRevision() const =0;

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
            //virtual IntrospectionRegistry getIntrospectionRegistry() =0;

            /**
             * Close transport.
             * @param force flag indicating force-full (e.g. remote disconnect) close.
             */
            virtual void close(bool force) =0;

            /**
             * Check connection status.
             * @return <code>true</code> if connected.
             */
            virtual bool isClosed() const =0;

            /**
             * Get transport verification status.
             * @return verification flag.
             */
            virtual bool isVerified() const =0;

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

        /**
         * Interface defining response handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ResponseHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class ResponseHandler {
        public:
            /**
             * Handle response.
             * @param[in] responseFrom  remote address of the responder, <code>null</code> if unknown.
             * @param[in] transport response source transport.
             * @param[in] version message version.
             * @param[in] payloadSize size of this message data available in the <code>payloadBuffer</code>.
             * @param[in] payloadBuffer message payload data.
             *                      Note that this might not be the only message in the buffer.
             *                      Code must not manipulate buffer.
             */
            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, ByteBuffer* payloadBuffer) =0;
        };

        /**
         * Client (user) of the transport.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: TransportClient.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class TransportClient {
        public:
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
            virtual Transport* connect(TransportClient* client, ResponseHandler* responseHandler,
                    osiSockAddr* address, short transportRevision, short priority) =0;

        };

    }
}

#endif /* REMOTE_H_ */
