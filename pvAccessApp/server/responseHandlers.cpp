/*
 * responseHandlers.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

#include "responseHandlers.h"
#include "remote.h"
#include "hexDump.h"

#include <byteBuffer.h>

#include <osiSock.h>
#include <errlog.h>

#include <sstream>

using std::ostringstream;
using std::hex;

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

void ServerBadResponse::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	char ipAddrStr[48];
	ipAddrToDottedIP(&responseFrom->ia, ipAddrStr, sizeof(ipAddrStr));

	errlogSevPrintf(errlogInfo,
			"Undecipherable message (bad response type %d) from %s.",
			command, ipAddrStr);

}

ServerResponseHandler::ServerResponseHandler(ServerContextImpl* context) {

	_badResponse = new ServerBadResponse(context);

	_handlerTable = new ResponseHandler*[HANDLER_TABLE_LENGTH];
	_handlerTable[0] = new ServerNoopResponse(context, "Beacon");
	_handlerTable[1] = new ServerConnectionValidationHandler(context);
	_handlerTable[2] = new ServerEchoHandler(context);
	_handlerTable[3] = new ServerSearchHandler(context);
	_handlerTable[4] = _badResponse;
	_handlerTable[5] = new ServerIntrospectionSearchHandler(context);
	_handlerTable[6] = _badResponse;
	_handlerTable[7] = new ServerCreateChannelHandler(context);
	_handlerTable[8] = new ServerDestroyChannelHandler(context);
	_handlerTable[9] = _badResponse;
	_handlerTable[10] = new ServerGetHandler(context);
	_handlerTable[11] = new ServerPutHandler(context);
	_handlerTable[12] = new ServerPutGetHandler(context);
	_handlerTable[13] = new ServerMonitorHandler(context);
	_handlerTable[14] = new ServerArrayHandler(context);
	_handlerTable[15] = new ServerCancelRequestHandler(context);
	_handlerTable[16] = new ServerProcessHandler(context);
	_handlerTable[17] = new ServerGetFieldHandler(context);
	_handlerTable[18] = _badResponse;
	_handlerTable[19] = _badResponse;
	_handlerTable[20] = new ServerRPCHandler(context);
	_handlerTable[21] = _badResponse;
	_handlerTable[22] = _badResponse;
	_handlerTable[23] = _badResponse;
	_handlerTable[24] = _badResponse;
	_handlerTable[25] = _badResponse;
	_handlerTable[26] = _badResponse;
	_handlerTable[27] = _badResponse;
}

ServerResponseHandler::~ServerResponseHandler() {
	delete _badResponse;
	delete _handlerTable[0];
	delete _handlerTable[1];
	delete _handlerTable[2];
	delete _handlerTable[3];
	delete _handlerTable[5];
	delete _handlerTable[7];
	delete _handlerTable[8];
	delete _handlerTable[10];
	delete _handlerTable[11];
	delete _handlerTable[12];
	delete _handlerTable[13];
	delete _handlerTable[14];
	delete _handlerTable[15];
	delete _handlerTable[16];
	delete _handlerTable[17];
	delete _handlerTable[20];
	delete[] _handlerTable;
}

void ServerResponseHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	if(command<0||command>=HANDLER_TABLE_LENGTH) {
		errlogSevPrintf(errlogMinor,
				"Invalid (or unsupported) command: %x.", (0xFF&command));
		// TODO remove debug output
		ostringstream name;
		name<<"Invalid CA header "<<hex<<(int)(0xFF&command);
		name<<", its payload buffer";

		hexDump(name.str(), (const int8*)payloadBuffer->getArray(),
				payloadBuffer->getPosition(), payloadSize);
		return;
	}

	// delegate
	_handlerTable[command]->handleResponse(responseFrom, transport,
			version, command, payloadSize, payloadBuffer);
}

void ServerConnectionValidationHandler::handleResponse(
		osiSockAddr* responseFrom, Transport* transport, int8 version,
		int8 command, int payloadSize,
		ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	transport->ensureData(2*sizeof(int32)+sizeof(int16));
	transport->setRemoteTransportReceiveBufferSize(
			payloadBuffer->getInt());
	transport->setRemoteTransportSocketReceiveBufferSize(
			payloadBuffer->getInt());
	transport->setRemoteMinorRevision(version);
	// TODO support priority  !!!
	//transport.setPriority(payloadBuffer.getShort());
}

void ServerEchoHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	EchoTransportSender* echoReply = new EchoTransportSender(
			responseFrom);

	// send back
	transport->enqueueSendRequest(echoReply);
}

void ServerIntrospectionSearchHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	THROW_BASE_EXCEPTION("not implemented");
}

/****************************************************************************************/

