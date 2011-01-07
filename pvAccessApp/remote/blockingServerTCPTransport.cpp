/*
 * blockingServerTCPTransport.cpp
 *
 *  Created on: Jan 4, 2011
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingTCP.h"
#include "remote.h"

/* pvData */
#include <lock.h>
#include <byteBuffer.h>

/* EPICSv3 */
#include <errlog.h>

/* standard */
#include <map>

using namespace epics::pvData;
using std::map;

namespace epics {
    namespace pvAccess {

        BlockingServerTCPTransport::BlockingServerTCPTransport(
                Context* context, SOCKET channel,
                ResponseHandler* responseHandler, int receiveBufferSize) :
            BlockingTCPTransport(context, channel, responseHandler,
                    receiveBufferSize, CA_DEFAULT_PRIORITY),
                    _introspectionRegistry(new IntrospectionRegistry(true)),
                    _lastChannelSID(0), _channels(
                            new map<int, ServerChannel*> ()), _channelsMutex(
                            new Mutex()) {
            // NOTE: priority not yet known, default priority is used to register/unregister
            // TODO implement priorities in Reactor... not that user will
            // change it.. still getPriority() must return "registered" priority!

            start();
        }

        BlockingServerTCPTransport::~BlockingServerTCPTransport() {
            delete _introspectionRegistry;
            delete _channels;
            delete _channelsMutex;
        }

        void BlockingServerTCPTransport::destroyAllChannels() {
            Lock lock(_channelsMutex);
            if(_channels->size()==0) return;

            char ipAddrStr[64];
            ipAddrToA(&_socketAddress->ia, ipAddrStr, sizeof(ipAddrStr));

            errlogSevPrintf(
                    errlogInfo,
                    "Transport to %s still has %d channel(s) active and closing...",
                    ipAddrStr, _channels->size());

            map<int, ServerChannel*>::iterator it = _channels->begin();
            for(; it!=_channels->end(); it++)
                it->second->destroy();

            _channels->clear();
        }

        void BlockingServerTCPTransport::internalClose(bool force) {
            BlockingTCPTransport::internalClose(force);
            destroyAllChannels();
        }

        int BlockingServerTCPTransport::preallocateChannelSID() {
            Lock lock(_channelsMutex);
            // search first free (theoretically possible loop of death)
            int sid = ++_lastChannelSID;
            while(_channels->find(sid)!=_channels->end())
                sid = ++_lastChannelSID;
            return sid;
        }

        void BlockingServerTCPTransport::registerChannel(int sid,
                ServerChannel* channel) {
            Lock lock(_channelsMutex);
            (*_channels)[sid] = channel;
        }

        void BlockingServerTCPTransport::unregisterChannel(int sid) {
            Lock lock(_channelsMutex);
            _channels->erase(sid);
        }

        ServerChannel* BlockingServerTCPTransport::getChannel(int sid) {
            Lock lock(_channelsMutex);

            map<int, ServerChannel*>::iterator it = _channels->find(sid);
            if(it!=_channels->end()) return it->second;

            return NULL;
        }

        int BlockingServerTCPTransport::getChannelCount() {
            Lock lock(_channelsMutex);
            return _channels->size();
        }

        void BlockingServerTCPTransport::send(ByteBuffer* buffer,
                TransportSendControl* control) {
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
