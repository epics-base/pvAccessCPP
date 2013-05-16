/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/responseHandlers.h>
#include <pv/remote.h>
#include <pv/hexDump.h>
#include <pv/serializationHelper.h>

#include <pv/byteBuffer.h>

#include <osiSock.h>
#include <pv/logger.h>

#include <sstream>

#include <pv/pvAccessMB.h>

using std::ostringstream;
using std::hex;

using std::tr1::dynamic_pointer_cast;
using std::tr1::static_pointer_cast;

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

void ServerBadResponse::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	char ipAddrStr[48];
	ipAddrToDottedIP(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

	LOG(logLevelInfo,
			"Undecipherable message (bad response type %d) from %s.",
			command, ipAddrStr);

}

ServerResponseHandler::ServerResponseHandler(ServerContextImpl::shared_pointer const & context)
{
    MB_INIT;                

    ResponseHandler::shared_pointer badResponse(new ServerBadResponse(context));
    m_handlerTable.resize(CMD_RPC+1);
                
    m_handlerTable[CMD_BEACON].reset(new ServerNoopResponse(context, "Beacon")); /*  0 */
    m_handlerTable[CMD_CONNECTION_VALIDATION].reset(new ServerConnectionValidationHandler(context)); /*  1 */
    m_handlerTable[CMD_ECHO].reset(new ServerEchoHandler(context)); /*  2 */
    m_handlerTable[CMD_SEARCH].reset(new ServerSearchHandler(context)); /*  3 */
    m_handlerTable[CMD_SEARCH_RESPONSE] = badResponse;
    m_handlerTable[CMD_INTROSPECTION_SEARCH].reset(new ServerIntrospectionSearchHandler(context)); /*  5 */
    m_handlerTable[CMD_INTROSPECTION_SEARCH_RESPONSE] = badResponse; /*  6 - introspection search */
    m_handlerTable[CMD_CREATE_CHANNEL].reset(new ServerCreateChannelHandler(context)); /*  7 */
    m_handlerTable[CMD_DESTROY_CHANNEL].reset(new ServerDestroyChannelHandler(context)); /*  8 */ 
    m_handlerTable[CMD_RESERVED0] = badResponse; /*  9 */
    
    m_handlerTable[CMD_GET].reset(new ServerGetHandler(context)); /* 10 - get response */
    m_handlerTable[CMD_PUT].reset(new ServerPutHandler(context)); /* 11 - put response */
    m_handlerTable[CMD_PUT_GET].reset(new ServerPutGetHandler(context)); /* 12 - put-get response */
    m_handlerTable[CMD_MONITOR].reset(new ServerMonitorHandler(context)); /* 13 - monitor response */
    m_handlerTable[CMD_ARRAY].reset(new ServerArrayHandler(context)); /* 14 - array response */
    m_handlerTable[CMD_CANCEL_REQUEST].reset(new ServerCancelRequestHandler(context)); /* 15 - cancel request */
    m_handlerTable[CMD_PROCESS].reset(new ServerProcessHandler(context)); /* 16 - process response */
    m_handlerTable[CMD_GET_FIELD].reset(new ServerGetFieldHandler(context)); /* 17 - get field response */
    m_handlerTable[CMD_MESSAGE] = badResponse; /* 18 - message to Requester */
    m_handlerTable[CMD_MULTIPLE_DATA] = badResponse; /* 19 - grouped monitors */
    m_handlerTable[CMD_RPC].reset(new ServerRPCHandler(context)); /* 20 - RPC response */
}

void ServerResponseHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	if(command<0||command>=(int8)m_handlerTable.size())
	{
		LOG(logLevelDebug,
				"Invalid (or unsupported) command: %x.", (0xFF&command));
				
		// TODO remove debug output
		std::ostringstream name;
		name<<"Invalid PVA header "<<hex<<(int)(0xFF&command);
		name<<", its payload buffer";

		hexDump(name.str(), (const int8*)payloadBuffer->getArray(),
				payloadBuffer->getPosition(), payloadSize);
		return;
	}

	// delegate
	m_handlerTable[command]->handleResponse(responseFrom, transport,
			version, command, payloadSize, payloadBuffer);
}

void ServerConnectionValidationHandler::handleResponse(
		osiSockAddr* responseFrom, Transport::shared_pointer const & transport, int8 version,
		int8 command, size_t payloadSize,
		ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	transport->ensureData(2*sizeof(int32)+sizeof(int16));
	transport->setRemoteTransportReceiveBufferSize(
			payloadBuffer->getInt());
	transport->setRemoteTransportSocketReceiveBufferSize(
			payloadBuffer->getInt());
	transport->setRemoteRevision(version);
	// TODO support priority  !!!
	//transport.setPriority(payloadBuffer.getShort());
}

void ServerEchoHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

    // send back
	TransportSender::shared_pointer echoReply(new EchoTransportSender(responseFrom));
	transport->enqueueSendRequest(echoReply);
}

void ServerIntrospectionSearchHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	THROW_BASE_EXCEPTION("not implemented");
}

/****************************************************************************************/

ServerSearchHandler::ServerSearchHandler(ServerContextImpl::shared_pointer const & context) :
        AbstractServerResponseHandler(context, "Search request"), _providers(context->getChannelProviders())
{
}

ServerSearchHandler::~ServerSearchHandler()
{
}

void ServerSearchHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	transport->ensureData((sizeof(int32)+sizeof(int16))/sizeof(int8)+1);
	const int32 searchSequenceId = payloadBuffer->getInt();
	const int8 qosCode = payloadBuffer->getByte();
	const int32 count = payloadBuffer->getShort() & 0xFFFF;
	const bool responseRequired = (QOS_REPLY_REQUIRED & qosCode) != 0;

	for (int32 i = 0; i < count; i++)
	{
		transport->ensureData(sizeof(int32)/sizeof(int8));
		const int32 cid = payloadBuffer->getInt();
		const String name = SerializeHelper::deserializeString(payloadBuffer, transport.get());
		// no name check here...

		// TODO object pool!!!
		int providerCount = _providers.size();
		ServerChannelFindRequesterImpl* pr = new ServerChannelFindRequesterImpl(_context, providerCount);
		pr->set(name, searchSequenceId, cid, responseFrom, responseRequired);
		ChannelFindRequester::shared_pointer spr(pr);
		
        for (int i = 0; i < providerCount; i++)		
		  _providers[i]->channelFind(name, spr);
	}
}

ServerChannelFindRequesterImpl::ServerChannelFindRequesterImpl(ServerContextImpl::shared_pointer const & context,
                                                               int32 expectedResponseCount) :
												_sendTo(NULL),
												_wasFound(false),
												_context(context),
												_expectedResponseCount(expectedResponseCount),
												_responseCount(0)
												{}

void ServerChannelFindRequesterImpl::clear()
{
	Lock guard(_mutex);
	_sendTo = NULL;
	_wasFound = false;
	_responseCount = 0;
}

ServerChannelFindRequesterImpl* ServerChannelFindRequesterImpl::set(String name, int32 searchSequenceId, int32 cid, osiSockAddr* sendTo, bool responseRequired)
{
	Lock guard(_mutex);
	_name = name;
	_searchSequenceId = searchSequenceId;
	_cid = cid;
	_sendTo = sendTo;
	_responseRequired = responseRequired;
	return this;
}

std::map<String, std::tr1::weak_ptr<ChannelProvider> > ServerSearchHandler::s_channelNameToProvider;

void ServerChannelFindRequesterImpl::channelFindResult(const Status& /*status*/, ChannelFind::shared_pointer const & channelFind, bool wasFound)
{
	// TODO status
	Lock guard(_mutex);
	
	_responseCount++;
	if (_responseCount > _expectedResponseCount)
	{
	   if ((_responseCount+1) == _expectedResponseCount)
	   {
    	   LOG(logLevelDebug,"[ServerChannelFindRequesterImpl::channelFindResult] More responses received than expected fpr channel '%s'!", _name.c_str());
	   }
	   return;
	}
	
	if (wasFound && _wasFound)
	{
	   LOG(logLevelDebug,"[ServerChannelFindRequesterImpl::channelFindResult] Channel '%s' is hosted by different channel providers!", _name.c_str());
	   return;
	}
	
	if (wasFound || (_responseRequired && (_responseCount == _expectedResponseCount)))
	{
	   if (wasFound && _expectedResponseCount > 1)
	   {
        ServerSearchHandler::s_channelNameToProvider[_name] = channelFind->getChannelProvider();
	   }
	   
		_wasFound = wasFound;
		TransportSender::shared_pointer thisSender = shared_from_this();
		_context->getBroadcastTransport()->enqueueSendRequest(thisSender);
	}
}

void ServerChannelFindRequesterImpl::lock()
{
	// noop
}

void ServerChannelFindRequesterImpl::unlock()
{
	// noop
}

void ServerChannelFindRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	int32 count = 1;
	control->startMessage((int8)4, (sizeof(int32)+sizeof(int8)+128+2*sizeof(int16)+count*sizeof(int32))/sizeof(int8));

	Lock guard(_mutex);
	buffer->putInt(_searchSequenceId);
	buffer->putByte(_wasFound ? (int8)1 : (int8)0);

	// NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
	encodeAsIPv6Address(buffer, _context->getServerInetAddress());
	buffer->putShort((int16)_context->getServerPort());
	buffer->putShort((int16)count);
	buffer->putInt(_cid);

	control->setRecipient(*_sendTo);
}

/****************************************************************************************/
void ServerCreateChannelHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// TODO for not only one request at the time is supported, i.e. dataCount == 1
	transport->ensureData((sizeof(int32)+sizeof(int16))/sizeof(int8));
	const int16 count = payloadBuffer->getShort();
	if (count != 1)
	{
		THROW_BASE_EXCEPTION("only 1 supported for now");
	}
	const pvAccessID cid = payloadBuffer->getInt();

	String channelName = SerializeHelper::deserializeString(payloadBuffer, transport.get());
	if (channelName.size() == 0)
	{

		char host[100];
		sockAddrToA(&transport->getRemoteAddress()->sa,host,100);
		LOG(logLevelDebug,"Zero length channel name, disconnecting client: %s", host);
		disconnect(transport);
		return;
	}
	else if (channelName.size() > MAX_CHANNEL_NAME_LENGTH)
	{
		char host[100];
		sockAddrToA(&transport->getRemoteAddress()->sa,host,100);
		LOG(logLevelDebug,"Unreasonable channel name length, disconnecting client: %s", host);
		disconnect(transport);
		return;
	}

    // TODO !!!
	//ServerChannelRequesterImpl::create(_providers[0], transport, channelName, cid);
	
	
	if (_providers.size() == 1)
    	ServerChannelRequesterImpl::create(_providers[0], transport, channelName, cid);
	else
    	ServerChannelRequesterImpl::create(ServerSearchHandler::s_channelNameToProvider[channelName].lock(), transport, channelName, cid);     // TODO !!!!
}

void ServerCreateChannelHandler::disconnect(Transport::shared_pointer const & transport)
{
	transport->close();
}

