/*
* testCodec.cpp
*/

#include <epicsExit.h>
#include <epicsUnitTest.h>
#include <testMain.h>
#include <pv/byteBuffer.h>

#include <pv/codec.h>
#include <pv/current_function.h>

using namespace epics::pvData;
using namespace epics::pvAccess::detail;

namespace epics {

namespace pvAccess {

struct sender_break : public connection_closed_exception
{
    sender_break() : connection_closed_exception("break") {}
};

struct TransportSenderDisconnect: public TransportSender {
    void send(ByteBuffer *buffer, TransportSendControl *control)
    {
        control->flush(true);
        throw sender_break();
    }
};

struct TransportSenderSignal: public TransportSender {
    Event *evt;
    TransportSenderSignal(Event& evt) :evt(&evt) {}
    void send(ByteBuffer *buffer, TransportSendControl *control)
    {
        evt->signal();
    }
};


class PVAMessage {

public:

    PVAMessage(int8_t version,
               int8_t flags,
               int8_t command,
               int32_t payloadSize) {
        _version = version;
        _flags = flags;
        _command = command;
        _payloadSize = payloadSize;
    }

    int8_t _version;
    int8_t _flags;
    int8_t _command;
    int32_t _payloadSize;
    std::tr1::shared_ptr<epics::pvData::ByteBuffer> _payload;

    //memberwise copy constructor/assigment operator
    //provided by the compiler
};


class ReadPollOneCallback {
public:
    virtual ~ReadPollOneCallback() {}
    virtual void readPollOne() = 0;
};


class WritePollOneCallback {
public:
    virtual ~WritePollOneCallback() {}
    virtual void writePollOne() = 0 ;
};


class TestCodec: public AbstractCodec {

public:

    TestCodec(
        std::size_t receiveBufferSize,
        std::size_t sendBufferSize,
        bool blocking = false):
        AbstractCodec(
            false,
            sendBufferSize,
            receiveBufferSize,
            sendBufferSize/10,
            blocking),
        _closedCount(0),
        _invalidDataStreamCount(0),
        _scheduleSendCount(0),
        _sendCompletedCount(0),
        _sendBufferFullCount(0),
        _readPollOneCount(0),
        _writePollOneCount(0),
        _throwExceptionOnSend(false),
        _readPayload(false),
        _disconnected(false),
        _forcePayloadRead(-1),
        _readBuffer(new ByteBuffer(receiveBufferSize)),
        _writeBuffer(sendBufferSize),
        _dummyAddress()
    {
        dummyAddr.ia.sin_family = AF_INET;
        dummyAddr.ia.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        dummyAddr.ia.sin_port = htons(42);
    }


    void reset()
    {
        _closedCount = 0;
        _invalidDataStreamCount = 0;
        _scheduleSendCount = 0;
        _sendCompletedCount = 0;
        _sendBufferFullCount = 0;
        _readPollOneCount = 0;
        _writePollOneCount = 0;
        _readBuffer->clear();
        _writeBuffer.clear();
        _receivedAppMessages.clear();
        _receivedControlMessages.clear();
    }


    int read(ByteBuffer *buffer) {

        if (_disconnected)
            return -1;

        std::size_t startPos = _readBuffer->getPosition();
        //buffer.put(readBuffer);
        //while (buffer.hasRemaining() && readBuffer.hasRemaining())
        //	buffer.put(readBuffer.get());

        std::size_t bufferRemaining = buffer->getRemaining();
        std::size_t readBufferRemaining =
            _readBuffer->getRemaining();

        if (bufferRemaining >= readBufferRemaining) {

            while(_readBuffer->getRemaining() > 0) {
                buffer->putByte(_readBuffer->getByte());
            }

        }
        else
        {
            // TODO this could be optimized
            for (std::size_t i = 0; i < bufferRemaining; i++) {
                buffer->putByte(_readBuffer->getByte());
            }
        }
        return _readBuffer->getPosition() - startPos;
    }


    int write(ByteBuffer *buffer) {
        if (_disconnected)
            return -1;	// TODO: not by the JavaDoc API spec

        if (_throwExceptionOnSend)
            throw io_exception("text IO exception");

        size_t nmove = std::min(buffer->getRemaining(), _writeBuffer.getRemaining());

        for(size_t n=0; n<nmove; n++)
            _writeBuffer.putByte(buffer->getByte());
        return nmove;
    }


    void transferToReadBuffer()
    {
        flushSerializeBuffer();
        _writeBuffer.flip();

        _readBuffer->clear();

        while(_writeBuffer.getRemaining() > 0) {
            _readBuffer->putByte(_writeBuffer.getByte());
        }

        _readBuffer->flip();

        _writeBuffer.clear();
    }


    void addToReadBuffer()
    {
        flushSerializeBuffer();
        _writeBuffer.flip();

        while(_writeBuffer.getRemaining() > 0) {
            _readBuffer->putByte(_writeBuffer.getByte());
        }

        _readBuffer->flip();

        _writeBuffer.clear();
    }


    void processControlMessage() {
        _receivedControlMessages.push_back(
            PVAMessage(_version, _flags, _command, _payloadSize));
    }


    void processApplicationMessage()  {
        PVAMessage caMessage(_version, _flags,
                             _command, _payloadSize);

        if (_readPayload && _payloadSize > 0)
        {
            // no fragmentation supported by this implementation
            std::size_t toRead =
                _forcePayloadRead >= 0
                ? _forcePayloadRead : _payloadSize;

            caMessage._payload.reset(new ByteBuffer(toRead));
            while (toRead > 0)
            {
                std::size_t partitalRead =
                    std::min<std::size_t>(toRead, MAX_ENSURE_DATA_SIZE);
                ensureData(partitalRead);
                std::size_t pos = caMessage._payload->getPosition();


                while(_socketBuffer.getRemaining() > 0) {
                    caMessage._payload->putByte(_socketBuffer.getByte());
                }

                std::size_t read =
                    caMessage._payload->getPosition() - pos;

                toRead -= read;
            }
        }
        _receivedAppMessages.push_back(caMessage);
    }


    void readPollOne() {
        _readPollOneCount++;
        if (_readPollOneCallback.get() != 0)
            _readPollOneCallback->readPollOne();
    }


    void writePollOne() {
        _writePollOneCount++;
        if (_writePollOneCallback.get() != 0)
            _writePollOneCallback->writePollOne();
    }


    void close()  {
        _closedCount++;
    }

    bool isOpen() {
        return _closedCount == 0;
    }

    ReadMode getReadMode() {
        return _readMode;
    }

    WriteMode getWriteMode() {
        return _writeMode;
    }

    ByteBuffer*  getSendBuffer()
    {
        return &_sendBuffer;
    }

    const osiSockAddr* getLastReadBufferSocketAddress()
    {
        return &_dummyAddress;
    }

    void invalidDataStreamHandler() {
        _invalidDataStreamCount++;
    }

    void scheduleSend() {
        _scheduleSendCount++;
    }

    void sendCompleted() {
        _sendCompletedCount++;
    }

    void breakSender() {
        enqueueSendRequest(std::tr1::shared_ptr<TransportSender>(new TransportSenderDisconnect()));
    }

    bool terminated() {
        return false;
    }

    void cachedSerialize(
        const std::tr1::shared_ptr<const Field>& field,
        ByteBuffer* buffer) {
        field->serialize(buffer, this);
    }

    bool acquire(
        std::tr1::shared_ptr<ClientChannelImpl> const & client)
    {
        return false;
    }

    bool directSerialize(
        ByteBuffer *existingBuffer,
        const char* toSerialize,
        std::size_t elementCount,
        std::size_t elementSize)  {
        return false;
    }

    bool directDeserialize(
        ByteBuffer *existingBuffer,
        char* deserializeTo,
        std::size_t elementCount,
        std::size_t elementSize)  {
        return false;
    }

    std::tr1::shared_ptr<const Field>
    cachedDeserialize(ByteBuffer* buffer)
    {
        return std::tr1::shared_ptr<const Field>();
    }

    void release(pvAccessID clientId) {}

    std::string getType() const
    {
        return std::string("TCP");
    }

    const osiSockAddr& getRemoteAddress() const  {
        return dummyAddr;
    }
    std::string dummyRemoteName;
    const std::string& getRemoteName() const {
        return dummyRemoteName;
    }

    epics::pvData::int8 getRevision() const
    {
        return PVA_PROTOCOL_REVISION;
    }

    std::size_t getReceiveBufferSize() const  {
        return 16384;
    }

