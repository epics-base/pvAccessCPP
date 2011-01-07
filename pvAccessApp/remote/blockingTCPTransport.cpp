/*
 * blockingTCPTransport.cpp
 *
 *  Created on: Dec 29, 2010
 *      Author: Miha Vitorovic
 */

#include "blockingTCP.h"
#include "inetAddressUtil.h"
#include "growingCircularBuffer.h"
#include "caConstants.h"

/* pvData */
#include <lock.h>
#include <byteBuffer.h>
#include <epicsException.h>
#include <noDefaultMethods.h>

/* EPICSv3 */
#include <osdSock.h>
#include <osiSock.h>
#include <epicsThread.h>
#include <errlog.h>

/* standard */
#include <sys/types.h>
#include <sys/socket.h>
#include <algorithm>
#include <sstream>

using namespace epics::pvData;

using std::max;
using std::min;
using std::ostringstream;

namespace epics {
    namespace pvAccess {

        class MonitorSender : public TransportSender, public NoDefaultMethods {
        public:
            MonitorSender(Mutex* monitorMutex, GrowingCircularBuffer<
                    TransportSender*>* monitorSendQueue) :
                _monitorMutex(monitorMutex),
                        _monitorSendQueue(monitorSendQueue) {
            }

            virtual ~MonitorSender() {
            }

            virtual void lock() {
            }

            virtual void unlock() {
            }

            virtual void
            send(ByteBuffer* buffer, TransportSendControl* control);

        private:
            Mutex* _monitorMutex;
            GrowingCircularBuffer<TransportSender*>* _monitorSendQueue;
        };

        BlockingTCPTransport::BlockingTCPTransport(Context* context,
                SOCKET channel, ResponseHandler* responseHandler,
                int receiveBufferSize, int16 priority) :
            _closed(false), _channel(channel), _socketAddress(new osiSockAddr),
                    _remoteTransportRevision(0),
                    _remoteTransportReceiveBufferSize(MAX_TCP_RECV),
                    _remoteTransportSocketReceiveBufferSize(MAX_TCP_RECV),
                    _priority(priority), _responseHandler(responseHandler),
                    _totalBytesReceived(0), _totalBytesSent(0),
                    _markerToSend(0), _verified(false), _remoteBufferFreeSpace(
                            LONG_LONG_MAX), _markerPeriodBytes(MARKER_PERIOD),
                    _nextMarkerPosition(_markerPeriodBytes),
                    _sendPending(false), _lastMessageStartPosition(0), _mutex(
                            new Mutex()), _sendQueueMutex(new Mutex()),
                    _verifiedMutex(new Mutex()), _monitorMutex(new Mutex()),
                    _stage(READ_FROM_SOCKET), _lastSegmentedMessageType(0),
                    _lastSegmentedMessageCommand(0), _storedPayloadSize(0),
                    _storedPosition(0), _storedLimit(0), _magicAndVersion(0),
                    _packetType(0), _command(0), _payloadSize(0),
                    _flushRequested(false), _sendBufferSentPosition(0),
                    _flushStrategy(DELAYED), _sendQueue(
                            new GrowingCircularBuffer<TransportSender*> (100)),
                    _rcvThreadId(NULL), _sendThreadId(NULL), _monitorSendQueue(
                            new GrowingCircularBuffer<TransportSender*> (100)),
                    _monitorSender(new MonitorSender(_monitorMutex,
                            _monitorSendQueue)), _context(context) {

            _socketBuffer = new ByteBuffer(max(MAX_TCP_RECV
                    +MAX_ENSURE_DATA_BUFFER_SIZE, receiveBufferSize));
            _socketBuffer->setPosition(_socketBuffer->getLimit());
            _startPosition = _socketBuffer->getPosition();

            // allocate buffer
            _sendBuffer = new ByteBuffer(_socketBuffer->getSize());
            _maxPayloadSize = _sendBuffer->getSize()-2*CA_MESSAGE_HEADER_SIZE; // one for header, one for flow control

            // get send buffer size

            socklen_t intLen = sizeof(int);

            int retval = getsockopt(_channel, SOL_SOCKET, SO_SNDBUF,
                    &_socketSendBufferSize, &intLen);
            if(retval<0) {
                _socketSendBufferSize = MAX_TCP_RECV;
                errlogSevPrintf(errlogMinor,
                        "Unable to retrieve socket send buffer size: %s",
                        strerror(errno));
            }

            socklen_t saSize = sizeof(sockaddr);
            retval = getpeername(_channel, &(_socketAddress->sa), &saSize);
            if(retval<0) {
                errlogSevPrintf(errlogMajor,
                        "Error fetching socket remote address: %s", strerror(
                                errno));
            }

            // prepare buffer
            clearAndReleaseBuffer();

            // add to registry
            _context->getTransportRegistry()->put(this);
        }