ServerChannelRequesterImpl::ServerChannelRequesterImpl(Transport::shared_pointer const & transport,
    const String channelName, const pvAccessID cid) :
    _serverChannel(),
    _transport(transport),
    _channelName(channelName),
    _cid(cid),
    _status(),
    _mutex()
{
}

ChannelRequester::shared_pointer ServerChannelRequesterImpl::create(
    ChannelProvider::shared_pointer const & provider, Transport::shared_pointer const & transport,
    const String channelName, const pvAccessID cid)
{
	ChannelRequester::shared_pointer cr(new ServerChannelRequesterImpl(transport, channelName, cid));
    // TODO exception guard and report error back
	provider->createChannel(channelName, cr, transport->getPriority());
	return cr;
}

void ServerChannelRequesterImpl::channelCreated(const Status& status, Channel::shared_pointer const & channel)
{
    if(Transport::shared_pointer transport = _transport.lock())
    {
		ServerChannel::shared_pointer serverChannel;
		try
		{
		    if (status.isSuccess())
		    {
    			// NOTE: we do not explicitly check if transport OK
    			ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);
    			if (!casTransport.get())
    			     THROW_BASE_EXCEPTION("transport is unable to host channels");
    
    			//
    			// create a new channel instance
    			//
    			pvAccessID sid = casTransport->preallocateChannelSID();
    			try
    			{
    			    epics::pvData::PVField::shared_pointer securityToken = casTransport->getSecurityToken();
    				serverChannel.reset(new ServerChannelImpl(channel, _cid, sid, securityToken));
    
    				// ack allocation and register
    				casTransport->registerChannel(sid, serverChannel);
    
    			} catch (...)
    			{
    				// depreallocate and rethrow
    				casTransport->depreallocateChannelSID(sid);
    				throw;
    			}
		    }
				
			{
        	Lock guard(_mutex);
	        _status = status;
	        _serverChannel = serverChannel;
            }

        	TransportSender::shared_pointer thisSender = shared_from_this();
        	transport->enqueueSendRequest(thisSender);
		}
		catch (std::exception& e)
		{
			LOG(logLevelDebug, "Exception caught when creating channel: %s", _channelName.c_str());
			{
			 Lock guard(_mutex);
            _status = Status(Status::STATUSTYPE_FATAL,  "failed to create channel", e.what());
			}
        	TransportSender::shared_pointer thisSender = shared_from_this();
        	transport->enqueueSendRequest(thisSender);
			// TODO make sure that serverChannel gets destroyed
		}
		catch (...)
		{
			LOG(logLevelDebug, "Exception caught when creating channel: %s", _channelName.c_str());
			{
			 Lock guard(_mutex);
			_status = Status(Status::STATUSTYPE_FATAL,  "failed to create channel");
			}
        	TransportSender::shared_pointer thisSender = shared_from_this();
        	transport->enqueueSendRequest(thisSender);
			// TODO make sure that serverChannel gets destroyed
		}
    }
}

void ServerChannelRequesterImpl::channelStateChange(Channel::shared_pointer const & /*channel*/, const Channel::ConnectionState /*isConnected*/)
{
	// TODO should we notify remote side?
}

String ServerChannelRequesterImpl::getRequesterName()
{
	std::stringstream name;
	name << "ServerChannelRequesterImpl/" << _channelName << "[" << _cid << "]"; 
	return name.str();
}

void ServerChannelRequesterImpl::message(String const & message, MessageType messageType)
{
	LOG(logLevelDebug, "[%s] %s", getMessageTypeName(messageType).c_str(), message.c_str());
}

void ServerChannelRequesterImpl::lock()
{
	//noop
}

void ServerChannelRequesterImpl::unlock()
{
	//noop
}

void ServerChannelRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	ServerChannel::shared_pointer serverChannel;
	Status status;
	{
		Lock guard(_mutex);
		serverChannel = _serverChannel.lock();
		status = _status;
	}

	// error response
	if (serverChannel.get() == NULL)
	{
		createChannelFailedResponse(buffer, control, status);
	}
	// OK
	else if (Transport::shared_pointer transport = _transport.lock())
	{
	   ServerChannelImpl::shared_pointer serverChannelImpl = dynamic_pointer_cast<ServerChannelImpl>(serverChannel);
		control->startMessage((int8)CMD_CREATE_CHANNEL, 2*sizeof(int32)/sizeof(int8));
		buffer->putInt(serverChannelImpl->getCID());
		buffer->putInt(serverChannelImpl->getSID());
		status.serialize(buffer, control);
	}
}


void ServerChannelRequesterImpl::createChannelFailedResponse(ByteBuffer* buffer, TransportSendControl* control, const Status& status)
{
	if (Transport::shared_pointer transport = _transport.lock())
	{
        control->startMessage((int8)CMD_CREATE_CHANNEL, 2*sizeof(int32)/sizeof(int8));
    	buffer->putInt(_cid);
    	buffer->putInt(-1);
		status.serialize(buffer, control);
	}
}

/****************************************************************************************/

void ServerDestroyChannelHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);


	transport->ensureData(2*sizeof(int32)/sizeof(int8));
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID cid = payloadBuffer->getInt();

	// get channel by SID
	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel.get() == NULL)
	{
		if (!transport->isClosed())
		{
			char host[100];
			sockAddrToA(&responseFrom->sa,host,100);
			LOG(logLevelDebug, "Trying to destroy a channel that no longer exists (SID: %d, CID %d, client: %s).", sid, cid, host);
		}
		return;
	}

	// destroy
	channel->destroy();

	// .. and unregister
	casTransport->unregisterChannel(sid);

	// send response back
	TransportSender::shared_pointer sr(new ServerDestroyChannelHandlerTransportSender(cid, sid));
	transport->enqueueSendRequest(sr);
}

