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

        void BadResponse::handleResponse(osiSockAddr* responseFrom,
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

            BadResponse* badResponse = new BadResponse(context);

            _handlerTable = new ResponseHandler*[HANDLER_TABLE_LENGTH];
            // TODO add real handlers, as they are developed
            _handlerTable[0] = new NoopResponse(context, "Beacon");
            _handlerTable[1] = new ConnectionValidationHandler(context);
            _handlerTable[2] = new EchoHandler(context);
            _handlerTable[3] = new SearchHandler(context);
            _handlerTable[4] = badResponse;
            _handlerTable[5] = new IntrospectionSearchHandler(context);
            _handlerTable[6] = badResponse;
            _handlerTable[7] = new CreateChannelHandler(context);
            _handlerTable[8] = new DestroyChannelHandler(context);
            _handlerTable[9] = badResponse;
            _handlerTable[10] = badResponse;
            _handlerTable[11] = badResponse;
            _handlerTable[12] = badResponse;
            _handlerTable[13] = badResponse;
            _handlerTable[14] = badResponse;
            _handlerTable[15] = badResponse;
            _handlerTable[16] = badResponse;
            _handlerTable[17] = badResponse;
            _handlerTable[18] = badResponse;
            _handlerTable[19] = badResponse;
            _handlerTable[20] = badResponse;
            _handlerTable[21] = badResponse;
            _handlerTable[22] = badResponse;
            _handlerTable[23] = badResponse;
            _handlerTable[24] = badResponse;
            _handlerTable[25] = badResponse;
            _handlerTable[26] = badResponse;
            _handlerTable[27] = badResponse;
        }

        ServerResponseHandler::~ServerResponseHandler() {
            delete _handlerTable[0];
            delete _handlerTable[1];
            delete _handlerTable[2];
            delete _handlerTable[3];
            delete _handlerTable[5];
            delete _handlerTable[7];
            delete _handlerTable[8];
            delete _handlerTable[27];
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

        void ConnectionValidationHandler::handleResponse(
                osiSockAddr* responseFrom, Transport* transport, int8 version,
                int8 command, int payloadSize,
                epics::pvData::ByteBuffer* payloadBuffer) {
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

        class EchoTransportSender : public TransportSender {
        public:
            EchoTransportSender(osiSockAddr* echoFrom) {
                memcpy(&_echoFrom, echoFrom, sizeof(osiSockAddr));
            }

            virtual void send(ByteBuffer* buffer, TransportSendControl* control) {
                control->startMessage(CMD_ECHO, 0);
                control->setRecipient(_echoFrom);
            }

            virtual void lock() {
            }

            virtual void unlock() {
            }
            
            virtual void acquire() {
            }
            
            virtual void release() {
                delete this;
            }
            
        private:
            osiSockAddr _echoFrom;

            virtual ~EchoTransportSender() {
            }
        };

        void EchoHandler::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, epics::pvData::ByteBuffer* payloadBuffer) {
            AbstractServerResponseHandler::handleResponse(responseFrom,
                    transport, version, command, payloadSize, payloadBuffer);

            EchoTransportSender* echoReply = new EchoTransportSender(
                    responseFrom);

            // send back
            transport->enqueueSendRequest(echoReply);
        }


        void IntrospectionSearchHandler::handleResponse(osiSockAddr* responseFrom,
                Transport* transport, int8 version, int8 command,
                int payloadSize, epics::pvData::ByteBuffer* payloadBuffer) {
            AbstractServerResponseHandler::handleResponse(responseFrom,
                    transport, version, command, payloadSize, payloadBuffer);

			THROW_BASE_EXCEPTION("not implemented");
        }

        /****************************************************************************************/

        SearchHandler::SearchHandler(ServerContextImpl* context) :
    		AbstractServerResponseHandler(context, "Introspection search request")
    	{
    		_provider = context->getChannelProvider();
    		_objectPool = new ChannelFindRequesterImplObjectPool(context);
    	}

        SearchHandler::~SearchHandler()
    	{
    		if(_objectPool) delete _objectPool;
    	}

        void SearchHandler::handleResponse(osiSockAddr* responseFrom,
                  Transport* transport, int8 version, int8 command,
                  int payloadSize, epics::pvData::ByteBuffer* payloadBuffer) {
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

        ChannelFindRequesterImpl::ChannelFindRequesterImpl(ServerContextImpl* context, ChannelFindRequesterImplObjectPool* objectPool) :
				_sendTo(NULL),
         		_context(context),
         		_objectPool(objectPool)
         {}

        void ChannelFindRequesterImpl::clear()
        {
        	Lock guard(_mutex);
        	_sendTo = NULL;
        }

        ChannelFindRequesterImpl* ChannelFindRequesterImpl::set(int32 searchSequenceId, int32 cid, osiSockAddr* sendTo, boolean responseRequired)
		{
        	Lock guard(_mutex);
        	_searchSequenceId = searchSequenceId;
        	_cid = cid;
        	_sendTo = sendTo;
        	_responseRequired = responseRequired;
        	return this;
		}

        void ChannelFindRequesterImpl::channelFindResult(const epics::pvData::Status& status, ChannelFind* channelFind, boolean wasFound)
        {
        	// TODO status
        	Lock guard(_mutex);
        	if (wasFound || _responseRequired)
        	{
        		_wasFound = wasFound;
        		_context->getBroadcastTransport()->enqueueSendRequest(this);
        	}
        }

        void ChannelFindRequesterImpl::lock()
        {
        	// noop
        }

        void ChannelFindRequesterImpl::unlock()
        {
        	// noop
        }

        void ChannelFindRequesterImpl::acquire()
        {
        	// noop
        }

        void ChannelFindRequesterImpl::release()
        {
        	// noop
        }

        void ChannelFindRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
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

        ChannelFindRequesterImplObjectPool::ChannelFindRequesterImplObjectPool(ServerContextImpl* context) :
        		_context(context)
        {}

        ChannelFindRequesterImpl* ChannelFindRequesterImplObjectPool::get()
        {
        	Lock guard(_mutex);
        	const int32 count = _elements.size();
        	if (count == 0)
        	{
        		return new ChannelFindRequesterImpl(_context, this);
        	}
        	else
        	{
        		ChannelFindRequesterImpl*  channelFindRequesterImpl = _elements.back();
        		_elements.pop_back();
        		return channelFindRequesterImpl;
        	}
        }

       void ChannelFindRequesterImplObjectPool::put(ChannelFindRequesterImpl* element)
       {
    	   Lock guard(_mutex);
    	   element->clear();
    	   _elements.push_back(element);
       }

       /****************************************************************************************/

       ChannelRequesterImpl::ChannelRequesterImpl(Transport* transport, const String channelName, const pvAccessID cid) :
    		   _transport(transport),
    		   _channelName(channelName),
    		   _cid(cid),
    		   _status(),
    		   _channel(NULL)
       {

       }

       void ChannelRequesterImpl::channelCreated(const Status& status, Channel* const channel)
       {
    	   Lock guard(_mutex);
    	   _status = status;
    	   _channel = channel;
    	   _transport->enqueueSendRequest(this);
       }

       void ChannelRequesterImpl::channelStateChange(Channel*  constc, const Channel::ConnectionState isConnected)
       {
    	   //noop
       }

       String ChannelRequesterImpl::getRequesterName()
       {
    	   stringstream name;
    	   name << typeid(*_transport).name() << "/" << _cid;
    	   return name.str();
       }

       void ChannelRequesterImpl::message(const String message, const epics::pvData::MessageType messageType)
       {
    	   // TODO review
    	   errlogSevPrintf(errlogMinor, "[%s] %s", messageTypeName[messageType].c_str(), message.c_str());
       }

       void ChannelRequesterImpl::lock()
       {
    	   //noop
       }

       void ChannelRequesterImpl::unlock()
       {
    	   //noop
       }

       void ChannelRequesterImpl::send(ByteBuffer* buffer, TransportSendControl* control)
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
					//TODO implement BaseChannelRequester
					//createChannelFailedResponse(buffer, control,
					//		BaseChannelRequester.statusCreate.createStatus(StatusType.FATAL, "failed to create channel", e));
					if (serverChannel != NULL)
					{
						serverChannel->destroy();
					}
				}
				catch (...)
				{
					errlogSevPrintf(errlogMinor, "Exception caught when creating channel: %s", _channelName.c_str());
					//TODO implement BaseChannelRequester
					//createChannelFailedResponse(buffer, control,
					//		BaseChannelRequester.statusCreate.createStatus(StatusType.FATAL, "failed to create channel", e));
					if (serverChannel != NULL)
					{
						serverChannel->destroy();
					}
				}
			}
       }

       void ChannelRequesterImpl::createChannelFailedResponse(ByteBuffer* buffer, TransportSendControl* control, const Status& status)
       {
			control->startMessage((int8)7, 2*sizeof(int32)/sizeof(int8));
			buffer->putInt(_cid);
			buffer->putInt(-1);
			_transport->getIntrospectionRegistry()->serializeStatus(buffer, control, status);
       }

       /****************************************************************************************/

       void DestroyChannelHandler::handleResponse(osiSockAddr* responseFrom,
               Transport* transport, int8 version, int8 command,
               int payloadSize, epics::pvData::ByteBuffer* payloadBuffer) {
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
           transport->enqueueSendRequest( new DestroyChannelHandlerTransportSender(cid, sid));
       }
    }
}