        BlockingTCPTransport::~BlockingTCPTransport() {
            close(true);

            delete _socketAddress;
            delete _sendQueue;
            delete _socketBuffer;
            delete _sendBuffer;

            delete _mutex;
            delete _sendQueueMutex;
            delete _verifiedMutex;
            delete _monitorMutex;
        }

        void BlockingTCPTransport::start() {
            String threadName = "TCP-receive "+inetAddressToString(
                    _socketAddress);

            errlogSevPrintf(errlogInfo, "Starting thread: %s",
                    threadName.c_str());

            _rcvThreadId = epicsThreadCreate(threadName.c_str(),
                    epicsThreadPriorityMedium, epicsThreadGetStackSize(
                            epicsThreadStackMedium),
                    BlockingTCPTransport::rcvThreadRunner, this);

            threadName = "TCP-send "+inetAddressToString(_socketAddress);

            errlogSevPrintf(errlogInfo, "Starting thread: %s",
                    threadName.c_str());

            _sendThreadId = epicsThreadCreate(threadName.c_str(),
                    epicsThreadPriorityMedium, epicsThreadGetStackSize(
                            epicsThreadStackMedium),
                    BlockingTCPTransport::sendThreadRunner, this);

        }

        void BlockingTCPTransport::clearAndReleaseBuffer() {
            // NOTE: take care that nextMarkerPosition is set right
            // fix position to be correct when buffer is cleared
            // do not include pre-buffered flow control message; not 100% correct, but OK
            _nextMarkerPosition -= _sendBuffer->getPosition()
                    -CA_MESSAGE_HEADER_SIZE;

            _sendQueueMutex->lock();
            _flushRequested = false;
            _sendQueueMutex->unlock();

            _sendBuffer->clear();

            _sendPending = false;

            // prepare ACK marker
            _sendBuffer->putShort(CA_MAGIC_AND_VERSION);
            _sendBuffer->putByte(1); // control data
            _sendBuffer->putByte(1); // marker ACK
            _sendBuffer->putInt(0);
        }

        void BlockingTCPTransport::close(bool force) {
            Lock lock(_mutex);

            // already closed check
            if(_closed) return;

            _closed = true;

            // remove from registry
            _context->getTransportRegistry()->remove(this);

            // clean resources
            internalClose(force);

            // threads cannot "wait" Epics, no need to notify
            // TODO check alternatives to "wait"
            // notify send queue
            //synchronized (sendQueue) {
            //    sendQueue.notifyAll();
            //}
        }

        void BlockingTCPTransport::internalClose(bool force) {
            // close the socket
            epicsSocketDestroy(_channel);
        }

        int BlockingTCPTransport::getSocketReceiveBufferSize() const {
            // Get value of the SO_RCVBUF option for this DatagramSocket,
            // that is the buffer size used by the platform for input on
            // this DatagramSocket.

            int sockBufSize;
            socklen_t intLen = sizeof(int);

            int retval = getsockopt(_channel, SOL_SOCKET, SO_RCVBUF,
                    &sockBufSize, &intLen);
            if(retval<0) errlogSevPrintf(errlogMajor,
                    "Socket getsockopt SO_RCVBUF error: %s", strerror(errno));

            return sockBufSize;
        }

        bool BlockingTCPTransport::waitUntilVerified(double timeout) {
            double internalTimeout = timeout;
            bool internalVerified = false;

            _verifiedMutex->lock();
            internalVerified = _verified;
            _verifiedMutex->unlock();

            while(!internalVerified&&internalTimeout>0) {
                epicsThreadSleep(min(0.1, internalTimeout));
                internalTimeout -= 0.1;

                _verifiedMutex->lock();
                internalVerified = _verified;
                _verifiedMutex->unlock();
            }
            return internalVerified;
        }