ServerSearchHandler::ServerSearchHandler(ServerContextImpl* context) :
    										AbstractServerResponseHandler(context, "Introspection search request")
    										{
	_provider = context->getChannelProvider();
	_objectPool = new ServerChannelFindRequesterImplObjectPool(context);
    										}

ServerSearchHandler::~ServerSearchHandler()
{
	if(_objectPool) delete _objectPool;
}

void ServerSearchHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	transport->ensureData((sizeof(int32)+sizeof(int16))/sizeof(int8)+1);
	const int32 searchSequenceId = payloadBuffer->getInt();
	const int8 qosCode = payloadBuffer->getByte();
	const int32 count = payloadBuffer->getShort() & 0xFFFF;
	const boolean responseRequired = (QOS_REPLY_REQUIRED & qosCode) != 0;

	for (int32 i = 0; i < count; i++)
	{
		transport->ensureData(sizeof(int32)/sizeof(int8));
		const int32 cid = payloadBuffer->getInt();
		const String name = SerializeHelper::deserializeString(payloadBuffer, transport);
		// no name check here...

		_provider->channelFind(name, _objectPool->get()->set(searchSequenceId, cid, responseFrom, responseRequired));
	}
}

ServerChannelFindRequesterImpl::ServerChannelFindRequesterImpl(ServerContextImpl* context, ServerChannelFindRequesterImplObjectPool* objectPool) :
												_sendTo(NULL),
												_context(context),
												_objectPool(objectPool)
												{}

void ServerChannelFindRequesterImpl::clear()
{
	Lock guard(_mutex);
	_sendTo = NULL;
}

ServerChannelFindRequesterImpl* ServerChannelFindRequesterImpl::set(int32 searchSequenceId, int32 cid, osiSockAddr* sendTo, boolean responseRequired)
{
	Lock guard(_mutex);
	_searchSequenceId = searchSequenceId;
	_cid = cid;
	_sendTo = sendTo;
	_responseRequired = responseRequired;
	return this;
}

void ServerChannelFindRequesterImpl::channelFindResult(const Status& status, ChannelFind* channelFind, boolean wasFound)
{
	// TODO status
	Lock guard(_mutex);
	if (wasFound || _responseRequired)
	{
		_wasFound = wasFound;
		_context->getBroadcastTransport()->enqueueSendRequest(this);
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
	control->startMessage((int8)4, (sizeof(int32)+sizeof(int8)+128+2*sizeof(int16)+count*sizeof(int32))/sizeof(8));

	Lock guard(_mutex);
	buffer->putInt(_searchSequenceId);
	buffer->putByte(_wasFound ? (int8)1 : (int8)0);

	// NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
	encodeAsIPv6Address(buffer, _context->getServerInetAddress());
	buffer->putShort((int16)_context->getServerPort());
	buffer->putShort((int16)count);
	buffer->putInt(_cid);

	control->setRecipient(*_sendTo);

	// return this object to the pool
	_objectPool->put(this);
}

ServerChannelFindRequesterImplObjectPool::ServerChannelFindRequesterImplObjectPool(ServerContextImpl* context) :
        										_context(context)
{}

ServerChannelFindRequesterImplObjectPool::~ServerChannelFindRequesterImplObjectPool()
{
	for(std::vector<ServerChannelFindRequesterImpl*>::iterator iter = _elements.begin();
			iter != _elements.end(); iter++)
	{
		delete *iter;
	}
	_elements.erase(_elements.begin(), _elements.end());
}

ServerChannelFindRequesterImpl* ServerChannelFindRequesterImplObjectPool::get()
{
	Lock guard(_mutex);
	const int32 count = _elements.size();
	if (count == 0)
	{
		return new ServerChannelFindRequesterImpl(_context, this);
	}
	else
	{
		ServerChannelFindRequesterImpl*  channelFindRequesterImpl = _elements.back();
		_elements.pop_back();
		return channelFindRequesterImpl;
	}
}

void ServerChannelFindRequesterImplObjectPool::put(ServerChannelFindRequesterImpl* element)
{
	Lock guard(_mutex);
	element->clear();
	_elements.push_back(element);
}

/****************************************************************************************/
void ServerCreateChannelHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
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

	String channelName = SerializeHelper::deserializeString(payloadBuffer, transport);
	if (channelName.size() == 0)
	{

		char host[100];
		sockAddrToA(&transport->getRemoteAddress()->sa,host,100);
		errlogSevPrintf(errlogMinor,"Zero length channel name, disconnecting client: %s", host);
		disconnect(transport);
		return;
	}
	else if (channelName.size() > UNREASONABLE_CHANNEL_NAME_LENGTH)
	{
		char host[100];
		sockAddrToA(&transport->getRemoteAddress()->sa,host,100);
		errlogSevPrintf(errlogMinor,"Unreasonable channel name length, disconnecting client: %s", host);
		disconnect(transport);
		return;
	}

	ChannelRequester* cr = new ServerChannelRequesterImpl(transport, channelName, cid);
	_provider->createChannel(channelName, cr, transport->getPriority());
}