/****************************************************************************************/

void ServerGetHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer)
{
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel.get() == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_GET, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerChannelGetRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
        MB_INC_AUTO_ID(channelGet);
        MB_POINT(channelGet, 3, "server channelGet->deserialize request (start)");
        
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;

		ServerChannelGetRequesterImpl::shared_pointer request = static_pointer_cast<ServerChannelGetRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_GET, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_GET, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

        MB_POINT(channelGet, 4, "server channelGet->deserialize request (end)");
        
		request->getChannelGet()->get(lastRequest);
	}
}

#define INIT_EXCEPTION_GUARD(cmd, code) \
    try { \
 	    code; \
    } \
    catch (std::exception &e) { \
        Status status(Status::STATUSTYPE_FATAL, e.what()); \
	    BaseChannelRequester::sendFailureMessage((int8)cmd, _transport, _ioid, (int8)QOS_INIT, status); \
	    destroy(); \
    } \
    catch (...) { \
        Status status(Status::STATUSTYPE_FATAL, "unknown exception caught"); \
	    BaseChannelRequester::sendFailureMessage((int8)cmd, _transport, _ioid, (int8)QOS_INIT, status); \
	    destroy(); \
    }

ServerChannelGetRequesterImpl::ServerChannelGetRequesterImpl(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid, Transport::shared_pointer const & transport) :
		BaseChannelRequester(context, channel, ioid, transport), _channelGet(), _bitSet(), _pvStructure()

{
}

ChannelGetRequester::shared_pointer ServerChannelGetRequesterImpl::create(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel, const pvAccessID ioid, Transport::shared_pointer const & transport,
PVStructure::shared_pointer const & pvRequest)
{
    ChannelGetRequester::shared_pointer thisPointer(new ServerChannelGetRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerChannelGetRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerChannelGetRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	ChannelGetRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_GET, _channelGet = _channel->getChannel()->createChannelGet(thisPointer, pvRequest));
}

void ServerChannelGetRequesterImpl::channelGetConnect(const Status& status, ChannelGet::shared_pointer const & channelGet, PVStructure::shared_pointer const & pvStructure, BitSet::shared_pointer const & bitSet)
{
	{
		Lock guard(_mutex);
		_bitSet = bitSet;
		_pvStructure = pvStructure;
		_status = status;
		_channelGet = channelGet;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelGetRequesterImpl::getDone(const Status& status)
{
    MB_POINT(channelGet, 5, "server channelGet->getDone()");
	{
		Lock guard(_mutex);
		_status = status;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelGetRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelGet != NULL)
		{
			_channelGet->destroy();
		}
	}
	// TODO not competely safe for when callig getChannelGet() now
	_channelGet.reset();
}

ChannelGet::shared_pointer ServerChannelGetRequesterImpl::getChannelGet()
{
	return _channelGet;
}

void ServerChannelGetRequesterImpl::lock()
{
	//noop
}

void ServerChannelGetRequesterImpl::unlock()
{
	//noop
}

void ServerChannelGetRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int8)CMD_GET, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->put((int8)request);
	{
		Lock guard(_mutex);
		_status.serialize(buffer, control);
	}

    // TODO !!!
    // if we call stopRequest() below (the second one, commented out), we might be too late
    // since between last serialization data and stopRequest() a buffer can be already flushed
    // (i.e. in case of directSerialize)
    // if we call it here, then a bad client can issue another request just after stopRequest() was called
	stopRequest();
	
	if (_status.isSuccess())
	{
		if (request & QOS_INIT)
		{
			Lock guard(_mutex);
            control->cachedSerialize(_pvStructure != NULL ? _pvStructure->getField() : FieldConstPtr(), buffer);

		}
		else
		{
            MB_POINT(channelGet, 6, "server channelGet->serialize response (start)");
            {
		    // we locked _mutex above, so _channelGet is valid
		    ScopedLock lock(_channelGet);
		    
			_bitSet->serialize(buffer, control);
			_pvStructure->serialize(buffer, control, _bitSet.get());
            }
            MB_POINT(channelGet, 7, "server channelGet->serialize response (end)");
		}
	}

	//stopRequest();

	// lastRequest
	if (request & QOS_DESTROY)
	{
		destroy();
	}
}
/****************************************************************************************/
void ServerPutHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);


	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_PUT, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerChannelPutRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;
		const bool get = (QOS_GET & qosCode) != 0;

		ServerChannelPutRequesterImpl::shared_pointer request = static_pointer_cast<ServerChannelPutRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_PUT, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_PUT, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		if (get)
		{
			// no destroy w/ get
			request->getChannelPut()->get();
		}
		else
		{
			// deserialize bitSet and do a put
			ChannelPut::shared_pointer channelPut = request->getChannelPut();
			{
    			ScopedLock lock(channelPut);     // TODO not needed if put is processed by the same thread
    			BitSet::shared_pointer putBitSet = request->getBitSet();
    			putBitSet->deserialize(payloadBuffer, transport.get());
    			request->getPVStructure()->deserialize(payloadBuffer, transport.get(), putBitSet.get());
			}
			channelPut->put(lastRequest);
		}
	}
}

ServerChannelPutRequesterImpl::ServerChannelPutRequesterImpl(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport):
		BaseChannelRequester(context, channel, ioid, transport), _channelPut(), _bitSet(), _pvStructure()
{
}