        void BlockingTCPTransport::flush(bool lastMessageCompleted) {

            // automatic end
            endMessage(!lastMessageCompleted);

            bool moreToSend = true;
            // TODO closed check !!!
            while(moreToSend) {
                moreToSend = !flush();

                // all sent, exit
                if(!moreToSend) break;

                // TODO solve this sleep in a better way
                epicsThreadSleep(0.01);
            }

            _lastMessageStartPosition = _sendBuffer->getPosition();
            // start with last header
            if(!lastMessageCompleted&&_lastSegmentedMessageType!=0) startMessage(
                    _lastSegmentedMessageCommand, 0);
        }

        void BlockingTCPTransport::startMessage(int8 command,
                int ensureCapacity) {
            _lastMessageStartPosition = -1;
            ensureBuffer(CA_MESSAGE_HEADER_SIZE+ensureCapacity);
            _lastMessageStartPosition = _sendBuffer->getPosition();
            _sendBuffer->putShort(CA_MAGIC_AND_VERSION);
            _sendBuffer->putByte(_lastSegmentedMessageType); // data
            _sendBuffer->putByte(command); // command
            _sendBuffer->putInt(0); // temporary zero payload

        }

        void BlockingTCPTransport::endMessage() {
            endMessage(false);
        }

        void BlockingTCPTransport::ensureBuffer(int size) {
            if(_sendBuffer->getRemaining()>=size) return;

            // too large for buffer...
            if(_maxPayloadSize<size) {
                ostringstream temp;
                temp<<"requested for buffer size "<<size<<", but only ";
                temp<<_maxPayloadSize<<" available.";
                THROW_BASE_EXCEPTION(temp.str().c_str());
            }

            while(_sendBuffer->getRemaining()<size&&!_closed)
                flush(false);

            if(!_closed) THROW_BASE_EXCEPTION("transport closed");
        }

        void BlockingTCPTransport::endMessage(bool hasMoreSegments) {
            if(_lastMessageStartPosition>=0) {

                // set message size
                _sendBuffer->putInt(_lastMessageStartPosition+sizeof(int16)+2,
                        _sendBuffer->getPosition()-_lastMessageStartPosition
                                -CA_MESSAGE_HEADER_SIZE);

                int flagsPosition = _lastMessageStartPosition+sizeof(int16);
                // set segmented bit
                if(hasMoreSegments) {
                    // first segment
                    if(_lastSegmentedMessageType==0) {
                        int8 type = _sendBuffer->getByte(flagsPosition);

                        // set first segment bit
                        _sendBuffer->putByte(flagsPosition, (int8)(type|0x10));

                        // first + last segment bit == in-between segment
                        _lastSegmentedMessageType = (int8)(type|0x30);
                        _lastSegmentedMessageCommand = _sendBuffer->getByte(
                                flagsPosition+1);
                    }
                }
                else {
                    // last segment
                    if(_lastSegmentedMessageType!=0) {
                        // set last segment bit (by clearing first segment bit)
                        _sendBuffer->putByte(flagsPosition,
                                (int8)(_lastSegmentedMessageType&0xEF));
                        _lastSegmentedMessageType = 0;
                    }
                }

                // manage markers
                int position = _sendBuffer->getPosition();
                int bytesLeft = _sendBuffer->getRemaining();

                if(position>=_nextMarkerPosition&&bytesLeft
                        >=CA_MESSAGE_HEADER_SIZE) {
                    _sendBuffer->putShort(CA_MAGIC_AND_VERSION);
                    _sendBuffer->putByte(1); // control data
                    _sendBuffer->putByte(0); // marker
                    _sendBuffer->putInt((int)(_totalBytesSent+position
                            +CA_MESSAGE_HEADER_SIZE));
                    _nextMarkerPosition = position+_markerPeriodBytes;
                }
            }
        }

