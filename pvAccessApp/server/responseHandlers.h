/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef RESPONSEHANDLERS_H_
#define RESPONSEHANDLERS_H_

#include <pv/serverContext.h>
#include <pv/remote.h>
#include <pv/serverChannelImpl.h>
#include <pv/baseChannelRequester.h>

namespace epics {
namespace pvAccess {

        /**
         */
        class AbstractServerResponseHandler : public AbstractResponseHandler {
        protected:
            ServerContextImpl::shared_pointer _context;
        public:
            AbstractServerResponseHandler(ServerContextImpl::shared_pointer const & context, epics::pvData::String description) : 
                AbstractResponseHandler(context.get(), description), _context(context) {
            }

            virtual ~AbstractServerResponseHandler() {
            }
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
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /**
         * PVAS request handler - main handler which dispatches requests to appropriate handlers.
         */
        class ServerResponseHandler : public ResponseHandler {
        public:
            ServerResponseHandler(ServerContextImpl::shared_pointer const & context);

            virtual ~ServerResponseHandler() {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        private:
            /**
             * Table of response handlers for each command ID.
             */
            std::vector<ResponseHandler::shared_pointer> m_handlerTable;

        };

        /**
         * Connection validation message handler.
         */
        class ServerConnectionValidationHandler : public AbstractServerResponseHandler {
        public:
            ServerConnectionValidationHandler(ServerContextImpl::shared_pointer const & context) :
                AbstractServerResponseHandler(context, "Connection validation") {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /**
         * NOOP response.
         */
        class ServerNoopResponse : public AbstractServerResponseHandler {
        public:
            ServerNoopResponse(ServerContextImpl::shared_pointer const & context, epics::pvData::String description) :
                AbstractServerResponseHandler(context, description) {
            }
        };

        /**
         * Echo request handler.
         */
        class ServerEchoHandler : public AbstractServerResponseHandler {
        public:
            ServerEchoHandler(ServerContextImpl::shared_pointer const & context) :
                AbstractServerResponseHandler(context, "Echo request") {
            }

            virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class EchoTransportSender : public TransportSender {
        public:
        	EchoTransportSender(osiSockAddr* echoFrom) {
        		memcpy(&_echoFrom, echoFrom, sizeof(osiSockAddr));
        	}

        	virtual ~EchoTransportSender() {
        	}

        	virtual void send(epics::pvData::ByteBuffer* /*buffer*/, TransportSendControl* control) {
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
        };

        /**
         * Introspection search request handler.
         */
        class ServerIntrospectionSearchHandler : public AbstractServerResponseHandler
        {
        public:
        	ServerIntrospectionSearchHandler(ServerContextImpl::shared_pointer const & context) :
        		AbstractServerResponseHandler(context, "Search request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        /****************************************************************************************/
        /**
         * Search channel request handler.
         */
         // TODO object pool!!!
        class ServerSearchHandler : public AbstractServerResponseHandler
        {
        public:
            // TODO
            static std::map<epics::pvData::String, std::tr1::weak_ptr<ChannelProvider> > s_channelNameToProvider;
            
        	ServerSearchHandler(ServerContextImpl::shared_pointer const & context);
        	virtual ~ServerSearchHandler();

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        private:
        	std::vector<ChannelProvider::shared_pointer> _providers;
        };


        class ServerChannelFindRequesterImpl:
            public ChannelFindRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelFindRequesterImpl>
        {
        public:
        	ServerChannelFindRequesterImpl(ServerContextImpl::shared_pointer const & context, epics::pvData::int32 expectedResponseCount);
        	void clear();
        	ServerChannelFindRequesterImpl* set(epics::pvData::String _name, epics::pvData::int32 searchSequenceId, epics::pvData::int32 cid, osiSockAddr* sendTo, bool responseRequired);
        	void channelFindResult(const epics::pvData::Status& status, ChannelFind::shared_pointer const & channelFind, bool wasFound);
        	void lock();
        	void unlock();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
            epics::pvData::String _name;
        	epics::pvData::int32 _searchSequenceId;
        	epics::pvData::int32 _cid;
        	osiSockAddr* _sendTo;
        	bool _responseRequired;
        	bool _wasFound;
        	ServerContextImpl::shared_pointer _context;
        	epics::pvData::Mutex _mutex;
        	epics::pvData::int32 _expectedResponseCount;
        	epics::pvData::int32 _responseCount;
        };

        /****************************************************************************************/
        /**
         * Create channel request handler.
         */
        class ServerCreateChannelHandler : public AbstractServerResponseHandler
        {
        public:
        	ServerCreateChannelHandler(ServerContextImpl::shared_pointer const & context) :
        		AbstractServerResponseHandler(context, "Create channel request") {
        		_providers = context->getChannelProviders();
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);

        private:
        	void disconnect(Transport::shared_pointer const & transport);
			std::vector<ChannelProvider::shared_pointer> _providers;
        };

        class ServerChannelRequesterImpl :
            public ChannelRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelRequesterImpl> const_shared_pointer;
        protected:
        	 ServerChannelRequesterImpl(Transport::shared_pointer const & transport, const epics::pvData::String channelName, const pvAccessID cid);
        public:
        	 static ChannelRequester::shared_pointer create(ChannelProvider::shared_pointer const & provider, Transport::shared_pointer const & transport, const epics::pvData::String channelName, const pvAccessID cid);
        	 void channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel);
        	 void channelStateChange(Channel::shared_pointer const & c, const Channel::ConnectionState isConnected);
        	 epics::pvData::String getRequesterName();
        	 void message(epics::pvData::String const & message, epics::pvData::MessageType messageType);
        	 void lock();
        	 void unlock();
        	 void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	 ServerChannel::weak_pointer _serverChannel;
        	 Transport::weak_pointer _transport;
        	 const epics::pvData::String _channelName;
        	 const pvAccessID _cid;
        	 epics::pvData::Status _status;
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
        	ServerDestroyChannelHandler(ServerContextImpl::shared_pointer const & context) :
        		AbstractServerResponseHandler(context, "Destroy channel request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
                    Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
                    std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };


        class ServerDestroyChannelHandlerTransportSender : public TransportSender
        {
        public:
        	ServerDestroyChannelHandlerTransportSender(pvAccessID cid, pvAccessID sid): _cid(cid), _sid(sid) {
        	}

        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) {
        		control->startMessage((epics::pvData::int8)CMD_DESTROY_CHANNEL, 2*sizeof(epics::pvData::int32)/sizeof(epics::pvData::int8));
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
        	ServerGetHandler(ServerContextImpl::shared_pointer const & context) :
        		AbstractServerResponseHandler(context, "Get request") {
        	}

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelGetRequesterImpl :
            public BaseChannelRequester,
            public ChannelGetRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelGetRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelGetRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelGetRequesterImpl> const_shared_pointer;
        protected:
        	ServerChannelGetRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	                              ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	                              Transport::shared_pointer const & transport);
        	void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static ChannelGetRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	                              ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	                              Transport::shared_pointer const & transport,
        	                              epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	void channelGetConnect(const epics::pvData::Status& status, ChannelGet::shared_pointer const & channelGet,
        	                       epics::pvData::PVStructure::shared_pointer const & pvStructure, epics::pvData::BitSet::shared_pointer const & bitSet);
        	void getDone(const epics::pvData::Status& status);
        	void destroy();

        	ChannelGet::shared_pointer getChannelGet();

        	void lock();
        	void unlock();
			void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
			ChannelGet::shared_pointer _channelGet;
			epics::pvData::BitSet::shared_pointer _bitSet;
			epics::pvData::PVStructure::shared_pointer _pvStructure;
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelPutRequesterImpl :
            public BaseChannelRequester,
            public ChannelPutRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelPutRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelPutRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelPutRequesterImpl> const_shared_pointer;
        protected:
        	ServerChannelPutRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static ChannelPutRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	       
        	void channelPutConnect(const epics::pvData::Status& status, ChannelPut::shared_pointer const & channelPut, epics::pvData::PVStructure::shared_pointer const & pvStructure, epics::pvData::BitSet::shared_pointer const & bitSet);
        	void putDone(const epics::pvData::Status& status);
        	void getDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
            void destroy();

            ChannelPut::shared_pointer getChannelPut();
            epics::pvData::BitSet::shared_pointer getBitSet();
            epics::pvData::PVStructure::shared_pointer getPVStructure();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelPutGetRequesterImpl :
            public BaseChannelRequester,
            public ChannelPutGetRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelPutGetRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelPutGetRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelPutGetRequesterImpl> const_shared_pointer;
        protected:
        	ServerChannelPutGetRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static ChannelPutGetRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	
        	void channelPutGetConnect(const epics::pvData::Status& status, ChannelPutGet::shared_pointer const & channelPutGet, epics::pvData::PVStructure::shared_pointer const & pvPutStructure, epics::pvData::PVStructure::shared_pointer const & pvGetStructure);
        	void getGetDone(const epics::pvData::Status& status);
        	void getPutDone(const epics::pvData::Status& status);
        	void putGetDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
            void destroy();

            ChannelPutGet::shared_pointer getChannelPutGet();
            epics::pvData::PVStructure::shared_pointer getPVPutStructure();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	ChannelPutGet::shared_pointer _channelPutGet;
        	epics::pvData::PVStructure::shared_pointer _pvPutStructure;
        	epics::pvData::PVStructure::shared_pointer _pvGetStructure;
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };


        class ServerMonitorRequesterImpl :
            public BaseChannelRequester,
            public epics::pvData::MonitorRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerMonitorRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerMonitorRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerMonitorRequesterImpl> const_shared_pointer;
        protected:
        	ServerMonitorRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static epics::pvData::MonitorRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	       
		void monitorConnect(const epics::pvData::Status& status, epics::pvData::Monitor::shared_pointer const & monitor, epics::pvData::StructureConstPtr const & structure);
        	void unlisten(epics::pvData::Monitor::shared_pointer const & monitor);
        	void monitorEvent(epics::pvData::Monitor::shared_pointer const & monitor);
        	void lock();
        	void unlock();
        	void destroy();

        	epics::pvData::Monitor::shared_pointer getChannelMonitor();
        	 void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	 epics::pvData::Monitor::shared_pointer _channelMonitor;
		     epics::pvData::StructureConstPtr _structure;
        	 epics::pvData::Status _status;
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelArrayRequesterImpl :
            public BaseChannelRequester,
            public ChannelArrayRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelArrayRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelArrayRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelArrayRequesterImpl> const_shared_pointer;
        protected:
        	ServerChannelArrayRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	 void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static ChannelArrayRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	       
        	void channelArrayConnect(const epics::pvData::Status& status, ChannelArray::shared_pointer const & channelArray, epics::pvData::PVArray::shared_pointer const & pvArray);
        	void getArrayDone(const epics::pvData::Status& status);
        	void putArrayDone(const epics::pvData::Status& status);
        	void setLengthDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
        	void destroy();

        	ChannelArray::shared_pointer getChannelArray();

        	epics::pvData::PVArray::shared_pointer getPVArray();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        	
        private:
        	 ChannelArray::shared_pointer _channelArray;
        	 epics::pvData::PVArray::shared_pointer _pvArray;
        	 epics::pvData::Status _status;
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelProcessRequesterImpl :
            public BaseChannelRequester,
            public ChannelProcessRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelProcessRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelProcessRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelProcessRequesterImpl> const_shared_pointer;
        protected:
        	ServerChannelProcessRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static ChannelProcessRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport, epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	       
        	void channelProcessConnect(const epics::pvData::Status& status, ChannelProcess::shared_pointer const & channelProcess);
        	void processDone(const epics::pvData::Status& status);
        	void lock();
        	void unlock();
        	void destroy();

    		ChannelProcess::shared_pointer getChannelProcess();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        	
        private:
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        private:
        	void getFieldFailureResponse(Transport::shared_pointer const & transport, const pvAccessID ioid, const epics::pvData::Status& errorStatus);
        };

        class ServerGetFieldRequesterImpl :
            public BaseChannelRequester,
            public GetFieldRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerGetFieldRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerGetFieldRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerGetFieldRequesterImpl> const_shared_pointer;

        	ServerGetFieldRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	       
        	void getDone(const epics::pvData::Status& status, epics::pvData::FieldConstPtr const & field);
        	void lock();
        	void unlock();
            void destroy();
        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	epics::pvData::Status _status;
        	epics::pvData::FieldConstPtr _field;
        };

        class ServerGetFieldHandlerTransportSender : public TransportSender
        {
        public:
        	ServerGetFieldHandlerTransportSender(const pvAccessID ioid,const epics::pvData::Status& status, Transport::shared_pointer const & transport):
        		_ioid(ioid), _status(status), _transport(transport) {

        	}

        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control) {
				control->startMessage((epics::pvData::int8)CMD_GET_FIELD, sizeof(epics::pvData::int32)/sizeof(epics::pvData::int8));
				buffer->putInt(_ioid);
				_status.serialize(buffer, control);
        	}

        	void lock() {
        		// noop
        	}

        	void unlock() {
        		// noop
        	}

        private:
        	const pvAccessID _ioid;
        	const epics::pvData::Status _status;
        	Transport::shared_pointer const & _transport;
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

        	virtual void handleResponse(osiSockAddr* responseFrom,
        			Transport::shared_pointer const & transport, epics::pvData::int8 version, epics::pvData::int8 command,
        			std::size_t payloadSize, epics::pvData::ByteBuffer* payloadBuffer);
        };

