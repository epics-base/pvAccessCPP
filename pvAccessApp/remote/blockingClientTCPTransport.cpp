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

using std::set;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { errlogSevPrintf(errlogMajor, "Unhandled exception caught from code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { errlogSevPrintf(errlogMajor, "Unhandled exception caught from code at %s:%d.", __FILE__, __LINE__); }
                
                        BlockingClientTCPTransport::BlockingClientTCPTransport(
                Context* context, SOCKET channel,
                ResponseHandler* responseHandler, int receiveBufferSize,
                TransportClient* client, short remoteTransportRevision,
                float beaconInterval, int16 priority) :
            BlockingTCPTransport(context, channel, responseHandler,
                    receiveBufferSize, priority), _introspectionRegistry(
                    new IntrospectionRegistry(false)), _connectionTimeout(beaconInterval
                    *1000), _unresponsiveTransport(false), _timerNode(
                    new TimerNode(*this)), _verifyOrEcho(true) {
//            _autoDelete = false;

            // initialize owners list, send queue
            acquire(client);

            // use immediate for clients
            setSendQueueFlushStrategy(IMMEDIATE);

            // setup connection timeout timer (watchdog)
            epicsTimeGetCurrent(&_aliveTimestamp);

            context->getTimer()->schedulePeriodic(*_timerNode, beaconInterval,
                    beaconInterval);

            start();

        }

        BlockingClientTCPTransport::~BlockingClientTCPTransport() {
            delete _introspectionRegistry;
            delete _timerNode;
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
            else if(diff>_connectionTimeout) {
                // send echo
                enqueueSendRequest(this);
            }
        }

        void BlockingClientTCPTransport::unresponsiveTransport() {
            Lock lock(_ownersMutex);
            if(!_unresponsiveTransport) {
                _unresponsiveTransport = true;

                set<TransportClient*>::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient* client = *it;
                    client->acquire();
                    EXCEPTION_GUARD(client->transportUnresponsive());
                    client->release();
                }
            }
        }

        bool BlockingClientTCPTransport::acquire(TransportClient* client) {
            Lock lock(_mutex);
            if(_closed) return false;
            
            char ipAddrStr[48];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
            errlogSevPrintf(errlogInfo, "Acquiring transport to %s.", ipAddrStr);

            Lock lock2(_ownersMutex);
// TODO double check?            if(_closed) return false;
            _owners.insert(client);

            return true;
        }

        void BlockingClientTCPTransport::internalClose(bool forced) {
            BlockingTCPTransport::internalClose(forced);

            _timerNode->cancel();

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

                set<TransportClient*>::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient* client = *it;
                    client->acquire();
                    EXCEPTION_GUARD(client->transportClosed());
                    client->release();
                }
                
            }

            _owners.clear();
        }

        void BlockingClientTCPTransport::release(TransportClient* client) {
            Lock lock(_mutex);
            if(_closed) return;
            
            char ipAddrStr[48];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));

            errlogSevPrintf(errlogInfo, "Releasing transport to %s.", ipAddrStr);

            Lock lock2(_ownersMutex);
            _owners.erase(client);

            // not used anymore
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

                set<TransportClient*>::iterator it = _owners.begin();
                for(; it!=_owners.end(); it++) {
                    TransportClient* client = *it;
                    client->acquire();
                    EXCEPTION_GUARD(client->transportResponsive(this));
                    client->release();
                }
            }
        }

        void BlockingClientTCPTransport::changedTransport() {
            _introspectionRegistry->reset();

            Lock lock(_ownersMutex);
            set<TransportClient*>::iterator it = _owners.begin();
            for(; it!=_owners.end(); it++) {
                TransportClient* client = *it;
                client->acquire();
                EXCEPTION_GUARD(client->transportChanged());
                client->release();
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