        void BlockingTCPTransport::ensureData(int size) {
            // enough of data?
            if(_socketBuffer->getRemaining()>=size) return;

            // too large for buffer...
            if(_maxPayloadSize<size) {
                ostringstream temp;
                temp<<"requested for buffer size "<<size<<", but only ";
                temp<<_maxPayloadSize<<" available.";
                THROW_BASE_EXCEPTION(temp.str().c_str());
            }

            // subtract what was already processed
            _storedPayloadSize -= _socketBuffer->getPosition()-_storedPosition;

            // no more data and we have some payload left => read buffer
            if(_storedPayloadSize>=size) {
                //errlogSevPrintf(errlogInfo,
                //        "storedPayloadSize >= size, remaining: %d",
                //        _socketBuffer->getRemaining());

                // just read up remaining payload
                // since there is no data on the buffer, read to the beginning of it, at least size bytes
                processReadCached(true, PROCESS_PAYLOAD, size, false);
                _storedPosition = _socketBuffer->getPosition();
                _storedLimit = _socketBuffer->getLimit();
                _socketBuffer->setLimit(min(_storedPosition+_storedPayloadSize,
                        _storedLimit));
            }
            else {
                // copy remaining bytes, if any
                int remainingBytes = _socketBuffer->getRemaining();
                for(int i = 0; i<remainingBytes; i++)
                    _socketBuffer->putByte(i, _socketBuffer->getByte());

                // read what is left
                _socketBuffer->setLimit(_storedLimit);

                _stage = PROCESS_HEADER;
                processReadCached(true, NONE, size, false);

                // copy before position
                for(int i = remainingBytes-1, j = _socketBuffer->getPosition()
                        -1; i>=0; i--, j--)
                    _socketBuffer->putByte(j, _socketBuffer->getByte(i));
                _startPosition = _socketBuffer->getPosition()-remainingBytes;
                _socketBuffer->setPosition(_startPosition);

                _storedPosition = _startPosition; //socketBuffer.position();
                _storedLimit = _socketBuffer->getLimit();
                _socketBuffer->setLimit(min(_storedPosition+_storedPayloadSize,
                        _storedLimit));

                // add if missing...
                if(!_closed&&_socketBuffer->getRemaining()<size) ensureData(
                        size);
            }

            if(_closed) THROW_BASE_EXCEPTION("transport closed");
        }