    epics::pvData::int16 getPriority() const  {
        return 0;
    }

    std::size_t getSocketReceiveBufferSize() const
    {
        return 16384;
    }

    void setRemoteRevision(epics::pvData::int8 revision)  {}

    void setRemoteTransportSocketReceiveBufferSize(
        std::size_t socketReceiveBufferSize)  {}

    void setRemoteTransportReceiveBufferSize(
        std::size_t remoteTransportReceiveBufferSize)  {}

    void changedTransport() {}

    void flushSendQueue() { };

    bool verify(epics::pvData::int32 timeoutMs) {
        return true;
    }

    void verified(epics::pvData::Status const &) {}

    void aliveNotification() {}

    void authNZMessage(epics::pvData::PVStructure::shared_pointer const & data) {}


    bool isClosed() {
        return false;
    }


    osiSockAddr dummyAddr;
    std::size_t _closedCount;
    std::size_t _invalidDataStreamCount;
    std::size_t _scheduleSendCount;
    std::size_t _sendCompletedCount;
    std::size_t _sendBufferFullCount;
    std::size_t _readPollOneCount;
    std::size_t _writePollOneCount;
    bool _throwExceptionOnSend;
    bool _readPayload;
    bool _disconnected;
    int _forcePayloadRead;

    epics::auto_ptr<epics::pvData::ByteBuffer> _readBuffer;
    epics::pvData::ByteBuffer _writeBuffer;

    std::vector<PVAMessage> _receivedAppMessages;
    std::vector<PVAMessage> _receivedControlMessages;

    epics::auto_ptr<ReadPollOneCallback> _readPollOneCallback;
    epics::auto_ptr<WritePollOneCallback> _writePollOneCallback;

    osiSockAddr _dummyAddress;

protected:

    void sendBufferFull(int tries) {
        testDiag("sendBufferFull tries=%d", tries);
        if(tries>10) // arbitrary limit
            testAbort("Stuck");
        _sendBufferFullCount++;
        _writeOpReady = false;
        _writeMode = WAIT_FOR_READY_SIGNAL;
        this->writePollOne();
        _writeMode = PROCESS_SEND_QUEUE;
    }
};


class CodecTest {

public:

    int runAllTest() {
        testPlan(5883);
        testHeaderProcess();
        testInvalidHeaderMagic();
        testInvalidHeaderSegmentedInNormal();
        testInvalidHeaderPayloadNotRead();
        testHeaderSplitRead();
        testNonEmptyPayload();
        testNormalAlignment();
        testSegmentedMessage();
        //testSegmentedInvalidInBetweenFlagsMessage();
        testSegmentedMessageAlignment();
        testSegmentedSplitMessage();
        testStartMessage();
        testStartMessageNonEmptyPayload();
        testStartMessageNormalAlignment();
        testStartMessageSegmentedMessage();
        testStartMessageSegmentedMessageAlignment();
        testReadNormalConnectionLoss();
        testSegmentedSplitConnectionLoss();
        testSendConnectionLoss();
        testEnqueueSendRequest();
        testEnqueueSendDirectRequest();
        testSendException();
        testSendHugeMessagePartes();
        testRecipient();
        testInvalidArguments();
        testDefaultModes();
        testEnqueueSendRequestExceptionThrown();
        testBlockingProcessQueueTest();
        return testDone();
    }

    virtual ~CodecTest() {}

protected:

    static const std::size_t DEFAULT_BUFFER_SIZE = 10240;

private:

    void testHeaderProcess() {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x01);
        codec._readBuffer->put((int8_t)0x23);
        codec._readBuffer->putInt(0x456789AB);
        codec._readBuffer->flip();


        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);

        PVAMessage header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(header._flags == 0x01,
               "%s: header._flags == 0x01", CURRENT_FUNCTION);
        testOk(header._command == 0x23,
               "%s: header._command == 0x23", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x456789AB,
               "%s: header._payloadSize == 0x456789AB", CURRENT_FUNCTION);

        codec.reset();