ChannelPutRequester::shared_pointer ServerChannelPutRequesterImpl::create(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport, PVStructure::shared_pointer const & pvRequest)
{
    ChannelPutRequester::shared_pointer thisPointer(new ServerChannelPutRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerChannelPutRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerChannelPutRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	ChannelPutRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_PUT, _channelPut = _channel->getChannel()->createChannelPut(thisPointer, pvRequest));
}

void ServerChannelPutRequesterImpl::channelPutConnect(const Status& status, ChannelPut::shared_pointer const & channelPut, PVStructure::shared_pointer const & pvStructure, BitSet::shared_pointer const & bitSet)
{
	{
		Lock guard(_mutex);
		_bitSet = bitSet;
		_pvStructure = pvStructure;
		_status = status;
		_channelPut = channelPut;
	}

	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelPutRequesterImpl::putDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelPutRequesterImpl::getDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelPutRequesterImpl::lock()
{
	//noop
}

void ServerChannelPutRequesterImpl::unlock()
{
	//noop
}

void ServerChannelPutRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelPut != NULL)
		{
			_channelPut->destroy();
		}
	}
	// TODO
	_channelPut.reset();
}

ChannelPut::shared_pointer ServerChannelPutRequesterImpl::getChannelPut()
{
	//Lock guard(_mutex);
	return _channelPut;
}

BitSet::shared_pointer ServerChannelPutRequesterImpl::getBitSet()
{
	//Lock guard(_mutex);
	return _bitSet;
}

PVStructure::shared_pointer ServerChannelPutRequesterImpl::getPVStructure()
{
	//Lock guard(_mutex);
	return _pvStructure;
}

void ServerChannelPutRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)CMD_PUT, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	{
		Lock guard(_mutex);
		_status.serialize(buffer, control);
	}

	if (_status.isSuccess())
	{
		if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
            control->cachedSerialize(_pvStructure != NULL ? _pvStructure->getField() : FieldConstPtr(), buffer);
		}
		else if ((QOS_GET & request) != 0)
		{
    		ScopedLock lock(_channelPut); // _channelPut is valid because we required _mutex above
			_pvStructure->serialize(buffer, control);
		}
	}

	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
		destroy();
}


/****************************************************************************************/
void ServerPutGetHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_PUT_GET, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerChannelPutGetRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;
		const bool getGet = (QOS_GET & qosCode) != 0;
		const bool getPut = (QOS_GET_PUT & qosCode) != 0;

		ServerChannelPutGetRequesterImpl::shared_pointer request = static_pointer_cast<ServerChannelPutGetRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_PUT_GET, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_PUT_GET, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		if (getGet)
		{
			request->getChannelPutGet()->getGet();
		}
		else if(getPut)
		{
			request->getChannelPutGet()->getPut();
		}
		else
		{
			// deserialize bitSet and do a put
			ChannelPutGet::shared_pointer channelPutGet = request->getChannelPutGet();
			{
    			ScopedLock lock(channelPutGet);  // TODO not necessary if read is done in putGet
			    request->getPVPutStructure()->deserialize(payloadBuffer, transport.get());
			}
			channelPutGet->putGet(lastRequest);
		}
	}
}

ServerChannelPutGetRequesterImpl::ServerChannelPutGetRequesterImpl(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport):
		BaseChannelRequester(context, channel, ioid, transport), _channelPutGet(), _pvPutStructure(), _pvGetStructure()
{
}

ChannelPutGetRequester::shared_pointer ServerChannelPutGetRequesterImpl::create(ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport,PVStructure::shared_pointer const & pvRequest)
{
    ChannelPutGetRequester::shared_pointer thisPointer(new ServerChannelPutGetRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerChannelPutGetRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerChannelPutGetRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	ChannelPutGetRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_PUT_GET, _channelPutGet = _channel->getChannel()->createChannelPutGet(thisPointer, pvRequest));
}

void ServerChannelPutGetRequesterImpl::channelPutGetConnect(const Status& status, ChannelPutGet::shared_pointer const & channelPutGet,
		PVStructure::shared_pointer const & pvPutStructure, PVStructure::shared_pointer const & pvGetStructure)
{
	{
		Lock guard(_mutex);
		_pvPutStructure = pvPutStructure;
		_pvGetStructure = pvGetStructure;
		_status = status;
		_channelPutGet = channelPutGet;
	}

	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelPutGetRequesterImpl::getGetDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelPutGetRequesterImpl::getPutDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelPutGetRequesterImpl::putGetDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelPutGetRequesterImpl::lock()
{
	//noop
}

void ServerChannelPutGetRequesterImpl::unlock()
{
	//noop
}

void ServerChannelPutGetRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelPutGet != NULL)
		{
			_channelPutGet->destroy();
		}
	}
	// TODO
	_channelPutGet.reset();
}

ChannelPutGet::shared_pointer ServerChannelPutGetRequesterImpl::getChannelPutGet()
{
	//Lock guard(_mutex);
	return _channelPutGet;
}

PVStructure::shared_pointer ServerChannelPutGetRequesterImpl::getPVPutStructure()
{
	//Lock guard(_mutex);
	return _pvPutStructure;
}

void ServerChannelPutGetRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)12, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	{
		Lock guard(_mutex);
		_status.serialize(buffer, control);
	}

	if (_status.isSuccess())
	{
		if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
            control->cachedSerialize(_pvPutStructure != NULL ? _pvPutStructure->getField() : FieldConstPtr(), buffer);
            control->cachedSerialize(_pvGetStructure != NULL ? _pvGetStructure->getField() : FieldConstPtr(), buffer);
		}
		else if ((QOS_GET & request) != 0)
		{
			Lock guard(_mutex);
			_pvGetStructure->serialize(buffer, control);
		}
		else if ((QOS_GET_PUT & request) != 0)
		{
		    ScopedLock lock(_channelPutGet);  // valid due to _mutex lock above
			//Lock guard(_mutex);
			_pvPutStructure->serialize(buffer, control);
		}
		else
		{
		    ScopedLock lock(_channelPutGet);  // valid due to _mutex lock above
			//Lock guard(_mutex);
			_pvGetStructure->serialize(buffer, control);
		}
	}

	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
		destroy();
}

