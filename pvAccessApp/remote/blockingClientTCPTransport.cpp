/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/blockingTCP.h>
#include <pv/introspectionRegistry.h>
#include <pv/logger.h>

#include <pv/lock.h>

#include <set>
#include <epicsTime.h>
#include <sstream>

using namespace std;
using namespace epics::pvData;

namespace epics {
    namespace pvAccess {

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from code at %s:%d.", __FILE__, __LINE__); }
                
        BlockingClientTCPTransport::BlockingClientTCPTransport(
                Context::shared_pointer const & context, SOCKET channel,
                auto_ptr<ResponseHandler>& responseHandler, int receiveBufferSize,
                TransportClient::shared_pointer client, int8 /*remoteTransportRevision*/,
                float beaconInterval, int16 priority) :
            BlockingTCPTransport(context, channel, responseHandler, receiveBufferSize, priority),
                    _connectionTimeout(beaconInterval*1000),
                    _unresponsiveTransport(false),
                    _verifyOrEcho(true)
        {
//            _autoDelete = false;

            // initialize owners list, send queue
            acquire(client);

            // use immediate for clients
            setFlushStrategy(DELAYED);

            // setup connection timeout timer (watchdog)
            epicsTimeGetCurrent(&_aliveTimestamp);
        }

        void BlockingClientTCPTransport::start()
        {
            TimerCallbackPtr tcb = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
            _context->getTimer()->schedulePeriodic(tcb, _connectionTimeout, _connectionTimeout);
            BlockingTCPTransport::start();
        }
        
        BlockingClientTCPTransport::~BlockingClientTCPTransport() {
        }

        void BlockingClientTCPTransport::callback() {
            epicsTimeStamp currentTime;
            epicsTimeGetCurrent(&currentTime);

            _mutex.lock();
            // no exception expected here
            double diff = epicsTimeDiffInSeconds(&currentTime, &_aliveTimestamp);
            _mutex.unlock();
            
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
            Lock lock(_mutex);
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

        bool BlockingClientTCPTransport::acquire(TransportClient::shared_pointer const & client) {
            Lock lock(_mutex);
            if(_closed.get()) return false;
            
            char ipAddrStr[48];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
            LOG(logLevelDebug, "Acquiring transport to %s.", ipAddrStr);

            _owners[client->getID()] = TransportClient::weak_pointer(client);
            //_owners.insert(TransportClient::weak_pointer(client));

            return true;
        }

        // _mutex is held when this method is called
        void BlockingClientTCPTransport::internalClose(bool forced) {
            BlockingTCPTransport::internalClose(forced);

            TimerCallbackPtr tcb = std::tr1::dynamic_pointer_cast<TimerCallback>(shared_from_this());
            _context->getTimer()->cancel(tcb);
        }

        void BlockingClientTCPTransport::internalPostClose(bool forced) {
            BlockingTCPTransport::internalPostClose(forced);

            // _owners cannot change when transport is closed
            closedNotifyClients();
        }

        /**
         * Notifies clients about disconnect.
         */
        void BlockingClientTCPTransport::closedNotifyClients() {

            // check if still acquired
            int refs = _owners.size();
            if(refs>0) {
                char ipAddrStr[48];
                ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));
                LOG(
                        logLevelDebug,
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

        //void BlockingClientTCPTransport::release(TransportClient::shared_pointer const & client) {
        void BlockingClientTCPTransport::release(pvAccessID clientID) {
            Lock lock(_mutex);
            if(_closed.get()) return;
            
            char ipAddrStr[48];
            ipAddrToDottedIP(&_socketAddress.ia, ipAddrStr, sizeof(ipAddrStr));

            LOG(logLevelDebug, "Releasing transport to %s.", ipAddrStr);

            _owners.erase(clientID);
            //_owners.erase(TransportClient::weak_pointer(client));
            
            // not used anymore, close it
            // TODO consider delayed destruction (can improve performance!!!)
            if(_owners.size()==0) close();		// TODO close(false)
        }

        void BlockingClientTCPTransport::aliveNotification() {
            Lock guard(_mutex);
            epicsTimeGetCurrent(&_aliveTimestamp);
            if(_unresponsiveTransport) responsiveTransport();
        }

        void BlockingClientTCPTransport::responsiveTransport() {
            Lock lock(_mutex);
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
        	outgoingIR.reset();

            Lock lock(_mutex);
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