        // two at the time, app and control
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x00);
        codec._readBuffer->put((int8_t)0x20);
        codec._readBuffer->putInt(0x00000000);

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x81);
        codec._readBuffer->put((int8_t)0xEE);
        codec._readBuffer->putInt(0xDDCCBBAA);
        codec._readBuffer->flip();

        codec.processRead();

        testOk(0 == codec._invalidDataStreamCount,
               "%s: 0 == codec._invalidDataStreamCount",
               CURRENT_FUNCTION);
        testOk(0 == codec._closedCount,
               "%s: 0 == codec._closedCount", CURRENT_FUNCTION);
        testOk(1 == codec._receivedControlMessages.size(),
               "%s: 1 == codec._receivedControlMessages.size()",
               CURRENT_FUNCTION);
        testOk(1 == codec._receivedAppMessages.size(),
               "%s: 1 == codec._receivedAppMessages.size()",
               CURRENT_FUNCTION);


        // app, no payload
        header = codec._receivedAppMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)0x00,
               "%s: header._flags == 0x00", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0x20,
               "%s: header._command == 0x20", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x00000000,
               "%s: header._payloadSize == 0x00000000", CURRENT_FUNCTION);

        // control
        header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)0x81,
               "%s: header._flags == 0x81", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0xEE,
               "%s: header._command == 0xEE", CURRENT_FUNCTION);
        testOk(header._payloadSize == (int32_t)0xDDCCBBAA,
               "%s: header._payloadSize == 0xDDCCBBAA",
               CURRENT_FUNCTION);
    }


    void testInvalidHeaderMagic()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readBuffer->put((int8_t)00);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x01);
        codec._readBuffer->put((int8_t)0x23);
        codec._readBuffer->putInt(0x456789AB);
        codec._readBuffer->flip();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 1,
               "%s: codec._invalidDataStreamCount == 1",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);
    }


    void testInvalidHeaderSegmentedInNormal()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        int8_t invalidFlagsValues[] =
        {(int8_t)0x20, (int8_t)(0x30+0x80)};

        std::size_t size=sizeof(invalidFlagsValues)/sizeof(int8_t);

        for (std::size_t i = 0; i < size; i++)
        {
            TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);
            codec._readBuffer->put(PVA_MAGIC);
            codec._readBuffer->put(PVA_VERSION);
            codec._readBuffer->put(invalidFlagsValues[i]);
            codec._readBuffer->put((int8_t)0x23);
            //codec._readBuffer->putInt(0);
            codec._readBuffer->putInt(i);   // to check zero-payload
            codec._readBuffer->flip();

            codec.processRead();

            testOk(codec._invalidDataStreamCount == (i != 0 ? 1 : 0),
                   //testOk(codec._invalidDataStreamCount == 1,
                   "%s: codec._invalidDataStreamCount == 1",
                   CURRENT_FUNCTION);
            testOk(codec._closedCount == 0,
                   "%s: codec._closedCount == 0", CURRENT_FUNCTION);
            testOk(codec._receivedControlMessages.size() == 0,
                   "%s: codec._receivedControlMessages.size() == 0 ",
                   CURRENT_FUNCTION);
            testOk(codec._receivedAppMessages.size() == 0,
                   "%s: codec._receivedAppMessages.size() == 0",
                   CURRENT_FUNCTION);
        }
    }


    void testInvalidHeaderPayloadNotRead()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x80);
        codec._readBuffer->put((int8_t)0x23);
        codec._readBuffer->putInt(0x456789AB);
        codec._readBuffer->flip();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 1,
               "%s: codec._invalidDataStreamCount == 1",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

    }


    void testHeaderSplitRead()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x01);
        codec._readBuffer->flip();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);

        codec._readBuffer->clear();

        codec._readBuffer->put((int8_t)0x23);
        codec._readBuffer->putInt(0x456789AB);
        codec._readBuffer->flip();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);


        // app, no payload
        PVAMessage header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)0x01,
               "%s: header._flags == 0x01", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0x23,
               "%s: header._command == 0x23", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x456789AB,
               "%s: header._payloadSize == 0x456789AB", CURRENT_FUNCTION);
    }


    void testNonEmptyPayload()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        // no misalignment
        codec._readPayload = true;

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x80);
        codec._readBuffer->put((int8_t)0x23);
        codec._readBuffer->putInt(1); // size
        codec._readBuffer->put((int8_t)0);
        codec._readBuffer->flip();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

        // app, no payload
        PVAMessage header = codec._receivedAppMessages[0];

        testOk(header._payload.get() != 0,
               "%s: header._payload.get() != 0", CURRENT_FUNCTION);

        header._payload->flip();

        testOk(
            1 == header._payload->getLimit(),
            "%s: 1 == header._payload->getLimit()",
            CURRENT_FUNCTION);

    }


    void testNormalAlignment()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x80);
        codec._readBuffer->put((int8_t)0x23);
        const int32_t payloadSize1 = 2;
        codec._readBuffer->putInt(payloadSize1);

        for (int32_t i = 0; i < payloadSize1; i++)
            codec._readBuffer->put((int8_t)i);
        // align
        std::size_t aligned = payloadSize1;

        for (std::size_t i = payloadSize1; i < aligned; i++)
            codec._readBuffer->put((int8_t)0xFF);


        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x80);
        codec._readBuffer->put((int8_t)0x45);

        const int32_t payloadSize2 = 1;
        codec._readBuffer->putInt(payloadSize2);

        for (int32_t i = 0; i < payloadSize2; i++) {
            codec._readBuffer->put((int8_t)i);
        }

        aligned = payloadSize2;

        for (std::size_t i = payloadSize2; i < aligned; i++) {
            codec._readBuffer->put((int8_t)0xFF);
        }

        codec._readBuffer->flip();
        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 2,
               "%s: codec._receivedAppMessages.size() == 2",
               CURRENT_FUNCTION);

        PVAMessage msg = codec._receivedAppMessages[0];

        testOk(msg._payloadSize == payloadSize1,
               "%s: msg._payloadSize == payloadSize1", CURRENT_FUNCTION);

        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);


        msg._payload->flip();

        testOk((std::size_t)payloadSize1 == msg._payload->getLimit(),
               "%s: payloadSize1, msg._payload->getLimit()",
               CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++) {
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getByte()",
                   CURRENT_FUNCTION);
        }

        msg = codec._receivedAppMessages[1];

        testOk(msg._payloadSize == payloadSize2,
               "%s: msg._payloadSize == payloadSize2", CURRENT_FUNCTION);

        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        msg._payload->flip();

        testOk((std::size_t)payloadSize2 == msg._payload->getLimit(),
               "%s: payloadSize2 == msg._payload->getLimit()",
               CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++) {
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getByte()",
                   CURRENT_FUNCTION);
        }
    }


    class ReadPollOneCallbackForTestSplitAlignment:
        public ReadPollOneCallback {

    public:

        ReadPollOneCallbackForTestSplitAlignment(
            TestCodec & codec,
            int32_t payloadSize1,
            int32_t payloadSize2):
            _codec(codec), _payloadSize1(payloadSize1),
            _payloadSize2(payloadSize2)  {}


        void readPollOne()  {

            if (_codec._readPollOneCount == 1)
            {
                _codec._readBuffer->clear();
                for (int32_t i = _payloadSize1-2;
                        i < _payloadSize1; i++) {
                    _codec._readBuffer->put((int8_t)i);
                }

                // align
                std::size_t aligned = _payloadSize1;

                for (std::size_t i = _payloadSize1; i < aligned; i++)
                    _codec._readBuffer->put((int8_t)0xFF);


                _codec._readBuffer->put(PVA_MAGIC);
                _codec._readBuffer->put(PVA_VERSION);
                _codec._readBuffer->put((int8_t)0x80);
                _codec._readBuffer->put((int8_t)0x45);
                _codec._readBuffer->putInt(_payloadSize2);

                for (int32_t i = 0; i < _payloadSize2; i++) {
                    _codec._readBuffer->put((int8_t)i);
                }

                _codec._readBuffer->flip();
            }
            else if (_codec._readPollOneCount == 2)
            {
                _codec._readBuffer->clear();

                std::size_t aligned = _payloadSize2;

                for (std::size_t i = _payloadSize2; i < aligned; i++) {
                    _codec._readBuffer->put((int8_t)0xFF);
                }

                _codec._readBuffer->flip();
            }

            else
                throw std::logic_error("should not happen");
        }

    private:
        TestCodec &_codec;
        int8_t _payloadSize1;
        int8_t _payloadSize2;
    };


    void testSegmentedMessage()
    {

        // no misalignment
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;

        // 1st
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x90);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize1 = 1;
        codec._readBuffer->putInt(payloadSize1);

        int32_t c = 0;
        for (int32_t i = 0; i < payloadSize1; i++)
            codec._readBuffer->put((int8_t)(c++));

        // 2nd
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xB0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize2 = 2;
        codec._readBuffer->putInt(payloadSize2);

        for (int32_t i = 0; i < payloadSize2; i++)
            codec._readBuffer->put((int8_t)(c++));

        // control in between
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x81);
        codec._readBuffer->put((int8_t)0xEE);
        codec._readBuffer->putInt(0xDDCCBBAA);

        // 3rd
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xB0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize3 = 1;
        codec._readBuffer->putInt(payloadSize3);

        for (int32_t i = 0; i < payloadSize3; i++)
            codec._readBuffer->put((int8_t)(c++));

        // 4t (last)
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xA0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize4 = 2;
        codec._readBuffer->putInt(payloadSize4);

        for (int32_t i = 0; i < payloadSize4; i++)
            codec._readBuffer->put((int8_t)(c++));

        codec._readBuffer->flip();

        int32_t payloadSizeSum =
            payloadSize1+payloadSize2+payloadSize3+payloadSize4;

        codec._forcePayloadRead = payloadSizeSum;

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);
        testOk(codec._readPollOneCount == 0,
               "%s: codec._readPollOneCount == 0", CURRENT_FUNCTION);


        PVAMessage msg = codec._receivedAppMessages[0];

        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        msg._payload->flip();

        testOk(
            (std::size_t)payloadSizeSum == msg._payload->getLimit(),
            "%s: payloadSizeSum == msg._payload->getLimit()",
            CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++) {
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getint8_t()",
                   CURRENT_FUNCTION);
        }


        msg = codec._receivedControlMessages[0];

        testOk(msg._version == PVA_VERSION,
               "%s: msg._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(msg._flags == (int8_t)0x81,
               "%s: msg._flags == 0x81", CURRENT_FUNCTION);
        testOk(msg._command == (int8_t)0xEE,
               "%s: msg._command == 0xEE", CURRENT_FUNCTION);
        testOk(msg._payloadSize == (int32_t)0xDDCCBBAA,
               "%s: msg._payloadSize == 0xDDCCBBAA", CURRENT_FUNCTION);
    }


    void testSegmentedInvalidInBetweenFlagsMessage()
    {
        // no misalignment
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;

        // 1st
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x90);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize1 = 1;
        codec._readBuffer->putInt(payloadSize1);

        int32_t c = 0;
        for (int32_t i = 0; i < payloadSize1; i++)
            codec._readBuffer->put((int8_t)(c++));

        // 2nd
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        // invalid flag, should be 0xB0
        codec._readBuffer->put((int8_t)0x90);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize2 = 2;
        codec._readBuffer->putInt(payloadSize2);

        for (int32_t i = 0; i < payloadSize2; i++)
            codec._readBuffer->put((int8_t)(c++));

        // control in between
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x81);
        codec._readBuffer->put((int8_t)0xEE);
        codec._readBuffer->putInt(0xDDCCBBAA);

        // 3rd
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xB0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize3 = 1;
        codec._readBuffer->putInt(payloadSize3);

        for (int32_t i = 0; i < payloadSize3; i++)
            codec._readBuffer->put((int8_t)(c++));

        // 4t (last)
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xA0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize4 = 2;
        codec._readBuffer->putInt(payloadSize4);

        for (int32_t i = 0; i < payloadSize4; i++)
            codec._readBuffer->put((int8_t)(c++));

        codec._readBuffer->flip();

        int32_t payloadSizeSum =
            payloadSize1+payloadSize2+payloadSize3+payloadSize4;
        codec._forcePayloadRead = payloadSizeSum;

        try {
            codec.processRead();
            testFail(
                "%s: invalid_data_stream_exception, but not reported",
                CURRENT_FUNCTION);
        } catch(invalid_data_stream_exception &) {
            testOk(true, "%s: invalid_data_stream_exception reported",
                   CURRENT_FUNCTION);
        }

        testOk(codec._invalidDataStreamCount == 1,
               "%s: codec._invalidDataStreamCount == 1",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);
    }


    void testSegmentedMessageAlignment()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;

        // 1st
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x90);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize1 = 1+1;
        codec._readBuffer->putInt(payloadSize1);

        int32_t c = 0;
        for (int32_t i = 0; i < payloadSize1; i++)
            codec._readBuffer->put((int8_t)(c++));

        std::size_t aligned = payloadSize1;
        for (std::size_t i = payloadSize1; i < aligned; i++)
            codec._readBuffer->put((int8_t)0xFF);


        // 2nd
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xB0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize2 = 1;
        int32_t payloadSize2Real = payloadSize2;

        codec._readBuffer->putInt(payloadSize2Real);

        for (int32_t i = 0; i < payloadSize2; i++)
            codec._readBuffer->put((int8_t)(c++));

        aligned = payloadSize2Real;

        for (std::size_t i = payloadSize2Real; i < aligned; i++)
            codec._readBuffer->put((int8_t)0xFF);

        // 3rd
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xB0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize3 = 3;
        int32_t payloadSize3Real = payloadSize3;
        codec._readBuffer->putInt(payloadSize3Real);

        for (int32_t i = 0; i < payloadSize3; i++)
            codec._readBuffer->put((int8_t)(c++));

        aligned = payloadSize3Real;

        for (std::size_t i = payloadSize3Real; i < aligned; i++)
            codec._readBuffer->put((int8_t)0xFF);

        // 4t (last)
        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0xA0);
        codec._readBuffer->put((int8_t)0x01);

        int32_t payloadSize4 = 2+3;
        int32_t payloadSize4Real = payloadSize4;

        codec._readBuffer->putInt(payloadSize4Real);

        for (int32_t i = 0; i < payloadSize4; i++)
            codec._readBuffer->put((int8_t)(c++));

        aligned = payloadSize4Real;

        for (std::size_t i = payloadSize4Real; i < aligned; i++)
            codec._readBuffer->put((int8_t)0xFF);

        codec._readBuffer->flip();

        int32_t payloadSizeSum =
            payloadSize1+payloadSize2+payloadSize3+payloadSize4;

        codec._forcePayloadRead = payloadSizeSum;

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);
        testOk(codec._readPollOneCount == 0,
               "%s: codec._readPollOneCount == 0", CURRENT_FUNCTION);

        PVAMessage msg = codec._receivedAppMessages[0];

        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        msg._payload->flip();

        testOk(
            (std::size_t)payloadSizeSum == msg._payload->getLimit(),
            "%s: payloadSizeSum == msg._payload->getLimit()",
            CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++)
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getByte()",
                   CURRENT_FUNCTION);
    }


    class ReadPollOneCallbackForTestSegmentedSplitMessage:
        public ReadPollOneCallback {

    public:

        ReadPollOneCallbackForTestSegmentedSplitMessage(
            TestCodec & codec,
            int32_t realReadBufferEnd):
            _codec(codec), _realReadBufferEnd(realReadBufferEnd) {}


        void readPollOne()  {
            if (_codec._readPollOneCount == 1)
            {
                _codec._readBuffer->setLimit(_realReadBufferEnd);
            }
            else
                throw std::logic_error("should not happen");
        }

    private:
        TestCodec &_codec;
        std::size_t _realReadBufferEnd;
    };


    void testSegmentedSplitMessage()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        for (int32_t firstMessagePayloadSize = 1;	// cannot be zero
                firstMessagePayloadSize <= 3;
                firstMessagePayloadSize++)
        {
            for (int32_t secondMessagePayloadSize = 0;
                    secondMessagePayloadSize <= 2;
                    secondMessagePayloadSize++)
            {
                // cannot be zero
                for (int32_t thirdMessagePayloadSize = 1;
                        thirdMessagePayloadSize <= 2;
                        thirdMessagePayloadSize++)
                {
                    std::size_t splitAt = 1;
                    while (true)
                    {
                        TestCodec codec(DEFAULT_BUFFER_SIZE,
                                        DEFAULT_BUFFER_SIZE);

                        codec._readPayload = true;

                        // 1st
                        codec._readBuffer->put(PVA_MAGIC);
                        codec._readBuffer->put(PVA_VERSION);
                        codec._readBuffer->put((int8_t)0x90);
                        codec._readBuffer->put((int8_t)0x01);

                        int32_t payloadSize1 = firstMessagePayloadSize;
                        codec._readBuffer->putInt(payloadSize1);

                        int32_t c = 0;
                        for (int32_t i = 0; i < payloadSize1; i++)
                            codec._readBuffer->put((int8_t)(c++));

                        std::size_t aligned = payloadSize1;

                        for (std::size_t i = payloadSize1; i < aligned; i++)
                            codec._readBuffer->put((int8_t)0xFF);

                        // 2nd
                        codec._readBuffer->put(PVA_MAGIC);
                        codec._readBuffer->put(PVA_VERSION);
                        codec._readBuffer->put((int8_t)0xB0);
                        codec._readBuffer->put((int8_t)0x01);

                        int32_t payloadSize2 = secondMessagePayloadSize;
                        int payloadSize2Real = payloadSize2;

                        codec._readBuffer->putInt(payloadSize2Real);

                        for (int32_t i = 0; i < payloadSize2; i++)
                            codec._readBuffer->put((int8_t)(c++));

                        aligned = payloadSize2Real;

                        for (std::size_t i = payloadSize2Real;
                                i < aligned; i++)
                            codec._readBuffer->put((int8_t)0xFF);

                        // 3rd
                        codec._readBuffer->put(PVA_MAGIC);
                        codec._readBuffer->put(PVA_VERSION);
                        codec._readBuffer->put((int8_t)0xA0);
                        codec._readBuffer->put((int8_t)0x01);

                        int32_t payloadSize3 = thirdMessagePayloadSize;
                        int32_t payloadSize3Real = payloadSize3;

                        codec._readBuffer->putInt(payloadSize3Real);

                        for (int32_t i = 0; i < payloadSize3; i++)
                            codec._readBuffer->put((int8_t)(c++));

                        aligned = payloadSize3Real;

                        for (std::size_t i = payloadSize3Real;
                                i < aligned; i++)
                            codec._readBuffer->put((int8_t)0xFF);

                        codec._readBuffer->flip();

                        std::size_t realReadBufferEnd =
                            codec._readBuffer->getLimit();

                        if (splitAt++ == realReadBufferEnd)
                            break;

                        codec._readBuffer->setLimit(splitAt);

                        codec._readPollOneCallback.reset(new ReadPollOneCallbackForTestSegmentedSplitMessage
                                                         (codec, realReadBufferEnd));


                        int32_t payloadSizeSum =
                            payloadSize1+payloadSize2+payloadSize3;

                        codec._forcePayloadRead = payloadSizeSum;

                        codec.processRead();

                        while (codec._invalidDataStreamCount == 0 &&
                                codec._readBuffer->getPosition() !=
                                realReadBufferEnd)
                        {
                            codec._readPollOneCount++;
                            codec._readPollOneCallback->readPollOne();
                            codec.processRead();
                        }

                        testOk(codec._invalidDataStreamCount == 0,
                               "%s: codec._invalidDataStreamCount == 0",
                               CURRENT_FUNCTION);
                        testOk(codec._closedCount == 0,
                               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
                        testOk(codec._receivedControlMessages.size() == 0,
                               "%s: codec._receivedControlMessages.size() == 0 ",
                               CURRENT_FUNCTION);
                        testOk(codec._receivedAppMessages.size() == 1,
                               "%s: codec._receivedAppMessages.size() == 1",
                               CURRENT_FUNCTION);


                        if (splitAt == realReadBufferEnd) {
                            testOk(0 == codec._readPollOneCount,
                                   "%s: 0 == codec._readPollOneCount",
                                   CURRENT_FUNCTION);
                        }
                        else {
                            testOk(1 == codec._readPollOneCount,
                                   "%s: 1 == codec._readPollOneCount",
                                   CURRENT_FUNCTION);
                        }

                        PVAMessage msg = codec._receivedAppMessages[0];

                        testOk(msg._payload.get() != 0,
                               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

                        msg._payload->flip();

                        testOk((std::size_t)payloadSizeSum ==
                               msg._payload->getLimit(),
                               "%s: payloadSizeSum == msg._payload->getLimit()",
                               CURRENT_FUNCTION);

                        for (int32_t i = 0; i < msg._payloadSize; i++) {
                            testOk((int8_t)i == msg._payload->getByte(),
                                   "%s: (int8_t)i == msg._payload->getByte()",
                                   CURRENT_FUNCTION);
                        }
                    }
                }
            }
        }
    }


    void testStartMessage()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec.putControlMessage((int8_t)0x23, 0x456789AB);

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);


        PVAMessage header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x01),
               "%s: header._flags == 0x(0|8)1", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0x23,
               "%s: header._command == 0x23", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x456789AB,
               "%s: header._payloadSize == 0x456789AB", CURRENT_FUNCTION);

        codec.reset();

        // two at the time, app and control
        codec.setByteOrder(EPICS_ENDIAN_LITTLE);
        codec.startMessage((int8_t)0x20, 0x00000000);
        codec.endMessage();

        codec.setByteOrder(EPICS_ENDIAN_BIG);
        codec.putControlMessage((int8_t)0xEE, 0xDDCCBBAA);

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

        // app, no payload
        header = codec._receivedAppMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)0x00,
               "%s: header._flags == 0x00", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0x20,
               "%s: header._command == 0x20", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x00000000,
               "%s: header._payloadSize == 0x00000000", CURRENT_FUNCTION);

        // control
        header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)0x81,
               "%s: header._flags == 0x81", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0xEE,
               "%s: header._command == 0xEE", CURRENT_FUNCTION);
        testOk(header._payloadSize == (int32_t)0xDDCCBBAA,
               "%s: header._payloadSize == 0xDDCCBBAA", CURRENT_FUNCTION);
    }


    void testStartMessageNonEmptyPayload()
    {
        // no misalignment
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;
        codec.startMessage((int8_t)0x23, 0);

        codec.ensureBuffer(1);
        codec.getSendBuffer()->put((int8_t)0);

        codec.endMessage();

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

        // app, no payload
        PVAMessage header = codec._receivedAppMessages[0];

        testOk(header._payload.get() != 0,
               "%s: header._payload.get() != 0", CURRENT_FUNCTION);

        header._payload->flip();

        testOk(1u == header._payload->getLimit(),
               "%s: PVA_ALIGNMENT == header._payload->getLimit()",
               CURRENT_FUNCTION);
    }


    void testStartMessageNormalAlignment()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;

        codec.startMessage((int8_t)0x23, 0);
        int32_t payloadSize1 = 1+1;
        codec.ensureBuffer(payloadSize1);

        for (int32_t i = 0; i < payloadSize1; i++)
            codec.getSendBuffer()->put((int8_t)i);

        codec.endMessage();

        codec.startMessage((int8_t)0x45, 0);
        int32_t payloadSize2 = 1;
        codec.ensureBuffer(payloadSize2);

        for (int32_t i = 0; i < payloadSize2; i++)
            codec.getSendBuffer()->put((int8_t)i);

        codec.endMessage();

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 2,
               "%s: codec._receivedAppMessages.size() == 2",
               CURRENT_FUNCTION);

        PVAMessage msg = codec._receivedAppMessages[0];

        testOk(msg._payloadSize == payloadSize1,
               "%s: msg._payloadSize == payloadSize1", CURRENT_FUNCTION);
        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        msg._payload->flip();

        testOk((std::size_t)payloadSize1 ==
               msg._payload->getLimit(),
               "%s: payloadSize1 == msg._payload->getLimit()",
               CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++)
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getByte()",
                   CURRENT_FUNCTION);

        msg = codec._receivedAppMessages[1];

        testOk(msg._payloadSize == payloadSize2,
               "%s: msg._payloadSize == payloadSize2", CURRENT_FUNCTION);
        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        msg._payload->flip();

        testOk((std::size_t)payloadSize2 ==
               msg._payload->getLimit(),
               "%s: payloadSize2 == msg._payload->getLimit()",
               CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++)
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getByte()",
                   CURRENT_FUNCTION);
    }


    void testStartMessageSegmentedMessage()
    {
        // no misalignment
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;

        codec.startMessage((int8_t)0x01, 0);

        int32_t c = 0;

        int32_t payloadSize1 = 1;
        for (int32_t i = 0; i < payloadSize1; i++)
            codec.getSendBuffer()->put((int8_t)(c++));

        codec.flush(false);

        int32_t payloadSize2 = 2;
        for (int32_t i = 0; i < payloadSize2; i++)
            codec.getSendBuffer()->put((int8_t)(c++));

        codec.flush(false);

        int32_t payloadSize3 = 1;
        for (int32_t i = 0; i < payloadSize3; i++)
            codec.getSendBuffer()->put((int8_t)(c++));

        codec.flush(false);

        int32_t payloadSize4 = 2;
        for (int32_t i = 0; i < payloadSize4; i++)
            codec.getSendBuffer()->put((int8_t)(c++));

        codec.endMessage();

        codec.transferToReadBuffer();

        int32_t payloadSizeSum =
            payloadSize1+payloadSize2+payloadSize3+payloadSize4;

        codec._forcePayloadRead = payloadSizeSum;

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);
        testOk(codec._readPollOneCount == 0,
               "%s: codec._readPollOneCount == 0", CURRENT_FUNCTION);

        PVAMessage msg = codec._receivedAppMessages[0];

        testOk(msg._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        msg._payload->flip();

        testOk((std::size_t)payloadSizeSum ==
               msg._payload->getLimit(),
               "%s: payloadSizeSum == msg._payload->getLimit()",
               CURRENT_FUNCTION);

        for (int32_t i = 0; i < msg._payloadSize; i++)
            testOk((int8_t)i == msg._payload->getByte(),
                   "%s: (int8_t)i == msg._payload->getByte()",
                   CURRENT_FUNCTION);
    }


    void testStartMessageSegmentedMessageAlignment()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        for (int32_t firstMessagePayloadSize = 1;	// cannot be zero
                firstMessagePayloadSize <= 3;
                firstMessagePayloadSize++)
        {
            for (int32_t secondMessagePayloadSize = 0;
                    secondMessagePayloadSize <= 2;
                    secondMessagePayloadSize++)
            {
                // cannot be zero
                for (int32_t thirdMessagePayloadSize = 1;
                        thirdMessagePayloadSize <= 2;
                        thirdMessagePayloadSize++)
                {
                    // cannot be zero
                    for (int32_t fourthMessagePayloadSize = 1;
                            fourthMessagePayloadSize <= 2;
                            fourthMessagePayloadSize++)
                    {
                        TestCodec codec(DEFAULT_BUFFER_SIZE,
                                        DEFAULT_BUFFER_SIZE);

                        codec._readPayload = true;

                        codec.startMessage((int8_t)0x01, 0);

                        int32_t c = 0;

                        int32_t payloadSize1 = firstMessagePayloadSize;
                        for (int32_t i = 0; i < payloadSize1; i++)
                            codec.getSendBuffer()->put((int8_t)(c++));

                        codec.flush(false);

                        int32_t payloadSize2 = secondMessagePayloadSize;
                        for (int32_t i = 0; i < payloadSize2; i++)
                            codec.getSendBuffer()->put((int8_t)(c++));

                        codec.flush(false);

                        int32_t payloadSize3 = thirdMessagePayloadSize;
                        for (int32_t i = 0; i < payloadSize3; i++)
                            codec.getSendBuffer()->put((int8_t)(c++));

                        codec.flush(false);

                        int32_t payloadSize4 = fourthMessagePayloadSize;
                        for (int32_t i = 0; i < payloadSize4; i++)
                            codec.getSendBuffer()->put((int8_t)(c++));

                        codec.endMessage();

                        codec.transferToReadBuffer();

                        int32_t payloadSizeSum =
                            payloadSize1+payloadSize2+payloadSize3
                            +payloadSize4;

                        codec._forcePayloadRead = payloadSizeSum;

                        codec.processRead();

                        testOk(codec._invalidDataStreamCount == 0,
                               "%s: codec._invalidDataStreamCount == 0",
                               CURRENT_FUNCTION);
                        testOk(codec._closedCount == 0,
                               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
                        testOk(codec._receivedControlMessages.size() == 0,
                               "%s: codec._receivedControlMessages.size() == 0 ",
                               CURRENT_FUNCTION);
                        testOk(codec._receivedAppMessages.size() == 1,
                               "%s: codec._receivedAppMessages.size() == 1",
                               CURRENT_FUNCTION);
                        testOk(codec._readPollOneCount == 0,
                               "%s: codec._readPollOneCount == 0",
                               CURRENT_FUNCTION);

                        PVAMessage msg = codec._receivedAppMessages[0];

                        testOk(msg._payload.get() != 0,
                               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

                        msg._payload->flip();

                        testOk((std::size_t)payloadSizeSum ==
                               msg._payload->getLimit(),
                               "%s: payloadSizeSum == msg._payload->getLimit()",
                               CURRENT_FUNCTION);

                        for (int32_t i = 0; i < msg._payloadSize; i++) {
                            if ((int8_t)i != msg._payload->getByte()) {
                                testFail(
                                    "%s: (int8_t)%d == msg._payload->getByte()",
                                    CURRENT_FUNCTION, (int8_t)i);
                            }
                        }
                    }
                }
            }
        }
    }


    void testReadNormalConnectionLoss()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;
        codec._disconnected = true;

        codec._readBuffer->put(PVA_MAGIC);
        codec._readBuffer->put(PVA_VERSION);
        codec._readBuffer->put((int8_t)0x01);
        codec._readBuffer->put((int8_t)0x23);
        codec._readBuffer->putInt(0x456789AB);

        codec._readBuffer->flip();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 1,
               "%s: codec._closedCount == 1", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);
    }


    class ReadPollOneCallbackForTestSegmentedSplitConnectionLoss:
        public ReadPollOneCallback {

    public:

        ReadPollOneCallbackForTestSegmentedSplitConnectionLoss(
            TestCodec & codec): _codec(codec) {}


        void readPollOne()  {
            if (_codec._readPollOneCount == 1)
            {
                _codec._disconnected = true;
            }
            else
                throw std::logic_error("should not happen");
        }

    private:
        TestCodec &_codec;
    };


    void testSegmentedSplitConnectionLoss()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);


        for (int32_t firstMessagePayloadSize = 1;	// cannot be zero
                firstMessagePayloadSize <= 3;
                firstMessagePayloadSize++)
        {
            for (int32_t secondMessagePayloadSize = 0;
                    secondMessagePayloadSize <= 2;
                    secondMessagePayloadSize++)
            {
                // cannot be zero
                for (int32_t thirdMessagePayloadSize = 1;
                        thirdMessagePayloadSize <= 2;
                        thirdMessagePayloadSize++)
                {
                    std::size_t splitAt = 1;

                    while (true)
                    {
                        TestCodec codec(DEFAULT_BUFFER_SIZE,
                                        DEFAULT_BUFFER_SIZE);

                        codec._readPayload = true;

                        // 1st
                        codec._readBuffer->put(PVA_MAGIC);
                        codec._readBuffer->put(PVA_VERSION);
                        codec._readBuffer->put((int8_t)0x90);
                        codec._readBuffer->put((int8_t)0x01);

                        int32_t payloadSize1 = firstMessagePayloadSize;
                        codec._readBuffer->putInt(payloadSize1);

                        int32_t c = 0;
                        for (int32_t i = 0; i < payloadSize1; i++)
                            codec._readBuffer->put((int8_t)(c++));

                        std::size_t aligned = payloadSize1;

                        for (std::size_t i = payloadSize1; i < aligned; i++)
                            codec._readBuffer->put((int8_t)0xFF);

                        // 2nd
                        codec._readBuffer->put(PVA_MAGIC);
                        codec._readBuffer->put(PVA_VERSION);
                        codec._readBuffer->put((int8_t)0xB0);
                        codec._readBuffer->put((int8_t)0x01);

                        int32_t payloadSize2 = secondMessagePayloadSize;
                        int32_t payloadSize2Real = payloadSize2;

                        codec._readBuffer->putInt(payloadSize2Real);

                        for (int32_t i = 0; i < payloadSize2; i++)
                            codec._readBuffer->put((int8_t)(c++));

                        aligned = payloadSize2Real;

                        for (std::size_t i = payloadSize2Real;
                                i < aligned; i++)
                            codec._readBuffer->put((int8_t)0xFF);

                        // 3rd
                        codec._readBuffer->put(PVA_MAGIC);
                        codec._readBuffer->put(PVA_VERSION);
                        codec._readBuffer->put((int8_t)0xA0);
                        codec._readBuffer->put((int8_t)0x01);

                        int32_t payloadSize3 = thirdMessagePayloadSize;
                        int32_t payloadSize3Real = payloadSize3;

                        codec._readBuffer->putInt(payloadSize3Real);

                        for (int32_t i = 0; i < payloadSize3; i++)
                            codec._readBuffer->put((int8_t)(c++));

                        aligned = payloadSize3Real;

                        for (std::size_t i = payloadSize3Real;
                                i < aligned; i++)
                            codec._readBuffer->put((int8_t)0xFF);

                        codec._readBuffer->flip();

                        std::size_t realReadBufferEnd =
                            codec._readBuffer->getLimit();

                        if (splitAt++ == realReadBufferEnd-1)
                            break;

                        codec._readBuffer->setLimit(splitAt);

                        codec._readPollOneCallback.reset(new
                                                         ReadPollOneCallbackForTestSegmentedSplitConnectionLoss
                                                         (codec));

                        int32_t payloadSizeSum =
                            payloadSize1+payloadSize2+payloadSize3;

                        codec._forcePayloadRead = payloadSizeSum;

                        codec.processRead();

                        while (codec._closedCount == 0 &&
                                codec._invalidDataStreamCount == 0 &&
                                codec._readBuffer->getPosition() !=
                                realReadBufferEnd)
                        {
                            codec._readPollOneCount++;
                            codec._readPollOneCallback->readPollOne();
                            codec.processRead();
                        }

                        testOk(codec._invalidDataStreamCount == 0,
                               "%s: codec._invalidDataStreamCount == 0",
                               CURRENT_FUNCTION);
                        testOk(codec._closedCount == 1,
                               "%s: codec._closedCount == 1", CURRENT_FUNCTION);
                    }
                }
            }
        }
    }


    void testSendConnectionLoss()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;
        codec._disconnected = true;

        codec.putControlMessage((int8_t)0x23, 0x456789AB);

        try
        {
            codec.transferToReadBuffer();
            testFail("%s: connection lost, but not reported",
                     CURRENT_FUNCTION);
        }
        catch (connection_closed_exception & ) {
            testOk(true, "%s: connection closed exception expected",
                   CURRENT_FUNCTION);
        }

        testOk(codec._closedCount == 1,
               "%s: codec._closedCount == 1", CURRENT_FUNCTION);
    }

    class TransportSenderForTestEnqueueSendRequest:
        public TransportSender {
    public:

        TransportSenderForTestEnqueueSendRequest(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.startMessage((int8_t)0x20, 0x00000000);
            _codec.endMessage();
        }

    private:
        TestCodec &_codec;
    };


    class TransportSender2ForTestEnqueueSendRequest:
        public TransportSender {
    public:

        TransportSender2ForTestEnqueueSendRequest(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.putControlMessage((int8_t)0xEE, 0xDDCCBBAA);
        }

    private:
        TestCodec &_codec;
    };


    void testEnqueueSendRequest()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSenderForTestEnqueueSendRequest(codec));

        std::tr1::shared_ptr<TransportSender> sender2 =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSender2ForTestEnqueueSendRequest(codec));

        // process
        codec.enqueueSendRequest(sender);
        codec.enqueueSendRequest(sender2);
        codec.breakSender();
        try {
            codec.processSendQueue();
        } catch(sender_break&) {}

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);


        // app, no payload
        PVAMessage header = codec._receivedAppMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x00),
               "%s: header._flags == 0x(0|8)0", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0x20,
               "%s: header._command == 0x20", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x00000000,
               "%s: header._payloadSize == 0x00000000", CURRENT_FUNCTION);

        // control
        header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x01),
               "%s: header._flags == 0x(0|8)1", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0xEE,
               "%s: header._command == 0xEE", CURRENT_FUNCTION);
        testOk(header._payloadSize == (int32_t)0xDDCCBBAA,
               "%s: header._payloadSize == 0xDDCCBBAA", CURRENT_FUNCTION);
    }


    class TransportSenderForTestEnqueueSendDirectRequest:
        public TransportSender {
    public:

        TransportSenderForTestEnqueueSendDirectRequest(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.startMessage((int8_t)0x20, 0x00000000);
            _codec.endMessage();
        }

    private:
        TestCodec &_codec;
    };


    class TransportSender2ForTestEnqueueSendDirectRequest:
        public TransportSender {
    public:

        TransportSender2ForTestEnqueueSendDirectRequest(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.putControlMessage((int8_t)0xEE,
                                     0xDDCCBBAA);
        }

    private:
        TestCodec &_codec;
    };


    void testEnqueueSendDirectRequest()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSenderForTestEnqueueSendDirectRequest(codec));
        std::tr1::shared_ptr<TransportSender> sender2 =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSender2ForTestEnqueueSendDirectRequest
                (codec));


        // thread not right
        codec.enqueueSendRequest(sender, PVA_MESSAGE_HEADER_SIZE);


        testOk(1 == codec._scheduleSendCount,
               "%s: 1 == codec._scheduleSendCount", CURRENT_FUNCTION);
        testOk(0 == codec._receivedControlMessages.size(),
               "%s: 0 == codec._receivedControlMessages.size()",
               CURRENT_FUNCTION);
        testOk(0 == codec._receivedAppMessages.size(),
               "%s: 0 == codec._receivedAppMessages.size()",
               CURRENT_FUNCTION);

        codec.setSenderThread();

        // not empty queue
        codec.enqueueSendRequest(sender2, PVA_MESSAGE_HEADER_SIZE);

        testOk(2 == codec._scheduleSendCount,
               "%s: 2 == codec._scheduleSendCount", CURRENT_FUNCTION);
        testOk(0 == codec._receivedControlMessages.size(),
               "%s: 0 == codec._receivedControlMessages.size()",
               CURRENT_FUNCTION);
        testOk(0 == codec._receivedAppMessages.size(),
               "%s: 0 == codec._receivedAppMessages.size()",
               CURRENT_FUNCTION);

        // send will be triggered after last
        //was processed
        testOk(0 == codec._sendCompletedCount,
               "%s: 0 == codec._sendCompletedCount", CURRENT_FUNCTION);
        testOk1(!codec.sendQueueEmpty());

        codec.breakSender();
        try {
            codec.processSendQueue();
        } catch(sender_break&) {}

        testOk(1 == codec._sendCompletedCount,
               "%s: 1 == codec._sendCompletedCount", CURRENT_FUNCTION);
        testOk1(codec.sendQueueEmpty());

        codec.transferToReadBuffer();

        codec.processRead();


        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 1,
               "%s: codec._receivedControlMessages.size() == 1 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

        // app, no payload
        PVAMessage header = codec._receivedAppMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x00),
               "%s: header._flags == 0x(0|8)0", CURRENT_FUNCTION);
        testOk(header._command == 0x20,
               "%s: header._command == 0x20", CURRENT_FUNCTION);
        testOk(header._payloadSize == 0x00000000,
               "%s: header._payloadSize == 0x00000000", CURRENT_FUNCTION);


        // control
        header = codec._receivedControlMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x01),
               "%s: header._flags == 0x(0|8)1", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0xEE,
               "%s: header._command == 0xEE", CURRENT_FUNCTION);
        testOk(header._payloadSize == (int32_t)0xDDCCBBAA,
               "%s: header._payloadSize == 0xDDCCBBAA", CURRENT_FUNCTION);


        testOk(0 == codec.getSendBuffer()->getPosition(),
               "%s: 0 == codec.getSendBuffer()->getPosition()",
               CURRENT_FUNCTION);

        testOk1(codec.sendQueueEmpty());

        testDiag("%u %u", (unsigned)codec._scheduleSendCount,
                 (unsigned)codec._sendCompletedCount);
        testOk1(3 == codec._scheduleSendCount);
        testOk1(1 == codec._sendCompletedCount);

        // now queue is empty and thread is right
        codec.enqueueSendRequest(sender2, PVA_MESSAGE_HEADER_SIZE);

        testOk((std::size_t)PVA_MESSAGE_HEADER_SIZE ==
               codec.getSendBuffer()->getPosition(),
               "%s: PVA_MESSAGE_HEADER_SIZE == "
               "codec.getSendBuffer()->getPosition()",
               CURRENT_FUNCTION);

        testDiag("%u %u", (unsigned)codec._scheduleSendCount,
                 (unsigned)codec._sendCompletedCount);
        testOk1(4 == codec._scheduleSendCount);
        testOk1(1 == codec._sendCompletedCount);

        codec.breakSender();

        try {
            codec.processWrite();
        } catch(sender_break&) {}

        testOk(2 == codec._sendCompletedCount,
               "%s: 2 == codec._sendCompletedCount", CURRENT_FUNCTION);


        codec.transferToReadBuffer();
        codec.processRead();

        testOk(codec._receivedControlMessages.size() == 2,
               "%s: codec._receivedControlMessages.size() == 2 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

        header = codec._receivedControlMessages[1];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x01),
               "%s: header._flags == 0x(0|8)1", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0xEE,
               "%s: header._command == 0xEE", CURRENT_FUNCTION);
        testOk(header._payloadSize == (int32_t)0xDDCCBBAA,
               "%s: header._payloadSize == 0xDDCCBBAA", CURRENT_FUNCTION);
    }



    class TransportSenderForTestSendPerPartes:
        public TransportSender {
    public:

        TransportSenderForTestSendPerPartes(
            TestCodec & codec, std::size_t bytesToSend):
            _codec(codec), _bytesToSent(bytesToSend) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.startMessage((int8_t)0x12, _bytesToSent);

            for (std::size_t i = 0; i < _bytesToSent; i++)
                _codec.getSendBuffer()->put((int8_t)i);

            _codec.endMessage();
        }

    private:
        TestCodec &_codec;
        std::size_t _bytesToSent;
    };


    void testSendPerPartes()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        std::size_t bytesToSent =
            DEFAULT_BUFFER_SIZE - 2*PVA_MESSAGE_HEADER_SIZE;

        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSenderForTestSendPerPartes(
                    codec, bytesToSent));

        codec._readPayload = true;

        // process
        codec.enqueueSendRequest(sender);
        codec.breakSender();
        try {
            codec.processSendQueue();
        } catch(sender_break&) {}

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);


        // app
        PVAMessage header = codec._receivedAppMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)0x80,
               "%s: header._flags == 0x80", CURRENT_FUNCTION);
        testOk(header._command == (int8_t)0x12,
               "%s: header._command == 0x12", CURRENT_FUNCTION);
        testOk(header._payloadSize == (int32_t)bytesToSent,
               "%s: header._payloadSize == bytesToSent",
               CURRENT_FUNCTION);


        testOk(header._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        header._payload->flip();

        testOk(bytesToSent == header._payload->getLimit(),
               "%s: bytesToSent == header._payload->getLimit()",
               CURRENT_FUNCTION);

        for (int32_t i = 0; i < header._payloadSize; i++) {
            if ((int8_t)i != header._payload->getByte()) {
                testFail("%s: (int8_t)%d == header._payload->getByte()",
                         CURRENT_FUNCTION, (int)i);
            }
        }
    }


    class TransportSenderForTestSendException:
        public TransportSender {
    public:

        TransportSenderForTestSendException(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.putControlMessage((int8_t)0x01, 0x00112233);
        }

    private:
        TestCodec &_codec;
    };


    void testSendException()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSenderForTestSendException(codec));

        codec._throwExceptionOnSend = true;

        // process
        codec.enqueueSendRequest(sender);
        codec.breakSender();
        try
        {
            codec.processSendQueue();
            testFail("%s: ConnectionClosedException expected",
                     CURRENT_FUNCTION);
        } catch (connection_closed_exception &) {
            // OK
        }

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 1,
               "%s: codec._closedCount == 1", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);
    }


    class TransportSenderForTestSendHugeMessagePartes:
        public TransportSender {
    public:

        TransportSenderForTestSendHugeMessagePartes(
            TestCodec & codec, std::size_t bytesToSend):
            _codec(codec), _bytesToSent(bytesToSend) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.startMessage((int8_t)0x12, 0);
            std::size_t toSend = _bytesToSent;
            int32_t c = 0;
            while (toSend > 0)
            {
                std::size_t sendNow = std::min<std::size_t>(toSend,
                                      AbstractCodec::MAX_ENSURE_BUFFER_SIZE);

                _codec.ensureBuffer(sendNow);
                for (std::size_t i = 0; i < sendNow; i++)
                    _codec.getSendBuffer()->put((int8_t)(c++));
                toSend -= sendNow;
            }
            _codec.endMessage();
        }

    private:
        TestCodec &_codec;
        std::size_t _bytesToSent;
    };


    class WritePollOneCallbackForTestSendHugeMessagePartes:
        public WritePollOneCallback {
    public:

        WritePollOneCallbackForTestSendHugeMessagePartes(
            TestCodec &codec): _codec(codec) {}

        void writePollOne() {
            testDiag("In %s", CURRENT_FUNCTION);
            _codec.processWrite();	// this should return immediately

            // now we fake reading
            _codec._writeBuffer.flip();

            // in this test we made sure readBuffer is big enough
            while(_codec._writeBuffer.getRemaining() > 0)
            {
                _codec._readBuffer->putByte(
                    _codec._writeBuffer.getByte());
            }

            _codec._writeBuffer.clear();
        }
    private:
        TestCodec & _codec;
    };


    void testSendHugeMessagePartes()
    {

        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        std::size_t bytesToSent = 10*DEFAULT_BUFFER_SIZE+1;
        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        codec._readPayload = true;
        codec._readBuffer.reset(
            new ByteBuffer(11*DEFAULT_BUFFER_SIZE));

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSenderForTestSendHugeMessagePartes(
                    codec, bytesToSent));

        codec._writePollOneCallback.reset(new WritePollOneCallbackForTestSendHugeMessagePartes
                                          (codec));


        // process
        codec.enqueueSendRequest(sender);
        codec.breakSender();
        try {
            codec.processSendQueue();
        } catch(sender_break&) {
            testDiag("sender_break");
        }

        codec.addToReadBuffer();

        codec._forcePayloadRead = bytesToSent;

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        testOk(codec._closedCount == 0,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 1,
               "%s: codec._receivedAppMessages.size() == 1",
               CURRENT_FUNCTION);

        // app
        PVAMessage header = codec._receivedAppMessages[0];

        testOk(header._version == PVA_VERSION,
               "%s: header._version == PVA_VERSION", CURRENT_FUNCTION);
        testOk(header._flags == (int8_t)((EPICS_BYTE_ORDER == EPICS_ENDIAN_BIG ? 0x80 : 0x00) | 0x10),
               "%s: header._flags == (int8_t)(0x(0|8)0 | 0x10)",
               CURRENT_FUNCTION);
        testOk(header._command == 0x12,
               "%s: header._command == 0x12", CURRENT_FUNCTION);

        testOk(header._payload.get() != 0,
               "%s: msg._payload.get() != 0", CURRENT_FUNCTION);

        header._payload->flip();

        testOk(bytesToSent == header._payload->getLimit(),
               "%s: bytesToSent == header._payload->getLimit()",
               CURRENT_FUNCTION);


        for (int32_t i = 0; i < header._payloadSize; i++) {
            if ((int8_t)i != header._payload->getByte()) {
                testFail("%s: (int8_t)%d == header._payload->getByte()",
                         CURRENT_FUNCTION, (int)i);
            }
        }

    }


    void testRecipient()
    {
        // nothing to test, depends on implementation
    }


    class TransportSenderForTestClearSendQueue:
        public TransportSender {
    public:

        TransportSenderForTestClearSendQueue(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.startMessage((int8_t)0x20, 0x00000000);
            _codec.endMessage();
        }

    private:
        TestCodec &_codec;
    };


    class TransportSender2ForTestClearSendQueue:
        public TransportSender {
    public:

        TransportSender2ForTestClearSendQueue(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.putControlMessage((int8_t)0xEE, 0xDDCCBBAA);
        }

    private:
        TestCodec &_codec;
    };

    void testInvalidArguments()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        TestCodec codec(DEFAULT_BUFFER_SIZE,
                        DEFAULT_BUFFER_SIZE);

        try
        {
            codec.ensureBuffer(MAX_TCP_RECV + AbstractCodec::MAX_ENSURE_DATA_BUFFER_SIZE+1);
            testFail("%s: too big size accepted",
                     CURRENT_FUNCTION);
        } catch (std::exception &) {
            // OK
        }

        try
        {
            codec.ensureData(AbstractCodec::MAX_ENSURE_DATA_SIZE+1);
            testFail("%s: too big size accepted", CURRENT_FUNCTION);
        } catch (std::exception &) {
            // OK
        }
    }


    void testDefaultModes()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);
        testOk(NORMAL== codec.getReadMode(),
               "%s: NORMAL== codec.getReadMode()", CURRENT_FUNCTION);
        testOk(PROCESS_SEND_QUEUE == codec.getWriteMode(),
               "%s: PROCESS_SEND_QUEUE == codec.getWriteMode()",
               CURRENT_FUNCTION);
    }


    class TransportSenderForTestEnqueueSendRequestExceptionThrown:
        public TransportSender {
    public:

        TransportSenderForTestEnqueueSendRequestExceptionThrown(
            TestCodec & /*codec*/) { /*: _codec(codec)*/ }

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            // after connection_closed_exception is thrown codec is no longer valid,
            // however we want to do some tests and in order for memory checkers not to complain
            // the following step is needed
            memset((void*)buffer->getBuffer(), 0, buffer->getSize());

            throw connection_closed_exception(
                "expected test exception");
        }

    private:
        //TestCodec &_codec;
    };


    class TransportSender2ForTestEnqueueSendRequestExceptionThrown:
        public TransportSender {
    public:

        TransportSender2ForTestEnqueueSendRequestExceptionThrown(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.putControlMessage((int8_t)0xEE, 0xDDCCBBAA);
        }

    private:
        TestCodec &_codec;
    };


    void testEnqueueSendRequestExceptionThrown()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        TestCodec codec(DEFAULT_BUFFER_SIZE,DEFAULT_BUFFER_SIZE);

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new
                TransportSenderForTestEnqueueSendRequestExceptionThrown
                (codec));

        std::tr1::shared_ptr<TransportSender> sender2 =
            std::tr1::shared_ptr<TransportSender>(new
                    TransportSender2ForTestEnqueueSendRequestExceptionThrown(
                        codec));


        // process
        codec.enqueueSendRequest(sender);
        codec.enqueueSendRequest(sender2);

        try
        {
            codec.processSendQueue();
            testFail("%s: ConnectionClosedException expected",
                     CURRENT_FUNCTION);

        } catch (connection_closed_exception &) {
            // OK
        }

        codec.transferToReadBuffer();

        codec.processRead();

        testOk(codec._invalidDataStreamCount == 0,
               "%s: codec._invalidDataStreamCount == 0",
               CURRENT_FUNCTION);
        // _closedCount is not incremented since CCE exception is being thrown manually
        // w/o calling close()
        testOk(codec._closedCount == 0 /*1*/,
               "%s: codec._closedCount == 0", CURRENT_FUNCTION);
        testOk(codec._receivedControlMessages.size() == 0,
               "%s: codec._receivedControlMessages.size() == 0 ",
               CURRENT_FUNCTION);
        testOk(codec._receivedAppMessages.size() == 0,
               "%s: codec._receivedAppMessages.size() == 0",
               CURRENT_FUNCTION);
    }


    class TransportSenderForTestBlockingProcessQueueTest:
        public TransportSender {
    public:

        TransportSenderForTestBlockingProcessQueueTest(
            TestCodec & codec): _codec(codec) {}

        void send(epics::pvData::ByteBuffer* buffer,
                  TransportSendControl* control)
        {
            _codec.putControlMessage((int8_t)0x01, 0x00112233);
            _codec.flush(true);
        }

    private:
        TestCodec &_codec;
    };


    class ValueHolder : public Runnable {
    public:
        ValueHolder(TestCodec &testCodec):
            _testCodec(testCodec) {}

        TestCodec &_testCodec;
        Event waiter;

        virtual void run() {
            waiter.signal();
            try {
                _testCodec.processSendQueue();
            } catch(sender_break&) {}
        }
    };


    void testBlockingProcessQueueTest()
    {
        testDiag("BEGIN TEST %s:", CURRENT_FUNCTION);

        TestCodec codec(DEFAULT_BUFFER_SIZE,
                        DEFAULT_BUFFER_SIZE, true);

        std::tr1::shared_ptr<TransportSender> sender =
            std::tr1::shared_ptr<TransportSender>(
                new TransportSenderForTestBlockingProcessQueueTest(codec));

        ValueHolder valueHolder(codec);
        Event done;

        epics::pvData::Thread thr(epics::pvData::Thread::Config(&valueHolder)
                                  .name("testBlockingProcessQueueTest-processThread"));

        valueHolder.waiter.wait();

        // let's put something into it

        codec.enqueueSendRequest(sender);
        codec.enqueueSendRequest(std::tr1::shared_ptr<TransportSender>(new TransportSenderSignal(done)));

        testDiag("Waiting for work");
        done.wait();

        testOk((std::size_t)PVA_MESSAGE_HEADER_SIZE ==
               codec._writeBuffer.getPosition(),
               "%s: PVA_MESSAGE_HEADER_SIZE == "
               "codec._writeBuffer.getPosition()  (%u)",
               CURRENT_FUNCTION,
               (unsigned)codec._writeBuffer.getPosition());

        codec.breakSender();

        thr.exitWait();
    }

private:

    AtomicValue<bool> _processTreadExited;
};
}
}


using namespace epics::pvAccess;

MAIN(testCodec)
{
    CodecTest codecTest;
    return codecTest.runAllTest();
}