void ServerCreateChannelHandler::disconnect(Transport* transport)
{
	transport->close(true);
}

ServerChannelRequesterImpl::ServerChannelRequesterImpl(Transport* transport, const String channelName, const pvAccessID cid) :
    										   _transport(transport),
    										   _channelName(channelName),
    										   _cid(cid),
    										   _status(),
    										   _channel(NULL)
{

}

void ServerChannelRequesterImpl::channelCreated(const Status& status, Channel* channel)
{
	Lock guard(_mutex);
	_status = status;
	_channel = channel;
	_transport->enqueueSendRequest(this);
}

void ServerChannelRequesterImpl::channelStateChange(Channel* c, const Channel::ConnectionState isConnected)
{
	//noop
}

String ServerChannelRequesterImpl::getRequesterName()
{
	stringstream name;
	name << typeid(*_transport).name() << "/" << _cid;
	return name.str();
}

void ServerChannelRequesterImpl::message(const String message, const MessageType messageType)
{
	errlogSevPrintf(errlogMinor, "[%s] %s", messageTypeName[messageType].c_str(), message.c_str());
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
	Channel* channel;
	Status status;
	{
		Lock guard(_mutex);
		channel = _channel;
		status = _status;
	}

	// error response
	if (channel == NULL)
	{
		createChannelFailedResponse(buffer, control, status);
	}
	// OK
	else
	{
		ServerChannelImpl* serverChannel = NULL;
		try
		{
			// NOTE: we do not explicitly check if transport OK
			ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(_transport);

			//
			// create a new channel instance
			//
			pvAccessID sid = casTransport->preallocateChannelSID();
			try
			{
				serverChannel = new ServerChannelImpl(channel, _cid, sid, casTransport->getSecurityToken());

				// ack allocation and register
				casTransport->registerChannel(sid, serverChannel);

			} catch (...)
			{
				// depreallocate and rethrow
				casTransport->depreallocateChannelSID(sid);
				throw;
			}

			control->startMessage((int8)7, 2*sizeof(int32)/sizeof(int8));
			buffer->putInt(_cid);
			buffer->putInt(sid);
			_transport->getIntrospectionRegistry()->serializeStatus(buffer, control, status);
		}
		catch (std::exception& e)
		{
			errlogSevPrintf(errlogMinor, "Exception caught when creating channel: %s", _channelName.c_str());
			createChannelFailedResponse(buffer, control, Status(Status::STATUSTYPE_FATAL,  "failed to create channel", e.what()));
			if (serverChannel != NULL)
			{
				serverChannel->destroy();
			}
		}
		catch (...)
		{
			errlogSevPrintf(errlogMinor, "Exception caught when creating channel: %s", _channelName.c_str());
			createChannelFailedResponse(buffer, control, Status(Status::STATUSTYPE_FATAL,  "failed to create channel"));
			if (serverChannel != NULL)
			{
				serverChannel->destroy();
			}
		}
	}
}


void ServerChannelRequesterImpl::createChannelFailedResponse(ByteBuffer* buffer, TransportSendControl* control, const Status& status)
{
	control->startMessage((int8)7, 2*sizeof(int32)/sizeof(int8));
	buffer->putInt(_cid);
	buffer->putInt(-1);
	_transport->getIntrospectionRegistry()->serializeStatus(buffer, control, status);
}