        void BlockingTCPTransport::processReadCached(bool nestedCall,
                ReceiveStage inStage, int requiredBytes, bool addToBuffer) {
            // TODO remove debug
            errlogSevPrintf(errlogInfo,
                    "processReadCached(%d, %d, %d, %d), _stage: %d",
                    nestedCall, inStage, requiredBytes, addToBuffer, _stage);

            try {
                while(!_closed) {
                    if(_stage==READ_FROM_SOCKET||inStage!=NONE) {
                        int currentStartPosition;
                        if(addToBuffer) {
                            currentStartPosition = _socketBuffer->getPosition();
                            _socketBuffer->setPosition(
                                    _socketBuffer->getLimit());
                            _socketBuffer->setLimit(_socketBuffer->getSize());
                        }
                        else {
                            // add to bytes read
                            _totalBytesReceived
                                    += (_socketBuffer->getPosition()
                                            -_startPosition);

                            // copy remaining bytes, if any
                            int remainingBytes = _socketBuffer->getRemaining();
                            int endPosition = MAX_ENSURE_DATA_BUFFER_SIZE
                                    +remainingBytes;
                            for(int i = MAX_ENSURE_DATA_BUFFER_SIZE; i
                                    <endPosition; i++)
                                _socketBuffer->putByte(i,
                                        _socketBuffer->getByte());

                            currentStartPosition = _startPosition
                                    = MAX_ENSURE_DATA_BUFFER_SIZE;
                            _socketBuffer->setPosition(
                                    MAX_ENSURE_DATA_BUFFER_SIZE+remainingBytes);
                            _socketBuffer->setLimit(_socketBuffer->getSize());
                        }

                        // read at least requiredBytes bytes

                        int requiredPosition = (currentStartPosition
                                +requiredBytes);

                        // TODO remove debug
                        errlogSevPrintf(errlogInfo,
                                "requredPos:%d, buffer->pos:%d",
                                requiredPosition, _socketBuffer->getPosition());
                        while(_socketBuffer->getPosition()<requiredPosition) {
                            // read
                            char readBuffer[MAX_TCP_RECV];
                            size_t maxToRead = min(MAX_TCP_RECV,
                                    _socketBuffer->getRemaining());
                            ssize_t bytesRead = recv(_channel, readBuffer,
                                    maxToRead, 0);
                            _socketBuffer->put(readBuffer, 0, bytesRead);

                            // TODO remove debug
                            if(bytesRead>0) errlogSevPrintf(
                                    errlogInfo,
                                    "***!!! got %d bytes of %d (reqPos=%d)!!!***",
                                    bytesRead, requiredBytes, requiredPosition);

                            if(bytesRead<0) {
                                // error (disconnect, end-of-stream) detected
                                close(true);

                                if(nestedCall) THROW_BASE_EXCEPTION(
                                        "bytesRead < 0");

                                return;
                            }
                        }
                        _socketBuffer->setLimit(_socketBuffer->getPosition());
                        _socketBuffer->setPosition(currentStartPosition);

                        // notify liveness
                        aliveNotification();

                        // exit
                        if(inStage!=NONE) return;

                        _stage = PROCESS_HEADER;
                    }

                    if(_stage==PROCESS_HEADER) {
                        // ensure CAConstants.CA_MESSAGE_HEADER_SIZE bytes of data
                        if(_socketBuffer->getRemaining()<CA_MESSAGE_HEADER_SIZE) processReadCached(
                                true, PROCESS_HEADER, CA_MESSAGE_HEADER_SIZE,
                                false);

                        // first byte is CA_MAGIC
                        // second byte version - major/minor nibble
                        // check magic and version at once
                        _magicAndVersion = _socketBuffer->getShort();
                        if((short)(_magicAndVersion&0xFFF0)
                                !=CA_MAGIC_AND_MAJOR_VERSION) {
                            // error... disconnect
                            errlogSevPrintf(
                                    errlogMinor,
                                    "Invalid header received from client %s, disconnecting...",
                                    inetAddressToString(_socketAddress).c_str());
                            close(true);
                            return;
                        }

                        // data vs. control packet
                        _packetType = _socketBuffer->getByte();

                        // command
                        _command = _socketBuffer->getByte();

                        // read payload size
                        _payloadSize = _socketBuffer->getInt();

                        // data
                        int8 type = (int8)(_packetType&0x0F);
                        if(type==0) {
                            _stage = PROCESS_PAYLOAD;
                        }
                        else if(type==1) {
                            if(_command==0) {
                                if(_markerToSend==0) _markerToSend
                                        = _payloadSize; // TODO send back response
                            }
                            else //if (command == 1)
                            {
                                int difference = (int)_totalBytesSent
                                        -_payloadSize+CA_MESSAGE_HEADER_SIZE;
                                // overrun check
                                if(difference<0) difference += INT_MAX;
                                _remoteBufferFreeSpace
                                        = _remoteTransportReceiveBufferSize
                                                +_remoteTransportSocketReceiveBufferSize
                                                -difference;
                                // TODO if this is calculated wrong, this can be critical !!!
                            }

                            // no payload
                            //stage = ReceiveStage.PROCESS_HEADER;
                            continue;
                        }
                        else {
                            errlogSevPrintf(
                                    errlogMajor,
                                    "Unknown packet type %d, received from client %s, disconnecting...",
                                    type,
                                    inetAddressToString(_socketAddress).c_str());
                            close(true);
                            return;
                        }
                    }

                    if(_stage==PROCESS_PAYLOAD) {
                        // read header
                        int8 version = (int8)(_magicAndVersion&0xFF);
                        // last segment bit set (means in-between segment or last segment)
                        bool notFirstSegment = (_packetType&0x20)!=0;

                        _storedPayloadSize = _payloadSize;

                        // if segmented, exit reading code
                        if(nestedCall&&notFirstSegment) return;

                        // NOTE: nested data (w/ payload) messages between segmented messages are not supported
                        _storedPosition = _socketBuffer->getPosition();
                        _storedLimit = _socketBuffer->getLimit();
                        _socketBuffer->setLimit(min(_storedPosition
                                +_storedPayloadSize, _storedLimit));
                        try {
                            // handle response
                            _responseHandler->handleResponse(_socketAddress,
                                    this, version, _command, _payloadSize,
                                    _socketBuffer);
                        } catch(...) {
                            //noop
                        }

                        /*
                         * Java finally start
                         */
                        _socketBuffer->setLimit(_storedLimit);
                        int newPosition = _storedPosition+_storedPayloadSize;
                        if(newPosition>_storedLimit) {
                            newPosition -= _storedLimit;
                            _socketBuffer->setPosition(_storedLimit);
                            processReadCached(true, PROCESS_PAYLOAD,
                                    newPosition, false);
                            newPosition += _startPosition;
                        }
                        _socketBuffer->setPosition(newPosition);
                        // TODO discard all possible segments?!!!
                        /*
                         * Java finally end
                         */

                        _stage = PROCESS_HEADER;

                        continue;
                    }

                }
            } catch(...) {
                // close connection
                close(true);

                if(nestedCall) throw;
            }
        }