        class ServerChannelRPCRequesterImpl :
            public BaseChannelRequester,
            public ChannelRPCRequester,
            public TransportSender,
            public std::tr1::enable_shared_from_this<ServerChannelRPCRequesterImpl>
        {
        public:
            typedef std::tr1::shared_ptr<ServerChannelRPCRequesterImpl> shared_pointer;
            typedef std::tr1::shared_ptr<const ServerChannelRPCRequesterImpl> const_shared_pointer;
        protected:
        	ServerChannelRPCRequesterImpl(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport);
        	 void activate(epics::pvData::PVStructure::shared_pointer const & pvRequest);
        public:
        	static ChannelRPCRequester::shared_pointer create(ServerContextImpl::shared_pointer const & context,
        	       ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid,
        	       Transport::shared_pointer const & transport,epics::pvData::PVStructure::shared_pointer const & pvRequest);
        	       
        	void channelRPCConnect(const epics::pvData::Status& status, ChannelRPC::shared_pointer const & channelRPC);
        	void requestDone(const epics::pvData::Status& status, epics::pvData::PVStructure::shared_pointer const & pvResponse);
        	void lock();
        	void unlock();
        	void destroy();
    		/**
    		 * @return the channelRPC
    		 */
        	ChannelRPC::shared_pointer getChannelRPC();

        	void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);
        private:
        	ChannelRPC::shared_pointer _channelRPC;
        	epics::pvData::PVStructure::shared_pointer _pvResponse;
        	epics::pvData::Status _status;
        };
    }
}

#endif /* RESPONSEHANDLERS_H_ */