/****************************************************************************************/

void ServerDestroyChannelHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);


	transport->ensureData(2*sizeof(int32)/sizeof(int8));
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID cid = payloadBuffer->getInt();

	// get channel by SID
	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		if (!transport->isClosed())
		{
			char host[100];
			sockAddrToA(&responseFrom->sa,host,100);
			errlogSevPrintf(errlogMinor, "Trying to destroy a channel that no longer exists (SID: %d, CID %d, client: %s).", sid, cid, host);
		}
		return;
	}

	// destroy
	channel->destroy();

	// .. and unregister
	casTransport->unregisterChannel(sid);

	// send response back
	transport->enqueueSendRequest(new ServerDestroyChannelHandlerTransportSender(cid, sid));
}

/****************************************************************************************/

void ServerGetHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)10, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructurePtr pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerChannelGetRequesterImpl(_context, channel, ioid, transport, pvRequest);
		
		delete pvRequest;
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;

		ServerChannelGetRequesterImpl* request = static_cast<ServerChannelGetRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)10, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)10, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		request->getChannelGet()->get(lastRequest);
	}
}

ServerChannelGetRequesterImpl::ServerChannelGetRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport,
		PVStructurePtr pvRequest) :
		BaseChannelRequester(context, channel, ioid, transport)
{
	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelGet = channel->getChannel()->createChannelGet(this, pvRequest);
	// TODO what if last call fails... registration is still present
}

void ServerChannelGetRequesterImpl::channelGetConnect(const Status& status, ChannelGet* channelGet, PVStructurePtr pvStructure,
		BitSet* bitSet)
{
	{
		Lock guard(_mutex);
		_bitSet = bitSet;
		_pvStructure = pvStructure;
		_status = status;
		_channelGet = channelGet;
	}
	_transport->enqueueSendRequest(this);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelGetRequesterImpl::getDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	_transport->enqueueSendRequest(this);
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
	release();
}

ChannelGet* ServerChannelGetRequesterImpl::getChannelGet()
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

	control->startMessage((int8)10, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->put((int8)request);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
	}

	if (_status.isSuccess())
	{
		if (request & QOS_INIT)
		{
			Lock guard(_mutex);
			introspectionRegistry->serialize(_pvStructure != NULL ? _pvStructure->getField() : NULL, buffer, control);

		}
		else
		{
			_bitSet->serialize(buffer, control);
			_pvStructure->serialize(buffer, control, _bitSet);
		}
	}

	stopRequest();

	// lastRequest
	if (request & QOS_DESTROY)
	{
		destroy();
	}
}
/****************************************************************************************/
void ServerPutHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);


	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)11, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructure* pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerChannelPutRequesterImpl(_context, channel, ioid, transport, pvRequest);

        delete pvRequest;		
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;
		const boolean get = (QOS_GET & qosCode) != 0;

		ServerChannelPutRequesterImpl* request = static_cast<ServerChannelPutRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)11, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)11, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
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
			BitSet* putBitSet = request->getBitSet();
			putBitSet->deserialize(payloadBuffer, transport);
			request->getPVStructure()->deserialize(payloadBuffer, transport, putBitSet);
			request->getChannelPut()->put(lastRequest);
		}
	}
}

ServerChannelPutRequesterImpl::ServerChannelPutRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel,
		const pvAccessID ioid, Transport* transport,PVStructure* pvRequest):
		BaseChannelRequester(context, channel, ioid, transport)
{
	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelPut = channel->getChannel()->createChannelPut(this, pvRequest);
	// TODO what if last call fails... registration is still present
}

void ServerChannelPutRequesterImpl::channelPutConnect(const Status& status, ChannelPut* channelPut, PVStructure* pvStructure, BitSet* bitSet)
{
	{
		Lock guard(_mutex);
		_bitSet = bitSet;
		_pvStructure = pvStructure;
		_status = status;
		_channelPut = channelPut;
	}

	_transport->enqueueSendRequest(this);

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
	_transport->enqueueSendRequest(this);
}

void ServerChannelPutRequesterImpl::getDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	_transport->enqueueSendRequest(this);
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
	release();
}

ChannelPut* ServerChannelPutRequesterImpl::getChannelPut()
{
	Lock guard(_mutex);
	return _channelPut;
}

