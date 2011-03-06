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
#include "serverChannelImpl.h"
#include "baseChannelRequester.h"
#include "referencedTransportSender.h"

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
        class ServerBadResponse : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            ServerBadResponse(ServerContextImpl* context) :
                AbstractServerResponseHandler(context, "Bad request") {
            }

            virtual ~ServerBadResponse() {
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
             * Bad response handlers.
             */
            ServerBadResponse *_badResponse;
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
        class ServerConnectionValidationHandler : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            ServerConnectionValidationHandler(ServerContextImpl* context) :
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
        class ServerNoopResponse : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             * @param description
             */
            ServerNoopResponse(ServerContextImpl* context, String description) :
                AbstractServerResponseHandler(context, description) {
            }
        };

        /**
         * Echo request handler.
         * @author <a href="mailto:matej.sekoranjaATcosylab.com">Matej Sekoranja</a>
         * @version $Id: EchoHandler.java,v 1.1 2010/05/03 14:45:39 mrkraimer Exp $
         */
        class ServerEchoHandler : public AbstractServerResponseHandler {
        public:
            /**
             * @param context
             */
            ServerEchoHandler(ServerContextImpl* context) :
                AbstractServerResponseHandler(context, "Echo request") {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class EchoTransportSender : public ReferencedTransportSender {
        public:
        	EchoTransportSender(osiSockAddr* echoFrom) {
        		memcpy(&_echoFrom, echoFrom, sizeof(osiSockAddr));
        	}

        	virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
        		control->startMessage(CMD_ECHO, 0);
        		control->setRecipient(_echoFrom);
        		// TODO content
        	}

        	virtual void lock() {
        	}

        	virtual void unlock() {
        	}

        private:
        	osiSockAddr _echoFrom;

        	virtual ~EchoTransportSender() {
        	}
        };

        /**
         * Introspection search request handler.
         */
        class ServerIntrospectionSearchHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerIntrospectionSearchHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Search request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /****************************************************************************************/
        /**
         * Search channel request handler.
         */
        class ServerChannelFindRequesterImplObjectPool;
        class ServerSearchHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerSearchHandler(ServerContextImpl* context);
        	~ServerSearchHandler();

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        private:
        	ChannelProvider* _provider;
        	ServerChannelFindRequesterImplObjectPool* _objectPool;
        };


        class ServerChannelFindRequesterImpl: public ChannelFindRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelFindRequesterImpl(ServerContextImpl* context, ServerChannelFindRequesterImplObjectPool* objectPool);
        	void clear();
        	ServerChannelFindRequesterImpl* set(int32 searchSequenceId, int32 cid, osiSockAddr* sendTo, boolean responseRequired);
        	void channelFindResult(const epics::pvData::Status& status, ChannelFind* channelFind, boolean wasFound);
        	void lock();
        	void unlock();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	int32 _searchSequenceId;
        	int32 _cid;
        	osiSockAddr* _sendTo;
        	boolean _responseRequired;
        	boolean _wasFound;
        	ServerContextImpl* _context;
        	epics::pvData::Mutex _mutex;
        	ServerChannelFindRequesterImplObjectPool* _objectPool;
        };

        class ServerChannelFindRequesterImplObjectPool
        {
        public:
        	ServerChannelFindRequesterImplObjectPool(ServerContextImpl* context);
        	~ServerChannelFindRequesterImplObjectPool();
        	ServerChannelFindRequesterImpl* get();
        	void put(ServerChannelFindRequesterImpl* element);

        private:
        	std::vector<ServerChannelFindRequesterImpl*> _elements;
        	ServerContextImpl* _context;
        	epics::pvData::Mutex _mutex;
        };
        /****************************************************************************************/
        /**
         * Create channel request handler.
         */
        class ServerCreateChannelHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerCreateChannelHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Create channel request") {
        		_provider = context->getChannelProvider();
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        private:
        	/**
        	 * Disconnect.
        	 */
        	void disconnect(Transport* transport);
			ChannelProvider* _provider;
        };

        class ServerChannelRequesterImpl : public ChannelRequester, public ReferencedTransportSender
        {
        public:
        	 ServerChannelRequesterImpl(Transport* transport, const String channelName, const pvAccessID cid);
        	 void channelCreated(const epics::pvData::Status& status, Channel* channel);
        	 void channelStateChange(Channel* c, const Channel::ConnectionState isConnected);
        	 String getRequesterName();
        	 void message(const String message, const epics::pvData::MessageType messageType);
        	 void lock();
        	 void unlock();
        	 void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	 Transport* _transport;
        	 const String _channelName;
        	 const pvAccessID _cid;
        	 epics::pvData::Status _status;
        	 Channel* _channel;
        	 epics::pvData::Mutex _mutex;
        	 void createChannelFailedResponse(epics::pvData::ByteBuffer* buffer, TransportSendControl* control, const epics::pvData::Status& status);
        };

        /****************************************************************************************/
        /**
         * Destroy channel request handler.
         */
        class ServerDestroyChannelHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerDestroyChannelHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Destroy channel request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport* transport, int8 version, int8 command,
                    int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };


        class ServerDestroyChannelHandlerTransportSender : public ReferencedTransportSender
        {
        public:
        	ServerDestroyChannelHandlerTransportSender(pvAccessID cid, pvAccessID sid): _cid(cid), _sid(sid) {

        	}

        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) {
        		control->startMessage((int8)8, 2*sizeof(int32)/sizeof(int8));
        		buffer->putInt(_sid);
        		buffer->putInt(_cid);
        	}

        	void lock() {
        		// noop
        	}

        	void unlock() {
        		// noop
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
        	/**
        	 * @param context
        	 */
        	ServerGetHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Get request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelGetRequesterImpl : public BaseChannelRequester, public ChannelGetRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelGetRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,
   				 epics::pvData::PVStructurePtr pvRequest);
        	void channelGetConnect(const epics::pvData::Status& status, ChannelGet* channelGet, epics::pvData::PVStructurePtr pvStructure,
        			epics::pvData::BitSet* bitSet);
        	void getDone(const epics::pvData::Status& status);
        	void destroy();
    		/**
    		 * @return the channelGet
    		 */
        	ChannelGet* getChannelGet();
        	void lock();
        	void unlock();
			void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
			ChannelGet* _channelGet;
			epics::pvData::BitSet* _bitSet;
			epics::pvData::PVStructurePtr _pvStructure;
			epics::pvData::Status _status;
        };


        /****************************************************************************************/
        /**
         * Put request handler.
         */
        class ServerPutHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerPutHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Put request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelPutRequesterImpl : public BaseChannelRequester, public ChannelPutRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelPutRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,epics::pvData::PVStructure* pvRequest);
        	void channelPutConnect(const epics::pvData::Status& status, ChannelPut* channelPut, epics::pvData::PVStructure* pvStructure, epics::pvData::BitSet* bitSet);
        	void putDone(const epics::pvData::Status& status);
        	void getDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
            void destroy();
    		/**
    		 * @return the channelPut
    		 */
            ChannelPut* getChannelPut();
    		/**
    		 * @return the bitSet
    		 */
            BitSet* getBitSet();
            /**
             * @return the pvStructure
             */
            PVStructure* getPVStructure();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	ChannelPut* _channelPut;
        	epics::pvData::BitSet* _bitSet;
        	epics::pvData::PVStructure* _pvStructure;
        	epics::pvData::Status _status;
        };

        /****************************************************************************************/
        /**
         * Put request handler.
         */
        class ServerPutGetHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerPutGetHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Put-get request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelPutGetRequesterImpl : public BaseChannelRequester, public ChannelPutGetRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelPutGetRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,epics::pvData::PVStructure* pvRequest);
        	void channelPutGetConnect(const epics::pvData::Status& status, ChannelPutGet* channelPutGet, epics::pvData::PVStructure* pvPutStructure, epics::pvData::PVStructure* pvGetStructure);
        	void getGetDone(const epics::pvData::Status& status);
        	void getPutDone(const epics::pvData::Status& status);
        	void putGetDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
            void destroy();
    		/**
    		 * @return the channelPutGet
    		 */
            ChannelPutGet* getChannelPutGet();
            /**
             * @return the pvPutStructure
             */
            PVStructure* getPVPutStructure();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	ChannelPutGet* _channelPutGet;
        	epics::pvData::PVStructure* _pvPutStructure;
        	epics::pvData::PVStructure* _pvGetStructure;
        	epics::pvData::Status _status;
        };


        /****************************************************************************************/
        /**
         * Monitor request handler.
         */
        class ServerMonitorHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerMonitorHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Monitor request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };


        class ServerMonitorRequesterImpl : public BaseChannelRequester, public MonitorRequester, public ReferencedTransportSender
        {
        public:
        	ServerMonitorRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,epics::pvData::PVStructure* pvRequest);
        	void monitorConnect(const epics::pvData::Status& status, epics::pvData::Monitor* monitor, epics::pvData::Structure* structure);
        	void unlisten(epics::pvData::Monitor* monitor);
        	void monitorEvent(epics::pvData::Monitor* monitor);
        	void lock();
        	void unlock();
        	void destroy();
    		/**
    		 * @return the channelMonitor
    		 */
        	epics::pvData::Monitor* getChannelMonitor();
        	 void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	 epics::pvData::Monitor* _monitor;
        	 epics::pvData::Monitor* _channelMonitor;
        	 epics::pvData::Structure* _structure;
        	 epics::pvData::Status _status;
        };


        /****************************************************************************************/
        /**
         * Array request handler.
         */
        class ServerArrayHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerArrayHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Array request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelArrayRequesterImpl : public BaseChannelRequester, public ChannelArrayRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelArrayRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,epics::pvData::PVStructure* pvRequest);
        	void channelArrayConnect(const epics::pvData::Status& status, ChannelArray* channelArray, epics::pvData::PVArray* pvArray);
        	void getArrayDone(const epics::pvData::Status& status);
        	void putArrayDone(const epics::pvData::Status& status);
        	void setLengthDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
        	void destroy();
        	/**
        	 * @return the channelArray
        	 */
        	ChannelArray* getChannelArray();
    		/**
    		 * @return the pvArray
    		 */
        	epics::pvData::PVArray* getPVArray();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	 ChannelArray* _channelArray;
        	 epics::pvData::PVArray* _pvArray;
        	 epics::pvData::Status _status;
        };

        /****************************************************************************************/
        /**
         * Cancel request handler.
         */
        class ServerCancelRequestHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerCancelRequestHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Cancel request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        private:
        	/**
        	 * @param transport
        	 * @param ioid
        	 * @param errorStatus
        	 */
        	void failureResponse(Transport* transport, pvAccessID ioid, const epics::pvData::Status& errorStatus);
        };


        /****************************************************************************************/
        /**
         * Process request handler.
         */
        class ServerProcessHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerProcessHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Process request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelProcessRequesterImpl : public BaseChannelRequester, public ChannelProcessRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelProcessRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,epics::pvData::PVStructure* pvRequest);
        	void channelProcessConnect(const epics::pvData::Status& status, ChannelProcess* channelProcess);
        	void processDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
        	void destroy();
    		/**
    		 * @return the channelProcess
    		 */
    		ChannelProcess* getChannelProcess();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	ChannelProcess* _channelProcess;
        	epics::pvData::Status _status;
        };

        /****************************************************************************************/
        /**
         *  Get field request handler.
         */
        class ServerGetFieldHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerGetFieldHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "Get field request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        private:
        	void getFieldFailureResponse(Transport* transport, const pvAccessID ioid, const epics::pvData::Status& errorStatus);
        };

        class ServerGetFieldRequesterImpl : public BaseChannelRequester, public GetFieldRequester, public ReferencedTransportSender
        {
        public:
        	ServerGetFieldRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport);
        	void getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr field);
        	void lock();
        	void unlock();
            void destroy();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	epics::pvData::Status _status;
        	epics::pvData::FieldConstPtr _field;
        };

        class ServerGetFieldHandlerTransportSender : public ReferencedTransportSender
        {
        public:
        	ServerGetFieldHandlerTransportSender(const pvAccessID ioid,const epics::pvData::Status& status, Transport* transport):
        		_ioid(ioid), _status(status), _transport(transport) {

        	}

        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) {
				control->startMessage((int8)17, sizeof(int32)/sizeof(int8));
				buffer->putInt(_ioid);
				_transport->getIntrospectionRegistry()->serializeStatus(buffer, control, _status);
        	}

        	void lock() {
        		// noop
        	}

        	void unlock() {
        		// noop
        	}

        private:
        	const pvAccessID _ioid;
        	const epics::pvData::Status& _status;
        	Transport* _transport;
        };



        /****************************************************************************************/
        /**
         * RPC handler.
         */
        class ServerRPCHandler : public AbstractServerResponseHandler
        {
        public:
        	/**
        	 * @param context
        	 */
        	ServerRPCHandler(ServerContextImpl* context) :
        		AbstractServerResponseHandler(context, "RPC request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport* transport, int8 version, int8 command,
        			int payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelRPCRequesterImpl : public BaseChannelRequester, public ChannelRPCRequester, public ReferencedTransportSender
        {
        public:
        	ServerChannelRPCRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,epics::pvData::PVStructure* pvRequest);
        	void channelRPCConnect(const epics::pvData::Status& status, ChannelRPC* channelRPC, epics::pvData::PVStructure* arguments, epics::pvData::BitSet* bitSet);
        	void requestDone(const epics::pvData::Status& status, epics::pvData::PVStructure* pvResponse);
        	void lock();
        	void unlock();
        	void destroy();
    		/**
    		 * @return the channelRPC
    		 */
        	ChannelRPC* getChannelRPC();
        	/**
        	 * @return the pvArguments
        	 */
        	PVStructure* getPvArguments();
        	/**
        	 * @return the agrumentsBitSet
        	 */
        	BitSet* getAgrumentsBitSet();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	ChannelRPC* _channelRPC;
        	epics::pvData::PVStructure* _pvArguments;
        	epics::pvData::PVStructure* _pvResponse;
        	epics::pvData::BitSet* _argumentsBitSet;
        	epics::pvData::Status _status;
        };
    }
}

#endif /* RESPONSEHANDLERS_H_ */