        bool BlockingTCPTransport::flush() {
            // request issues, has not sent anything yet (per partes)
            if(!_sendPending) {
                _sendPending = true;

                // start sending from the start
                _sendBufferSentPosition = 0;

                // if not set skip marker otherwise set it
                int markerValue = _markerToSend;
                _markerToSend = 0;
                if(markerValue==0)
                    _sendBufferSentPosition = CA_MESSAGE_HEADER_SIZE;
                else
                    _sendBuffer->putInt(4, markerValue);
            }

            bool success = false;
            try {
                // remember current position
                int currentPos = _sendBuffer->getPosition();

                // set to send position
                _sendBuffer->setPosition(_sendBufferSentPosition);
                _sendBuffer->setLimit(currentPos);

                success = send(_sendBuffer);

                // all sent?
                if(success)
                    clearAndReleaseBuffer();
                else {
                    // remember position
                    _sendBufferSentPosition = _sendBuffer->getPosition();

                    // .. reset to previous state
                    _sendBuffer->setPosition(currentPos);
                    _sendBuffer->setLimit(_sendBuffer->getSize());
                }
            } catch(BaseException* e) {
                String trace;
                e->toString(trace);
                errlogSevPrintf(errlogMajor, trace.c_str());
                // error, release lock
                clearAndReleaseBuffer();
            } catch(...) {
                clearAndReleaseBuffer();
            }
            return success;
        }

        bool BlockingTCPTransport::send(ByteBuffer* buffer) {
            try {
                // TODO simply use value from marker???!!!
                // On Windows, limiting the buffer size is important to prevent
                // poor throughput performances when transferring large amount of
                // data. See Microsoft KB article KB823764.
                // We do it also for other systems just to be safe.
                int maxBytesToSend = min(_socketSendBufferSize,
                        _remoteTransportSocketReceiveBufferSize)/2;

                int limit = buffer->getLimit();
                int bytesToSend = limit-buffer->getPosition();

                //errlogSevPrintf(errlogInfo,"Total bytes to send: %d", bytesToSend);

                // limit sending
                if(bytesToSend>maxBytesToSend) {
                    bytesToSend = maxBytesToSend;
                    buffer->setLimit(buffer->getPosition()+bytesToSend);
                }

                //errlogSevPrintf(errlogInfo,
                //        "Sending %d of total %d bytes in the packet to %s.",
                //        bytesToSend, limit,
                //        inetAddressToString(_socketAddress).c_str());

                while(buffer->getRemaining()>0) {
                    ssize_t bytesSent = ::send(_channel,
                            &buffer->getArray()[buffer->getPosition()],
                            buffer->getRemaining(), 0);

                    if(bytesSent<0) {
                        // connection lost
                        ostringstream temp;
                        temp<<"error in sending TCP data: "<<strerror(errno);
                        errlogSevPrintf(errlogMajor, temp.str().c_str());
                        THROW_BASE_EXCEPTION(temp.str().c_str());
                    }
                    else if(bytesSent==0) {
                        //errlogSevPrintf(errlogInfo,
                        //        "Buffer full, position %d of total %d bytes.",
                        //        buffer->getPosition(), limit);

                        /* buffers full, reset the limit and indicate that there
                         * is more data to be sent
                         */
                        if(bytesSent==maxBytesToSend) buffer->setLimit(limit);

                        //errlogSevPrintf(errlogInfo,
                        //        "Send buffer full for %s, waiting...",
                        //        inetAddressToString(_socketAddress));
                        return false;
                    }

                    buffer->setPosition(buffer->getPosition()+bytesSent);

                    _totalBytesSent += bytesSent;

                    // readjust limit
                    if(bytesToSend==maxBytesToSend) {
                        bytesToSend = limit-buffer->getPosition();
                        if(bytesToSend>maxBytesToSend) bytesToSend
                                = maxBytesToSend;
                        buffer->setLimit(buffer->getPosition()+bytesToSend);
                    }

                    //errlogSevPrintf(errlogInfo,
                    //        "Sent, position %d of total %d bytes.",
                    //        buffer->getPosition(), limit);
                } // while
            } catch(...) {
                close(true);
                throw;
            }

            // all sent
            return true;
        }