BitSet* ServerChannelPutRequesterImpl::getBitSet()
{
	Lock guard(_mutex);
	return _bitSet;
}

PVStructure* ServerChannelPutRequesterImpl::getPVStructure()
{
	Lock guard(_mutex);
	return _pvStructure;
}

void ServerChannelPutRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)11, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
	}

	if (_status.isSuccess())
	{
		if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
			introspectionRegistry->serialize(_pvStructure != NULL ? _pvStructure->getField() : NULL, buffer, control);
		}
		else if ((QOS_GET & request) != 0)
		{
			Lock guard(_mutex);
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
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)12, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructure* pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerChannelPutGetRequesterImpl(_context, channel, ioid, transport, pvRequest);
		
		delete pvRequest;
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;
		const boolean getGet = (QOS_GET & qosCode) != 0;
		const boolean getPut = (QOS_GET_PUT & qosCode) != 0;

		ServerChannelPutGetRequesterImpl* request = static_cast<ServerChannelPutGetRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)12, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)12, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
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
			request->getPVPutStructure()->deserialize(payloadBuffer, transport);
			request->getChannelPutGet()->putGet(lastRequest);
		}
	}
}

ServerChannelPutGetRequesterImpl::ServerChannelPutGetRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel,
		const pvAccessID ioid, Transport* transport,PVStructure* pvRequest):
		BaseChannelRequester(context, channel, ioid, transport)
{
	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelPutGet = channel->getChannel()->createChannelPutGet(this, pvRequest);
	// TODO what if last call fails... registration is still present
}

void ServerChannelPutGetRequesterImpl::channelPutGetConnect(const Status& status, ChannelPutGet* channelPutGet,
		PVStructure* pvPutStructure, PVStructure* pvGetStructure)
{
	{
		Lock guard(_mutex);
		_pvPutStructure = pvPutStructure;
		_pvGetStructure = pvGetStructure;
		_status = status;
		_channelPutGet = channelPutGet;
	}

	_transport->enqueueSendRequest(this);

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
	_transport->enqueueSendRequest(this);
}

void ServerChannelPutGetRequesterImpl::getPutDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	_transport->enqueueSendRequest(this);
}

void ServerChannelPutGetRequesterImpl::putGetDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	_transport->enqueueSendRequest(this);
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
	release();
}

ChannelPutGet* ServerChannelPutGetRequesterImpl::getChannelPutGet()
{
	Lock guard(_mutex);
	return _channelPutGet;
}

PVStructure* ServerChannelPutGetRequesterImpl::getPVPutStructure()
{
	Lock guard(_mutex);
	return _pvPutStructure;
}

void ServerChannelPutGetRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)12, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
	}

	if (_status.isSuccess())
	{
		if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
			introspectionRegistry->serialize(_pvPutStructure != NULL ? _pvPutStructure->getField() : NULL, buffer, control);
			introspectionRegistry->serialize(_pvGetStructure != NULL ? _pvGetStructure->getField() : NULL, buffer, control);
		}
		else if ((QOS_GET & request) != 0)
		{
			Lock guard(_mutex);
			_pvGetStructure->serialize(buffer, control);
		}
		else if ((QOS_GET_PUT & request) != 0)
		{
			Lock guard(_mutex);
			_pvPutStructure->serialize(buffer, control);
		}
		else
		{
			Lock guard(_mutex);
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
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)12, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructure* pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerMonitorRequesterImpl(_context, channel, ioid, transport, pvRequest);
		
		delete pvRequest;
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;
		const boolean get = (QOS_GET & qosCode) != 0;
		const boolean process = (QOS_PROCESS & qosCode) != 0;

		ServerMonitorRequesterImpl* request = static_cast<ServerMonitorRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)13, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)13, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}


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

ServerMonitorRequesterImpl::ServerMonitorRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel,
		const pvAccessID ioid, Transport* transport,PVStructure* pvRequest):
		BaseChannelRequester(context, channel, ioid, transport)
{
	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelMonitor = channel->getChannel()->createMonitor(this, pvRequest);
	// TODO what if last call fails... registration is still present
}