/****************************************************************************************/
void ServerMonitorHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();
	
	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_MONITOR, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerMonitorRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;
		const bool get = (QOS_GET & qosCode) != 0;
		const bool process = (QOS_PROCESS & qosCode) != 0;

		ServerMonitorRequesterImpl::shared_pointer request = static_pointer_cast<ServerMonitorRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_MONITOR, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

        /*
		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_MONITOR, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}
		*/

		if (process)
		{
			if (get)
				request->getChannelMonitor()->start();
			else
				request->getChannelMonitor()->stop();
			//request.stopRequest();
		}
		else if (get)
		{
			// not supported
		}

		if (lastRequest)
			request->destroy();
	}
}

ServerMonitorRequesterImpl::ServerMonitorRequesterImpl(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport):
		BaseChannelRequester(context, channel, ioid, transport), _channelMonitor(), _structure()
{
}

MonitorRequester::shared_pointer ServerMonitorRequesterImpl::create(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport,PVStructure::shared_pointer const & pvRequest)
{
    MonitorRequester::shared_pointer thisPointer(new ServerMonitorRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerMonitorRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerMonitorRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	MonitorRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_MONITOR, _channelMonitor = _channel->getChannel()->createMonitor(thisPointer, pvRequest));
}

void ServerMonitorRequesterImpl::monitorConnect(const Status& status, Monitor::shared_pointer const & monitor, epics::pvData::StructureConstPtr const & structure)
{
	{
		Lock guard(_mutex);
		_status = status;
		_channelMonitor = monitor;
		_structure = structure;
	}
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerMonitorRequesterImpl::unlisten(Monitor::shared_pointer const & /*monitor*/)
{
	//TODO
}

void ServerMonitorRequesterImpl::monitorEvent(Monitor::shared_pointer const & /*monitor*/)
{
	// TODO !!! if queueSize==0, monitor.poll() has to be called and returned NOW (since there is no cache)
	//sendEvent(transport);

	// TODO implement via TransportSender
	/*
		// initiate submit to dispatcher queue, if necessary
		synchronized (register) {
			if (register.getAndSet(true))
				eventConsumer.consumeEvents(this);
		}*/
	// TODO
	// multiple ((BlockingServerTCPTransport)transport).enqueueMonitorSendRequest(this);
	TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerMonitorRequesterImpl::lock()
{
	//noop
}

void ServerMonitorRequesterImpl::unlock()
{
	//noop
}

void ServerMonitorRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelMonitor != NULL)
		{
			_channelMonitor->destroy();
		}
	}
	// TODO
	_channelMonitor.reset();
}

Monitor::shared_pointer ServerMonitorRequesterImpl::getChannelMonitor()
{
	//Lock guard(_mutex);
	return _channelMonitor;
}

void ServerMonitorRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	if ((QOS_INIT & request) != 0)
	{
		control->startMessage((int32)CMD_MONITOR, sizeof(int32)/sizeof(int8) + 1);
		buffer->putInt(_ioid);
		buffer->putByte((int8)request);

		{
			Lock guard(_mutex);
			_status.serialize(buffer, control);
		}

		if (_status.isSuccess())
		{
		    // valid due to _mutex lock above
			control->cachedSerialize(_structure, buffer);
		}
		stopRequest();
		startRequest(QOS_DEFAULT);
	}
	else
	{
		Monitor::shared_pointer monitor = _channelMonitor;
		MonitorElement::shared_pointer element = monitor->poll();
		if (element != NULL)
		{
			control->startMessage((int8)CMD_MONITOR, sizeof(int32)/sizeof(int8) + 1);
			buffer->putInt(_ioid);
			buffer->putByte((int8)request);

			// changedBitSet and data, if not notify only (i.e. queueSize == -1)
			BitSet::shared_pointer changedBitSet = element->changedBitSet;
			if (changedBitSet != NULL)
			{
				changedBitSet->serialize(buffer, control);
				element->pvStructurePtr->serialize(buffer, control, changedBitSet.get());

				// overrunBitset
				element->overrunBitSet->serialize(buffer, control);
			}

			monitor->release(element);
		}
	}
}

/****************************************************************************************/
void ServerArrayHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_ARRAY, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerChannelArrayRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;
		const bool get = (QOS_GET & qosCode) != 0;
		const bool setLength = (QOS_GET_PUT & qosCode) != 0;

		ServerChannelArrayRequesterImpl::shared_pointer request = static_pointer_cast<ServerChannelArrayRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_ARRAY, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_ARRAY, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}


		if (get)
		{
			const int32 offset = SerializeHelper::readSize(payloadBuffer, transport.get());
			const int32 count = SerializeHelper::readSize(payloadBuffer, transport.get());
			request->getChannelArray()->getArray(lastRequest, offset, count);
		}
		else if (setLength)
		{
			const int32 length = SerializeHelper::readSize(payloadBuffer, transport.get());
			const int32 capacity = SerializeHelper::readSize(payloadBuffer, transport.get());
			request->getChannelArray()->setLength(lastRequest, length, capacity);
		}
		else
		{
			// deserialize data to put
			int32 offset;
			ChannelArray::shared_pointer channelArray = request->getChannelArray();
    	    PVArray::shared_pointer array = request->getPVArray();
			{
    			ScopedLock lock(channelArray);   // TODO not needed if read by the same thread
    			offset = SerializeHelper::readSize(payloadBuffer, transport.get());
    			array->deserialize(payloadBuffer, transport.get());
			}
			channelArray->putArray(lastRequest, offset, array->getLength());
		}
	}
}

