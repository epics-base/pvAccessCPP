/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <algorithm>

#define epicsExportSharedSymbols
#include <pv/beaconEmitter.h>
#include <pv/serializationHelper.h>
#include <pv/logger.h>
#include <pv/serverContextImpl.h>

using namespace std;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

const float BeaconEmitter::EPICS_PVA_MIN_BEACON_PERIOD = 1.0;
const float BeaconEmitter::EPICS_PVA_MIN_BEACON_COUNT_LIMIT = 3.0;

//BeaconEmitter::BeaconEmitter(Transport::shared_pointer const & transport, ServerContextImpl::shared_pointer const & context) :
BeaconEmitter::BeaconEmitter(std::string const & protocol,
                             Transport::shared_pointer const & transport, std::tr1::shared_ptr<ServerContextImpl>& context) :
    _protocol(protocol),
    _transport(transport),
    _beaconSequenceID(0),
    _guid(context->getGUID()),
    _fastBeaconPeriod(std::max(context->getBeaconPeriod(), EPICS_PVA_MIN_BEACON_PERIOD)),
    _slowBeaconPeriod(std::max(180.0, _fastBeaconPeriod)), // TODO configurable
    _beaconCountLimit((int16)std::max(10.0f, EPICS_PVA_MIN_BEACON_COUNT_LIMIT)), // TODO configurable
    _serverAddress(*(context->getServerInetAddress())),
    _serverPort(context->getServerPort()),
    _serverStatusProvider(context->getBeaconServerStatusProvider()),
    _timer(context->getTimer())
{
}

BeaconEmitter::~BeaconEmitter()
{
    // shared_from_this is not yet allows in destructor
    // be sure to call destroy() first !!!
    // destroy();
}

void BeaconEmitter::send(ByteBuffer* buffer, TransportSendControl* control)
{
    // get server status
    PVField::shared_pointer serverStatus;
    if(_serverStatusProvider.get())
    {
        try
        {
            serverStatus = _serverStatusProvider->getServerStatusData();
        }
        catch (...) {
            // we have to proctect internal code from external implementation...
            LOG(logLevelDebug, "BeaconServerStatusProvider implementation thrown an exception.");
        }
    }

    // send beacon
    control->startMessage((int8)CMD_BEACON, 12+2+2+16+2);

    buffer->put(_guid.value, 0, sizeof(_guid.value));

    // TODO qos/flags (e.g. multicast/unicast)
    buffer->putByte(0);

    buffer->putByte(_beaconSequenceID);

    // TODO for now fixed changeCount
    buffer->putShort(0);

    // NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
    encodeAsIPv6Address(buffer, &_serverAddress);
    buffer->putShort((int16)_serverPort);

    SerializeHelper::serializeString(_protocol, buffer, control);

    if (serverStatus)
    {
        // introspection interface + data
        serverStatus->getField()->serialize(buffer, control);
        serverStatus->serialize(buffer, control);
    }
    else
    {
        SerializationHelper::serializeNullField(buffer, control);
    }
    control->flush(true);

    // increment beacon sequence ID
    _beaconSequenceID++;

    reschedule();
}

void BeaconEmitter::timerStopped()
{
    //noop
}

void BeaconEmitter::destroy()
{
    Timer::shared_pointer timer(_timer.lock());
    if(timer)
        timer->cancel(shared_from_this());
}

void BeaconEmitter::start()
{
    Timer::shared_pointer timer(_timer.lock());
    if(timer)
        timer->scheduleAfterDelay(shared_from_this(), 0.0);
}

void BeaconEmitter::reschedule()
{
    const double period = (_beaconSequenceID >= _beaconCountLimit) ? _slowBeaconPeriod : _fastBeaconPeriod;
    if (period > 0)
    {
        Timer::shared_pointer timer(_timer.lock());
        if(timer)
            timer->scheduleAfterDelay(shared_from_this(), period);
    }
}

void BeaconEmitter::callback()
{
    _transport->enqueueSendRequest(shared_from_this());
}

}
}