void ServerMonitorRequesterImpl::monitorConnect(const Status& status, Monitor* monitor, Structure* structure)
{
	{
		Lock guard(_mutex);
		_status = status;
		_monitor = monitor;
		_structure = structure;
		_monitor = monitor;
	}
	_transport->enqueueSendRequest(this);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerMonitorRequesterImpl::unlisten(Monitor* monitor)
{
	//TODO
}

void ServerMonitorRequesterImpl::monitorEvent(Monitor* monitor)
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
	_transport->enqueueSendRequest(this);
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
	release();
}

Monitor* ServerMonitorRequesterImpl::getChannelMonitor()
{
	Lock guard(_mutex);
	return _channelMonitor;
}

void ServerMonitorRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	if ((QOS_INIT & request) != 0)
	{
		control->startMessage((int32)13, sizeof(int32)/sizeof(int8) + 1);
		buffer->putInt(_ioid);
		buffer->putByte((int8)request);

		IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
		{
			Lock guard(_mutex);
			introspectionRegistry->serializeStatus(buffer, control, _status);
		}

		if (_status.isSuccess())
		{
			introspectionRegistry->serialize(_structure, buffer, control);
		}
		stopRequest();
		startRequest(QOS_DEFAULT);
	}
	else
	{
		Monitor* monitor = _monitor;
		MonitorElement* element = monitor->poll();
		if (element != NULL)
		{
			control->startMessage((int8)13, sizeof(int32)/sizeof(int8) + 1);
			buffer->putInt(_ioid);
			buffer->putByte((int8)request);

			// changedBitSet and data, if not notify only (i.e. queueSize == -1)
			BitSet* changedBitSet = element->getChangedBitSet();
			if (changedBitSet != NULL)
			{
				changedBitSet->serialize(buffer, control);
				element->getPVStructure()->serialize(buffer, control, changedBitSet);

				// overrunBitset
				element->getOverrunBitSet()->serialize(buffer, control);
			}

			monitor->release(element);
		}
	}
}

/****************************************************************************************/
void ServerArrayHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)12, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructure* pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerChannelArrayRequesterImpl(_context, channel, ioid, transport, pvRequest);
		
		delete pvRequest;
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;
		const boolean get = (QOS_GET & qosCode) != 0;
		const boolean setLength = (QOS_GET_PUT & qosCode) != 0;

		ServerChannelArrayRequesterImpl* request = static_cast<ServerChannelArrayRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)14, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)14, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}


		if (get)
		{
			const int32 offset = SerializeHelper::readSize(payloadBuffer, transport);
			const int32 count = SerializeHelper::readSize(payloadBuffer, transport);
			request->getChannelArray()->getArray(lastRequest, offset, count);
		}
		else if (setLength)
		{
			const int32 length = SerializeHelper::readSize(payloadBuffer, transport);
			const int32 capacity = SerializeHelper::readSize(payloadBuffer, transport);
			request->getChannelArray()->setLength(lastRequest, length, capacity);
		}
		else
		{
			// deserialize data to put
			const int32 offset = SerializeHelper::readSize(payloadBuffer, transport);
			PVArray* array = request->getPVArray();
			array->deserialize(payloadBuffer, transport);
			request->getChannelArray()->putArray(lastRequest, offset, array->getLength());
		}
	}
}

ServerChannelArrayRequesterImpl::ServerChannelArrayRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel,
		const pvAccessID ioid, Transport* transport,PVStructure* pvRequest):
		BaseChannelRequester(context, channel, ioid, transport)
{

	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelArray = channel->getChannel()->createChannelArray(this, pvRequest);
	// TODO what if last call fails... registration is still present
}

void ServerChannelArrayRequesterImpl::channelArrayConnect(const Status& status, ChannelArray* channelArray, PVArray* pvArray)
{
	{
		Lock guard(_mutex);
		_status = status;
		_pvArray = pvArray;
		_channelArray = channelArray;
	}

	_transport->enqueueSendRequest(this);

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
	_transport->enqueueSendRequest(this);
}

void ServerChannelArrayRequesterImpl::putArrayDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	_transport->enqueueSendRequest(this);
}

void ServerChannelArrayRequesterImpl::setLengthDone(const Status& status)
{
	{
		Lock guard(_mutex);
		_status = status;
	}
	_transport->enqueueSendRequest(this);
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
	release();
}

ChannelArray* ServerChannelArrayRequesterImpl::getChannelArray()
{
	Lock guard(_mutex);
	return _channelArray;
}