ServerChannelArrayRequesterImpl::ServerChannelArrayRequesterImpl(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport):
		BaseChannelRequester(context, channel, ioid, transport), _channelArray(), _pvArray()
{
}

ChannelArrayRequester::shared_pointer ServerChannelArrayRequesterImpl::create(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport,PVStructure::shared_pointer const & pvRequest)
{
    ChannelArrayRequester::shared_pointer thisPointer(new ServerChannelArrayRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerChannelArrayRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerChannelArrayRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	ChannelArrayRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_ARRAY, _channelArray = _channel->getChannel()->createChannelArray(thisPointer, pvRequest));
}

void ServerChannelArrayRequesterImpl::channelArrayConnect(const Status& status, ChannelArray::shared_pointer const & channelArray, PVArray::shared_pointer const & pvArray)
{
	{
		Lock guard(_mutex);
		_status = status;
		_pvArray = pvArray;
		_channelArray = channelArray;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelArrayRequesterImpl::getArrayDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelArrayRequesterImpl::putArrayDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelArrayRequesterImpl::setLengthDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelArrayRequesterImpl::lock()
{
	//noop
}

void ServerChannelArrayRequesterImpl::unlock()
{
	//noop
}

void ServerChannelArrayRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelArray != NULL)
		{
			_channelArray->destroy();
		}
	}
	// TODO
	_channelArray.reset();
}

ChannelArray::shared_pointer ServerChannelArrayRequesterImpl::getChannelArray()
{
	//Lock guard(_mutex);
	return _channelArray;
}

PVArray::shared_pointer ServerChannelArrayRequesterImpl::getPVArray()
{
	//Lock guard(_mutex);
	return _pvArray;
}

void ServerChannelArrayRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)CMD_ARRAY, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	{
		Lock guard(_mutex);
		_status.serialize(buffer, control);
	}

	if (_status.isSuccess())
	{
		if ((QOS_GET & request) != 0)
		{
			//Lock guard(_mutex);
			ScopedLock lock(_channelArray);  // valid due to _mutex lock above
			_pvArray->serialize(buffer, control, 0, _pvArray->getLength());
		}
		else if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
            control->cachedSerialize(_pvArray != NULL ? _pvArray->getField() : FieldConstPtr(), buffer);
		}
	}

	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
		destroy();
}

/****************************************************************************************/
void ServerCancelRequestHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8));
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		failureResponse(transport, ioid, BaseChannelRequester::badCIDStatus);
		return;
	}

	Destroyable::shared_pointer request = channel->getRequest(ioid);
	if (request == NULL)
	{
		failureResponse(transport, ioid, BaseChannelRequester::badIOIDStatus);
		return;
	}

	// destroy
	request->destroy();

	// ... and remove from channel
	channel->unregisterRequest(ioid);
}

void ServerCancelRequestHandler::failureResponse(Transport::shared_pointer const & transport, pvAccessID ioid, const Status& errorStatus)
{
	BaseChannelRequester::message(transport, ioid, errorStatus.getMessage(), warningMessage);
}

/****************************************************************************************/
void ServerProcessHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_PROCESS, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerChannelProcessRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;

		ServerChannelProcessRequesterImpl::shared_pointer request = static_pointer_cast<ServerChannelProcessRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_PROCESS, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_PROCESS, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		request->getChannelProcess()->process(lastRequest);
	}
}

ServerChannelProcessRequesterImpl::ServerChannelProcessRequesterImpl(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport):
		BaseChannelRequester(context, channel, ioid, transport), _channelProcess()
{
}

ChannelProcessRequester::shared_pointer ServerChannelProcessRequesterImpl::create(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport,PVStructure::shared_pointer const & pvRequest)
{
    ChannelProcessRequester::shared_pointer thisPointer(new ServerChannelProcessRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerChannelProcessRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerChannelProcessRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	ChannelProcessRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_PROCESS, _channelProcess = _channel->getChannel()->createChannelProcess(thisPointer, pvRequest));
}

void ServerChannelProcessRequesterImpl::channelProcessConnect(const Status& status, ChannelProcess::shared_pointer const & channelProcess)
{
	{
		Lock guard(_mutex);
		_status = status;
		_channelProcess = channelProcess;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelProcessRequesterImpl::processDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelProcessRequesterImpl::lock()
{
	//noop
}

void ServerChannelProcessRequesterImpl::unlock()
{
	//noop
}

void ServerChannelProcessRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelProcess != NULL)
		{
			_channelProcess->destroy();
		}
	}
	// TODO
	_channelProcess.reset();
}

ChannelProcess::shared_pointer ServerChannelProcessRequesterImpl::getChannelProcess()
{
	//Lock guard(_mutex);
	return _channelProcess;
}

void ServerChannelProcessRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)CMD_PROCESS, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	{
		Lock guard(_mutex);
		_status.serialize(buffer, control);
	}

	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
	{
		destroy();
	}
}


/****************************************************************************************/
void ServerGetFieldHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8));
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		getFieldFailureResponse(transport, ioid, BaseChannelRequester::badCIDStatus);
		return;
	}

	String subField = SerializeHelper::deserializeString(payloadBuffer, transport.get());

	// issue request
	GetFieldRequester::shared_pointer gfr(new ServerGetFieldRequesterImpl(_context, channel, ioid, transport));
	// TODO exception check
	channel->getChannel()->getField(gfr, subField);
}

