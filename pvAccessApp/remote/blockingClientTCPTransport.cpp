/*
 * BlockingClientTCPTransport.cpp
 *
 *  Created on: Jan 3, 2011
 *      Author: Miha Vitorovic
 */

/* pvAccess */
#include "blockingTCP.h"

#include "introspectionRegistry.h"

/* pvData */
#include <lock.h>

/* EPICSv3 */
#include <errlog.h>

/* standard */
#include <set>
#include <epicsTime.h>
#include <sstream>

using namespace std;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { errlogSevPrintf(errlogMajor, "Unhandled exception caught from code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { errlogSevPrintf(errlogMajor, "Unhandled exception caught from code at %s:%d.", __FILE__, __LINE__); }
                
        BlockingClientTCPTransport::BlockingClientTCPTransport(
                Context::shared_pointer& context, SOCKET channel,
                auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize,
                TransportClient::shared_pointer client, short remoteTransportRevision,
                float beaconInterval, int16 priority) :
            BlockingTCPTransport(context, channel, responseHandler, receiveBufferSize, priority),
                    _introspectionRegistry(false),
                    _connectionTimeout(beaconInterval*1000),
                    _unresponsiveTransport(false),
                    _timerNode(*this),
                    _verifyOrEcho(true)
        {
//            _autoDelete = false;

            // initialize owners list, send queue
            acquire(client);

            // use immediate for clients
            setSendQueueFlushStrategy(IMMEDIATE);

            // setup connection timeout timer (watchdog)
            epicsTimeGetCurrent(&_aliveTimestamp);

            context->getTimer()->schedulePeriodic(_timerNode, beaconInterval, beaconInterval);

            //start();

        }

        BlockingClientTCPTransport::~BlockingClientTCPTransport() {
        }

        void BlockingClientTCPTransport::callback() {
            epicsTimeStamp currentTime;
            epicsTimeGetCurrent(&currentTime);

            _ownersMutex.lock();
            // no exception expected here
            double diff = epicsTimeDiffInSeconds(&currentTime, &_aliveTimestamp);
            _ownersMutex.unlock();
            
            if(diff>2*_connectionTimeout) {
                unresponsiveTransport();
            }
            // use some k (3/4) to handle "jitter"
            else if(diff>=((3*_connectionTimeout)/4)) {
                // send echo
                TransportSender::shared_pointer transportSender = std::tr1::dynamic_pointer_cast<TransportSender>(shared_from_this());
                enqueueSendRequest(transportSender);
            }
        }

        void BlockingClientTCPTransport::unresponsiveTransport() {
            Lock lock(_ownersMutex);
            if(!_unresponsiveTransport) {
                _unresponsiveTransport = true;

                TransportClientMap_t::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient::shared_pointer client = it->second.lock();
                    if (client)
                    {
                        EXCEPTION_GUARD(client->transportUnresponsive());
                    }
                }
            }
        }

        bool BlockingClientTCPTransport::acquire(TransportClient::shared_pointer& client) {
            Lock lock(_mutex);
            if(_closed) return false;
            
            char ipAddrStr[48];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
            errlogSevPrintf(errlogInfo, "Acquiring transport to %s.", ipAddrStr);

            Lock lock2(_ownersMutex);
// TODO double check?            if(_closed) return false;
            //_owners.insert(TransportClient::weak_pointer(client));
            _owners[client->getID()] = TransportClient::weak_pointer(client);

            return true;
        }

        void BlockingClientTCPTransport::internalClose(bool forced) {
            BlockingTCPTransport::internalClose(forced);

            _timerNode.cancel();

            closedNotifyClients();
        }

        /**
         * Notifies clients about disconnect.
         */
        void BlockingClientTCPTransport::closedNotifyClients() {
            Lock lock(_ownersMutex);

            // check if still acquired
            int refs = _owners.size();
            if(refs>0) {
                char ipAddrStr[48];
                ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
                errlogSevPrintf(
                        errlogInfo,
                        "Transport to %s still has %d client(s) active and closing...",
                        ipAddrStr, refs);

                TransportClientMap_t::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient::shared_pointer client = it->second.lock();
                    if (client)
                    {
                        EXCEPTION_GUARD(client->transportClosed());
                    }
                }
                
            }

            _owners.clear();
        }

        //void BlockingClientTCPTransport::release(TransportClient::shared_pointer& client) {
        void BlockingClientTCPTransport::release(pvAccessID clientID) {
            Lock lock(_mutex);
            if(_closed) return;
            
            char ipAddrStr[48];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));

            errlogSevPrintf(errlogInfo, "Releasing transport to %s.", ipAddrStr);

            Lock lock2(_ownersMutex);
            _owners.erase(clientID);
            //_owners.erase(TransportClient::weak_pointer(client));
            
            // not used anymore, close it
            // TODO consider delayed destruction (can improve performance!!!)
            if(_owners.size()==0) close(false);
        }

        void BlockingClientTCPTransport::aliveNotification() {
            Lock guard(_ownersMutex);
            epicsTimeGetCurrent(&_aliveTimestamp);
            if(_unresponsiveTransport) responsiveTransport();
        }

        void BlockingClientTCPTransport::responsiveTransport() {
            Lock lock(_ownersMutex);
            if(_unresponsiveTransport) {
                _unresponsiveTransport = false;

                Transport::shared_pointer thisSharedPtr = shared_from_this();
                TransportClientMap_t::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient::shared_pointer client = it->second.lock();
                    if (client)
                    {
                        EXCEPTION_GUARD(client->transportResponsive(thisSharedPtr));
                    }
                }
            }
        }

        void BlockingClientTCPTransport::changedTransport() {
            _introspectionRegistry.reset();

            Lock lock(_ownersMutex);
            TransportClientMap_t::iterator it = _owners.begin();
            for(; it!=_owners.end(); it++) {
                TransportClient::shared_pointer client = it->second.lock();
                if (client)
                {
                    EXCEPTION_GUARD(client->transportChanged());
                }
            }
        }

        void BlockingClientTCPTransport::send(ByteBuffer* buffer,
                TransportSendControl* control) {
            if(_verifyOrEcho) {
                /*
                 * send verification response message
                 */

                control->startMessage(CMD_CONNECTION_VALIDATION, 2*sizeof(int32)+sizeof(int16));

                // receive buffer size
                buffer->putInt(getReceiveBufferSize());

                // socket receive buffer size
                buffer->putInt(getSocketReceiveBufferSize());

                // connection priority
                buffer->putShort(getPriority());

                // send immediately
                control->flush(true);

                _verifyOrEcho = false;
            }
            else {
                control->startMessage(CMD_ECHO, 0);
                // send immediately
                control->flush(true);
            }

        }

    }
}