PVArray* ServerChannelArrayRequesterImpl::getPVArray()
{
	Lock guard(_mutex);
	return _pvArray;
}

void ServerChannelArrayRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)14, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
	}

	if (_status.isSuccess())
	{
		if ((QOS_GET & request) != 0)
		{
			Lock guard(_mutex);
			_pvArray->serialize(buffer, control, 0, _pvArray->getLength());
		}
		else if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
			introspectionRegistry->serialize(_pvArray != NULL ? _pvArray->getField() : NULL, buffer, control);
		}
	}

	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
		destroy();
}

/****************************************************************************************/
void ServerCancelRequestHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		failureResponse(transport, ioid, BaseChannelRequester::badCIDStatus);
		return;
	}

	Destroyable* request = channel->getRequest(ioid);
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

void ServerCancelRequestHandler::failureResponse(Transport* transport, pvAccessID ioid, const Status& errorStatus)
{
	BaseChannelRequester::message(transport, ioid, errorStatus.getMessage(), warningMessage);
}

/****************************************************************************************/
void ServerProcessHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)16, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructure* pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerChannelProcessRequesterImpl(_context, channel, ioid, transport, pvRequest);
		
		delete pvRequest;
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;

		ServerChannelProcessRequesterImpl* request = static_cast<ServerChannelProcessRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)16, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)16, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		request->getChannelProcess()->process(lastRequest);
	}
}

ServerChannelProcessRequesterImpl::ServerChannelProcessRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel,
		const pvAccessID ioid, Transport* transport,PVStructure* pvRequest): BaseChannelRequester(context, channel, ioid, transport),
		_refCount(1)
{
	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelProcess = channel->getChannel()->createChannelProcess(this, pvRequest);
	// TODO what if last call fails... registration is still present
}

void ServerChannelProcessRequesterImpl::channelProcessConnect(const Status& status, ChannelProcess* channelProcess)
{
	{
		Lock guard(_mutex);
		_status = status;
		_channelProcess = channelProcess;
	}
	_transport->enqueueSendRequest(this);

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
	_transport->enqueueSendRequest(this);
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
	release();
}

ChannelProcess* ServerChannelProcessRequesterImpl::getChannelProcess()
{
	Lock guard(_mutex);
	return _channelProcess;
}

void ServerChannelProcessRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)16, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
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
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8));
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		getFieldFailureResponse(transport, ioid, BaseChannelRequester::badCIDStatus);
		return;
	}

	String subField = SerializeHelper::deserializeString(payloadBuffer, transport);

	// issue request
	channel->getChannel()->getField(new ServerGetFieldRequesterImpl(_context, channel, ioid, transport), subField);
}

void ServerGetFieldHandler::getFieldFailureResponse(Transport* transport, const pvAccessID ioid, const Status& errorStatus)
{
	transport->enqueueSendRequest(new ServerGetFieldHandlerTransportSender(ioid,errorStatus,transport));
}

ServerGetFieldRequesterImpl::ServerGetFieldRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel, const pvAccessID ioid, Transport* transport) :
							BaseChannelRequester(context, channel, ioid, transport)
{
}

void ServerGetFieldRequesterImpl::getDone(const Status& status, FieldConstPtr field)
{
	{
		Lock guard(_mutex);
		_status = status;
		_field = field;
	}
	_transport->enqueueSendRequest(this);
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
	release();
}

void ServerGetFieldRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	control->startMessage((int8)17, sizeof(int32)/sizeof(int8));
	buffer->putInt(_ioid);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
		introspectionRegistry->serialize(_field, buffer, control);
	}
}

