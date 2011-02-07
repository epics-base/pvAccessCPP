/*
 * responseHandlers.h
 *
 *  Created on: Jan 5, 2011
 *      Author: user
 */

#ifndef RESPONSEHANDLERS_H_
#define RESPONSEHANDLERS_H_

#include "serverContext.h"
#include "remote.h"

namespace epics {
    namespace pvAccess {

        /**
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: AbstractServerResponseHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class AbstractServerResponseHandler : public AbstractResponseHandler {
        protected:
            ServerContextImpl* _context;
        public:
            /**
             * @param context
             * @param description
             */
            AbstractServerResponseHandler(ServerContextImpl* context, String description) : 
                AbstractResponseHandler(context, description), _context(context) {
            }

            virtual ~AbstractServerResponseHandler() {
            }
        };

        /**
         * Bad request handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: BadResponse.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class BadResponse : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            BadResponse(ServerContextImpl* context) :
                AbstractServerResponseHandler(context, "Bad request") {
            }

            virtual ~BadResponse() {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /**
         * CAS request handler - main handler which dispatches requests to appropriate handlers.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ServerResponseHandler.java,v 1.1 2010/05/03 14:45:48 mrkraimer Exp $
         */
        class ServerResponseHandler : public ResponseHandler {
        public:
            ServerResponseHandler(ServerContextImpl* context);

            virtual ~ServerResponseHandler();

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        private:
            static const int HANDLER_TABLE_LENGTH = 28;
            /**
             * Table of response handlers for each command ID.
             */
            ResponseHandler** _handlerTable;

        };

        /**
         * Connection validation message handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: ConnectionValidationHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class ConnectionValidationHandler : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            ConnectionValidationHandler(ServerContextImpl* context) :
                AbstractServerResponseHandler(context, "Connection validation") {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /**
         * NOOP response.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: NoopResponse.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class NoopResponse : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             * @param description
             */
            NoopResponse(ServerContextImpl* context, String description) :
                AbstractServerResponseHandler(context, description) {
            }
        };

        /**
         * Echo request handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: EchoHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class EchoHandler : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            EchoHandler(ServerContextImpl* context) :
                AbstractServerResponseHandler(context, "Echo request") {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };


        /**
         * Introspection search request handler.
         */
        class IntrospectionSearchHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	IntrospectionSearchHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Search request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /**
         * Introspection search request handler.
         */
        class ChannelFindRequesterImplObjectPool;
        class SearchHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	SearchHandler(ServerContextImpl* context);
        	~SearchHandler();

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        private:
        	ChannelProvider* _provider;
        	ChannelFindRequesterImplObjectPool* _objectPool;
        };


        class ChannelFindRequesterImpl: public ChannelFindRequester, public TransportSender
        {
        public:
        	ChannelFindRequesterImpl(ServerContextImpl* context, ChannelFindRequesterImplObjectPool* objectPool);
        	void clear();
        	ChannelFindRequesterImpl* set(int32 searchSequenceId, int32 cid, osiSockAddr* sendTo, boolean responseRequired);
        	void channelFindResult(epics::pvData::Status* status, ChannelFind* channelFind, boolean wasFound);
        	void lock();
        	void unlock();
            void acquire();
            void release();
        	void send(ByteBuffer* buffer, TransportSendControl* control);
        private:
        	int32 _searchSequenceId;
        	int32 _cid;
        	osiSockAddr* _sendTo;
        	boolean _responseRequired;
        	boolean _wasFound;
        	epics::pvData::Mutex _mutex;
        	ServerContextImpl* _context;
        	ChannelFindRequesterImplObjectPool* _objectPool;
        };

        class ChannelFindRequesterImplObjectPool
        {
        public:
        	ChannelFindRequesterImplObjectPool(ServerContextImpl* context);
        	ChannelFindRequesterImpl* get();
        	void put(ChannelFindRequesterImpl* element);

        private:
        	std::vector<ChannelFindRequesterImpl*> _elements;
        	epics::pvData::Mutex _mutex;
        	ServerContextImpl* _context;
        };

        /**
         * Create channel request handler.
         */
        class CreateChannelHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	CreateChannelHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Create channel request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        private:
        	void disconnect(Transport* transport);
        };

        class ServerChannelImpl;
        class ChannelRequesterImpl : public ChannelRequester, public TransportSender
        {
        public:
        	 ChannelRequesterImpl(Transport* transport, const String channelName, const int32 cid);
        	 void channelCreated(Status* const status, Channel* const channel);
        	 void channelStateChange(Channel* const c, const Channel::ConnectionState isConnected);
        	 String getRequesterName();
        	 void message(const String message, const epics::pvData::MessageType messageType);
        	 void lock();
        	 void unlock();
        	 void send(ByteBuffer* buffer, TransportSendControl* control);

        private:
        	 Transport* _transport;
        	 const String _channelName;
        	 const int32 _cid;
        	 Status* _status;
        	 Channel* _channel;
        	 epics::pvData::Mutex _mutex;
        	 void createChannelFailedResponse(ByteBuffer* buffer, TransportSendControl* control, Status* const status);
        };



    }
}

#endif /* RESPONSEHANDLERS_H_ */
