/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/blockingTCP.h>
#include <pv/remote.h>
#include <pv/logger.h>

#include <pv/lock.h>
#include <pv/byteBuffer.h>

/* standard */
#include <map>

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {

        BlockingServerTCPTransport::BlockingServerTCPTransport(
                Context::shared_pointer const & context, SOCKET channel,
                auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize) :
            BlockingTCPTransport(context, channel, responseHandler, receiveBufferSize, PVA_DEFAULT_PRIORITY),
            _lastChannelSID(0)
        {
            // for performance testing
            setFlushStrategy(DELAYED);
            _delay = 0.000;
            
            // NOTE: priority not yet known, default priority is used to register/unregister
            // TODO implement priorities in Reactor... not that user will
            // change it.. still getPriority() must return "registered" priority!

            //start();
        }

        BlockingServerTCPTransport::~BlockingServerTCPTransport() {
        }

        void BlockingServerTCPTransport::destroyAllChannels() {
            Lock lock(_channelsMutex);
            if(_channels.size()==0) return;

            char ipAddrStr[64];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));

            LOG(
                    logLevelDebug,
                    "Transport to %s still has %u channel(s) active and closing...",
                    ipAddrStr, (unsigned int)_channels.size());

            map<pvAccessID, ServerChannel::shared_pointer>::iterator it = _channels.begin();
            for(; it!=_channels.end(); it++)
                it->second->destroy();

            _channels.clear();
        }

        void BlockingServerTCPTransport::internalClose(bool force) {
            Transport::shared_pointer thisSharedPtr = shared_from_this();
            BlockingTCPTransport::internalClose(force);
            destroyAllChannels();
        }

        void BlockingServerTCPTransport::internalPostClose(bool forced) {
            BlockingTCPTransport::internalPostClose(forced);
        }
        
        pvAccessID BlockingServerTCPTransport::preallocateChannelSID() {
            Lock lock(_channelsMutex);
            // search first free (theoretically possible loop of death)
            pvAccessID sid = ++_lastChannelSID;
            while(_channels.find(sid)!=_channels.end())
                sid = ++_lastChannelSID;
            return sid;
        }

        void BlockingServerTCPTransport::registerChannel(pvAccessID sid, ServerChannel::shared_pointer const & channel) {
            Lock lock(_channelsMutex);
            _channels[sid] = channel;
        }

        void BlockingServerTCPTransport::unregisterChannel(pvAccessID sid) {
            Lock lock(_channelsMutex);
            _channels.erase(sid);
        }

        ServerChannel::shared_pointer BlockingServerTCPTransport::getChannel(pvAccessID sid) {
            Lock lock(_channelsMutex);

            map<pvAccessID, ServerChannel::shared_pointer>::iterator it = _channels.find(sid);
            if(it!=_channels.end()) return it->second;

            return ServerChannel::shared_pointer();
        }

        int BlockingServerTCPTransport::getChannelCount() {
            Lock lock(_channelsMutex);
            return _channels.size();
        }

        void BlockingServerTCPTransport::send(ByteBuffer* buffer,
                TransportSendControl* control) {
                    
    		//
    		// set byte order control message 
    		//
    		
    		control->ensureBuffer(PVA_MESSAGE_HEADER_SIZE);
    		buffer->putByte(PVA_MAGIC);
    		buffer->putByte(PVA_VERSION);
    		buffer->putByte(0x01 | ((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG) ? 0x80 : 0x00));		// control + big endian
    		buffer->putByte(2);		// set byte order
    		buffer->putInt(0);		
                    
                    
            //
            // send verification message
            //
            control->startMessage(CMD_CONNECTION_VALIDATION, 2*sizeof(int32));

            // receive buffer size
            buffer->putInt(getReceiveBufferSize());

            // socket receive buffer size
            buffer->putInt(getSocketReceiveBufferSize());

            // send immediately
            control->flush(true);
        }

    }
}