/****************************************************************************************/
void ServerRPCHandler::handleResponse(osiSockAddr* responseFrom,
		Transport* transport, int8 version, int8 command,
		int payloadSize, ByteBuffer* payloadBuffer) {
	AbstractServerResponseHandler::handleResponse(responseFrom,
			transport, version, command, payloadSize, payloadBuffer);

	// NOTE: we do not explicitly check if transport is OK
	ChannelHostingTransport* casTransport = dynamic_cast<ChannelHostingTransport*>(transport);

	transport->ensureData(2*sizeof(int32)/sizeof(int8)+1);
	const pvAccessID sid = payloadBuffer->getInt();
	const pvAccessID ioid = payloadBuffer->getInt();

	// mode
	const int8 qosCode = payloadBuffer->getByte();

	ServerChannelImpl* channel = static_cast<ServerChannelImpl*>(casTransport->getChannel(sid));
	if (channel == NULL)
	{
		BaseChannelRequester::sendFailureMessage((int8)16, transport, ioid, qosCode, BaseChannelRequester::badCIDStatus);
		return;
	}

	const boolean init = (QOS_INIT & qosCode) != 0;
	if (init)
	{
		// pvRequest
		//TODO who is responsible to delete this pvRequest??
		PVStructure* pvRequest = transport->getIntrospectionRegistry()->deserializePVRequest(payloadBuffer, transport);

		// create...
		new ServerChannelRPCRequesterImpl(_context, channel, ioid, transport, pvRequest);
		
		delete pvRequest;
	}
	else
	{
		const boolean lastRequest = (QOS_DESTROY & qosCode) != 0;

		ServerChannelRPCRequesterImpl* request = static_cast<ServerChannelRPCRequesterImpl*>(channel->getRequest(ioid));
		if (request == NULL)
		{
			BaseChannelRequester::sendFailureMessage((int8)20, transport, ioid, qosCode, BaseChannelRequester::badIOIDStatus);
			return;
		}

		if (!request->startRequest(qosCode))
		{
			BaseChannelRequester::sendFailureMessage((int8)20, transport, ioid, qosCode, BaseChannelRequester::otherRequestPendingStatus);
			return;
		}

		// deserialize put data
		BitSet* changedBitSet = request->getAgrumentsBitSet();
		changedBitSet->deserialize(payloadBuffer, transport);
		request->getPvArguments()->deserialize(payloadBuffer, transport, changedBitSet);
		request->getChannelRPC()->request(lastRequest);
	}
}

ServerChannelRPCRequesterImpl::ServerChannelRPCRequesterImpl(ServerContextImpl* context, ServerChannelImpl* channel,
		const pvAccessID ioid, Transport* transport,PVStructure* pvRequest):
BaseChannelRequester(context, channel, ioid, transport)
{
	startRequest(QOS_INIT);
	channel->registerRequest(ioid, static_cast<Destroyable*>(this));
	_channelRPC = channel->getChannel()->createChannelRPC(this, pvRequest);

}

void ServerChannelRPCRequesterImpl::channelRPCConnect(const Status& status, ChannelRPC* channelRPC, PVStructure* arguments, BitSet* bitSet)
{
	{
		Lock guard(_mutex);
		_pvArguments = arguments;
		_argumentsBitSet = bitSet;
		_status = status;
	}
	_transport->enqueueSendRequest(this);

	// self-destruction
	if (!status.isSuccess())
	{
		destroy();
	}
}

void ServerChannelRPCRequesterImpl::requestDone(const Status& status, PVStructure* pvResponse)
{
	{
		Lock guard(_mutex);
		_status = status;
		_pvResponse = pvResponse;
	}
	_transport->enqueueSendRequest(this);
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
	release();
}

ChannelRPC* ServerChannelRPCRequesterImpl::getChannelRPC()
{
	Lock guard(_mutex);
	return _channelRPC;
}

PVStructure* ServerChannelRPCRequesterImpl::getPvArguments()
{
	Lock guard(_mutex);
	return _pvArguments;
}

BitSet* ServerChannelRPCRequesterImpl::getAgrumentsBitSet()
{
	Lock guard(_mutex);
	return _argumentsBitSet;
}

void ServerChannelRPCRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
{
	const int32 request = getPendingRequest();

	control->startMessage((int32)20, sizeof(int32)/sizeof(int8) + 1);
	buffer->putInt(_ioid);
	buffer->putByte((int8)request);
	IntrospectionRegistry* introspectionRegistry = _transport->getIntrospectionRegistry();
	{
		Lock guard(_mutex);
		introspectionRegistry->serializeStatus(buffer, control, _status);
	}

	if (_status.isSuccess())
	{
		if ((QOS_INIT & request) != 0)
		{
			Lock guard(_mutex);
			introspectionRegistry->serialize(_pvArguments != NULL ? _pvArguments->getField() : NULL, buffer, control);
		}
		else
		{
			introspectionRegistry->serializeStructure(buffer, control, _pvResponse);
		}
	}

	stopRequest();

	// lastRequest
	if ((QOS_DESTROY & request) != 0)
		destroy();
}

}
}
