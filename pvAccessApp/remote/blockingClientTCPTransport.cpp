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

        BlockingClientTCPTransport::BlockingClientTCPTransport(
                Context* context, SOCKET channel,
                ResponseHandler* responseHandler, int receiveBufferSize,
                TransportClient* client, short remoteTransportRevision,
                float beaconInterval, int16 priority) :
            BlockingTCPTransport(context, channel, responseHandler,
                    receiveBufferSize, priority), _introspectionRegistry(
                    new IntrospectionRegistry(false)), _owners(new set<
                    TransportClient*> ()), _connectionTimeout(beaconInterval
                    *1000), _unresponsiveTransport(false), _timerNode(
                    new TimerNode(this)), _mutex(new Mutex()), _ownersMutex(
                    new Mutex()), _verifyOrEcho(true) {

            // initialize owners list, send queue
            acquire(client);

            // use immediate for clients
            setSendQueueFlushStrategy(IMMEDIATE);

            // setup connection timeout timer (watchdog)
            epicsTimeGetCurrent(const_cast<epicsTimeStamp*> (&_aliveTimestamp));

            context->getTimer()->schedulePeriodic(_timerNode, beaconInterval,
                    beaconInterval);

            start();

        }

        BlockingClientTCPTransport::~BlockingClientTCPTransport() {
            delete _introspectionRegistry;
            delete _owners;
            delete _timerNode;
            delete _mutex;
            delete _ownersMutex;
        }

        void BlockingClientTCPTransport::callback() {
            epicsTimeStamp currentTime;
            epicsTimeGetCurrent(&currentTime);

            double diff = epicsTimeDiffInSeconds(&currentTime,
                    const_cast<epicsTimeStamp*> (&_aliveTimestamp));
            if(diff>2*_connectionTimeout) {
                unresponsiveTransport();
            }
            else if(diff>_connectionTimeout) {
                // send echo
                enqueueSendRequest(this);
            }
        }

        void BlockingClientTCPTransport::unresponsiveTransport() {
            if(!_unresponsiveTransport) {
                _unresponsiveTransport = true;

                Lock lock(_ownersMutex);
                set<TransportClient*>::iterator it = _owners->begin();
                for(; it!=_owners->end(); it++)
                    (*it)->transportUnresponsive();
            }
        }

        bool BlockingClientTCPTransport::acquire(TransportClient* client) {
            Lock lock(_mutex);

            if(_closed) return false;

            char ipAddrStr[48];
            ipAddrToA(&_socketAddress->ia, ipAddrStr, sizeof(ipAddrStr));
            errlogSevPrintf(errlogInfo, "Acquiring transport to %s.", ipAddrStr);

            _ownersMutex->lock();
            if(_closed) return false;

            _owners->insert(client);
            _ownersMutex->unlock();

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
            int refs = _owners->size();
            if(refs>0) {
                char ipAddrStr[48];
                ipAddrToA(&_socketAddress->ia, ipAddrStr, sizeof(ipAddrStr));
                errlogSevPrintf(
                        errlogInfo,
                        "Transport to %s still has %d client(s) active and closing...",
                        ipAddrStr, refs);

                set<TransportClient*>::iterator it = _owners->begin();
                for(; it!=_owners->end(); it++)
                    (*it)->transportClosed();
            }

            _owners->clear();
        }

        void BlockingClientTCPTransport::release(TransportClient* client) {
            if(_closed) return;

            char ipAddrStr[48];
            ipAddrToA(&_socketAddress->ia, ipAddrStr, sizeof(ipAddrStr));

            errlogSevPrintf(errlogInfo, "Releasing transport to %s.", ipAddrStr);

            Lock lock(_ownersMutex);
            _owners->erase(client);

            // not used anymore
            // TODO consider delayed destruction (can improve performance!!!)
            if(_owners->size()==0) close(false);
        }

        void BlockingClientTCPTransport::aliveNotification() {
            epicsTimeGetCurrent(const_cast<epicsTimeStamp*> (&_aliveTimestamp));
            if(_unresponsiveTransport) responsiveTransport();
        }

        void BlockingClientTCPTransport::responsiveTransport() {
            if(_unresponsiveTransport) {
                _unresponsiveTransport = false;
                Lock lock(_ownersMutex);

                set<TransportClient*>::iterator it = _owners->begin();
                for(; it!=_owners->end(); it++)
                    (*it)->transportResponsive(this);
            }
        }

        void BlockingClientTCPTransport::changedTransport() {
            _introspectionRegistry->reset();
            Lock lock(_ownersMutex);

            set<TransportClient*>::iterator it = _owners->begin();
            for(; it!=_owners->end(); it++)
                (*it)->transportChanged();
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