void ServerGetFieldHandler::getFieldFailureResponse(Transport::shared_pointer const & transport, const pvAccessID ioid, const Status& errorStatus)
{
    TransportSender::shared_pointer sender(new ServerGetFieldHandlerTransportSender(ioid,errorStatus,transport));
	transport->enqueueSendRequest(sender);
}

ServerGetFieldRequesterImpl::ServerGetFieldRequesterImpl(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
        const pvAccessID ioid, Transport::shared_pointer const & transport) :
		BaseChannelRequester(context, channel, ioid, transport), _field()
{
}

void ServerGetFieldRequesterImpl::getDone(const Status& status, FieldConstPtr const & field)
{
	{
		Lock guard(_mutex);
		_status = status;
		_field = field;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerGetFieldRequesterImpl::lock()
{
	//noop
}

void ServerGetFieldRequesterImpl::unlock()
{
	//noop
}

void ServerGetFieldRequesterImpl::destroy()
{
}

void ServerGetFieldRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	control->startMessage((int8)CMD_GET_FIELD, sizeof(int32)/sizeof(int8));
	buffer->putInt(_ioid);
	{
		Lock guard(_mutex);
		_status.serialize(buffer, control);
		control->cachedSerialize(_field, buffer);
	}
}

/****************************************************************************************/
void ServerRPCHandler::handleResponse(osiSockAddr* responseFrom,
		Transport::shared_pointer const & transport, int8 version, int8 command,
		size_t payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport::shared_pointer casTransport = dynamic_pointer_cast<ChannelHostingTransport>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl::shared_pointer channel = static_pointer_cast<ServerChannelImpl>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)CMD_RPC, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const bool init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		PVStructure::shared_pointer pvRequest(SerializationHelper::deserializePVRequest(payloadBuffer, transport.get()));

		// create...
		ServerChannelRPCRequesterImpl::create(_context, channel, ioid, transport, pvRequest);
	}
	else
	{
		const bool lastRequest = (QOS_DESTROY & qosCode) != 0;

		ServerChannelRPCRequesterImpl::shared_pointer request = static_pointer_cast<ServerChannelRPCRequesterImpl>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_RPC, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)CMD_RPC, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		// deserialize put data
		ChannelRPC::shared_pointer channelRPC = request->getChannelRPC();
		// pvArgument
		PVStructure::shared_pointer pvArgument(SerializationHelper::deserializeStructureFull(payloadBuffer, transport.get()));
		channelRPC->request(pvArgument, lastRequest);
	}
}

ServerChannelRPCRequesterImpl::ServerChannelRPCRequesterImpl(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport):
        BaseChannelRequester(context, channel, ioid, transport),
        _channelRPC(), _pvResponse()

{
}

ChannelRPCRequester::shared_pointer ServerChannelRPCRequesterImpl::create(
        ServerContextImpl::shared_pointer const & context, ServerChannelImpl::shared_pointer const & channel,
		const pvAccessID ioid, Transport::shared_pointer const & transport, PVStructure::shared_pointer const & pvRequest)
{
    ChannelRPCRequester::shared_pointer thisPointer(new ServerChannelRPCRequesterImpl(context, channel, ioid, transport));
    static_cast<ServerChannelRPCRequesterImpl*>(thisPointer.get())->activate(pvRequest);
    return thisPointer;
}

void ServerChannelRPCRequesterImpl::activate(PVStructure::shared_pointer const & pvRequest)
{
	startRequest(QOS_INIT);
	ChannelRPCRequester::shared_pointer thisPointer = shared_from_this();
	Destroyable::shared_pointer thisDestroyable = shared_from_this();
	_channel->registerRequest(_ioid, thisDestroyable);
    INIT_EXCEPTION_GUARD(CMD_RPC, _channelRPC = _channel->getChannel()->createChannelRPC(thisPointer, pvRequest));
}

void ServerChannelRPCRequesterImpl::channelRPCConnect(const Status& status, ChannelRPC::shared_pointer const & channelRPC)
{
	{
		Lock guard(_mutex);
		_status = status;
		_channelRPC = channelRPC;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelRPCRequesterImpl::requestDone(const Status& status, PVStructure::shared_pointer const & pvResponse)
{
	{
		Lock guard(_mutex);
		_status = status;
		_pvResponse = pvResponse;
	}
    TransportSender::shared_pointer thisSender = shared_from_this();
	_transport->enqueueSendRequest(thisSender);
}

void ServerChannelRPCRequesterImpl::lock()
{
	//noop
}

void ServerChannelRPCRequesterImpl::unlock()
{
	//noop
}

void ServerChannelRPCRequesterImpl::destroy()
{
	{
		Lock guard(_mutex);
		_channel->unregisterRequest(_ioid);
		if (_channelRPC != NULL)
		{
			_channelRPC->destroy();
		}
	}
	// TODO
	_channelRPC.reset();
}

ChannelRPC::shared_pointer ServerChannelRPCRequesterImpl::getChannelRPC()
{
	//Lock guard(_mutex);
	return _channelRPC;
}

void ServerChannelRPCRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)CMD_RPC, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	
	{
    	Lock guard(_mutex);
		_status.serialize(buffer, control);

    	if (_status.isSuccess())
    	{
    		if ((QOS_INIT & request) != 0)
    		{
    		    // noop
    		}
    		else
    		{
    			SerializationHelper::serializeStructureFull(buffer, control, _pvResponse);
    		}
    	}
	}
	
	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
		destroy();
}

}
}