        TransportSender* BlockingTCPTransport::extractFromSendQueue() {
            TransportSender* retval;

            _sendQueueMutex->lock();
            try {
                if(_sendQueue->size()>0)
                    retval = _sendQueue->extract();
                else
                    retval = NULL;
            } catch(...) {
                // not expecting the exception here, but just to be safe
                retval = NULL;
            }

            _sendQueueMutex->unlock();

            return retval;
        }

        void BlockingTCPTransport::processSendQueue() {
            while(!_closed) {
                TransportSender* sender;

                sender = extractFromSendQueue();
                // wait for new message
                while(sender==NULL&&!_flushRequested&&!_closed) {
                    if(_flushStrategy==DELAYED) {
                        if(delay>0) epicsThreadSleep(delay);
                        if(_sendQueue->size()==0) {
                            // if (hasMonitors || sendBuffer.position() > CAConstants.CA_MESSAGE_HEADER_SIZE)
                            if(_sendBuffer->getPosition()
                                    >CA_MESSAGE_HEADER_SIZE)
                                _flushRequested = true;
                            else
                                epicsThreadSleep(0);
                        }
                    }
                    else
                        epicsThreadSleep(0);
                    sender = extractFromSendQueue();
                }

                // always do flush from this thread
                if(_flushRequested) {
                    /*
                     if (hasMonitors)
                     {
                     monitorSender.send(sendBuffer, this);
                     }
                     */

                    flush();
                }

                if(sender!=NULL) {
                    sender->lock();
                    try {
                        _lastMessageStartPosition = _sendBuffer->getPosition();
                        sender->send(_sendBuffer, this);

                        if(_flushStrategy==IMMEDIATE)
                            flush(true);
                        else
                            endMessage(false);// automatic end (to set payload)

                    } catch(BaseException* e) {
                        String trace;
                        e->toString(trace);
                        errlogSevPrintf(errlogMajor, trace.c_str());
                        _sendBuffer->setPosition(_lastMessageStartPosition);
                    } catch(...) {
                        _sendBuffer->setPosition(_lastMessageStartPosition);
                    }
                    sender->unlock();
                } // if(sender!=NULL)
            } // while(!_closed)
        }

        void BlockingTCPTransport::requestFlush() {
            // needless lock, manipulating a single byte
            //Lock lock(_sendQueueMutex);
            if(_flushRequested) return;
            _flushRequested = true;
        }

        void BlockingTCPTransport::freeSendBuffers() {
            // TODO ?
        }

        void BlockingTCPTransport::freeConnectionResorces() {
            freeSendBuffers();

            errlogSevPrintf(errlogInfo, "Connection to %s closed.",
                    inetAddressToString(_socketAddress).c_str());

            epicsSocketDestroy(_channel);
        }

        void BlockingTCPTransport::rcvThreadRunner(void* param) {
            ((BlockingTCPTransport*)param)->processReadCached(false, NONE,
                    CA_MESSAGE_HEADER_SIZE, false);
        }

        void BlockingTCPTransport::sendThreadRunner(void* param) {
            BlockingTCPTransport* obj = (BlockingTCPTransport*)param;

            obj->processSendQueue();

            obj->freeConnectionResorces();
        }

        void BlockingTCPTransport::enqueueSendRequest(TransportSender* sender) {
            Lock lock(_sendQueueMutex);
            _sendQueue->insert(sender);
        }

        void BlockingTCPTransport::enqueueMonitorSendRequest(
                TransportSender* sender) {
            Lock lock(_monitorMutex);
            _monitorSendQueue->insert(sender);
            if(_monitorSendQueue->size()==1) enqueueSendRequest(_monitorSender);
        }

        void MonitorSender::send(ByteBuffer* buffer,
                TransportSendControl* control) {
            control->startMessage(19, 0);

            while(true) {
                TransportSender* sender;
                _monitorMutex->lock();
                if(_monitorSendQueue->size()>0)
                    try {
                        sender = _monitorSendQueue->extract();
                    } catch(...) {
                        sender = NULL;
                    }
                else
                    sender = NULL;
                _monitorMutex->unlock();

                if(sender==NULL) {
                    control->ensureBuffer(sizeof(int32));
                    buffer->putInt(CAJ_INVALID_IOID);
                    break;
                }
                sender->send(buffer, control);
            }
        }

    }
}
