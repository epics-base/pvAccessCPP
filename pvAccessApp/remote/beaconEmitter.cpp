/*
 * beaconEmitter.cpp
 */

#include "beaconEmitter.h"

using namespace std;

namespace epics { namespace pvAccess {

const float BeaconEmitter::EPICS_CA_MIN_BEACON_PERIOD = 1.0;
const float BeaconEmitter::EPICS_CA_MIN_BEACON_COUNT_LIMIT = 3.0;

BeaconEmitter::BeaconEmitter(Transport* transport, ServerContext* context): _transport(transport)
{
	if(transport == NULL || context == NULL)
	{
		THROW_BASE_EXCEPTION("null transport or context");
	}

/*	_timer = context->getTimer();
	_logger = context->getLogger();
	_beaconSequenceID = 0;
	_serverAddress = context->getServerInetAddres();
	_serverPort = context->getServerPort();
	_serverStatusProvider = context->getBeaconServerStatusProvider();
	_fastBeaconPeriod = std::max(context->getBeaconPeriod(), EPICS_CA_MIN_BEACON_PERIOD);
	_slowBeaconPeriod = std::max(180.0, _fastBeaconPeriod);	// TODO configurable
	_beaconCountLimit = (int16)std::max(10, EPICS_CA_MIN_BEACON_COUNT_LIMIT);	// TODO configurable
	_startupTime = TimeStampFactory.create(System.currentTimeMillis());
	_timerNode = TimerFactory.createNode(this);*/
}

BeaconEmitter::BeaconEmitter(Transport* transport,const osiSockAddr* serverAddress): _transport(transport)
{
	if(transport == NULL)
	{
		THROW_BASE_EXCEPTION("null transport");
	}

	_timer = new Timer("pvAccess-server timer", lowPriority);
	//_logger = new Loger();
	_beaconSequenceID = 0;
	_serverAddress = serverAddress;
	_serverPort = serverAddress->ia.sin_port;
	_serverStatusProvider = NULL;//new BeaconServerStatusProvider();
	_fastBeaconPeriod = EPICS_CA_MIN_BEACON_PERIOD;
	_slowBeaconPeriod = 180.0;
	_beaconCountLimit = 10;
	_startupTime = new TimeStamp();
	_timerNode = new TimerNode(this);
}

BeaconEmitter::~BeaconEmitter()
{
	if(_timer) delete _timer;
	if(_serverStatusProvider) delete _serverStatusProvider;
	if(_startupTime) delete _startupTime;
	if(_timerNode) delete _timerNode;
}

void BeaconEmitter::lock()
{
	//noop
}

void BeaconEmitter::unlock()
{
	//noop
}

void BeaconEmitter::acquire()
{
	//noop
}

void BeaconEmitter::release()
{
	//noop
}

void BeaconEmitter::send(ByteBuffer* buffer, TransportSendControl* control)
{
	// get server status
	PVFieldPtr serverStatus = NULL;
	if(_serverStatusProvider != NULL)
	{
		try
		{
			serverStatus = _serverStatusProvider->getServerStatusData();
		}
		catch (...) {
			// we have to proctect internal code from external implementation...
			//logger->log(Level.WARNING, "BeaconServerStatusProvider implementation thrown an exception.", th);
		}
	}

	// send beacon
	control->startMessage((int8)0, (sizeof(int16)+2*sizeof(int32)+128+sizeof(int16))/sizeof(int8));

	buffer->putShort(_beaconSequenceID);
	buffer->putInt((int32)_startupTime->getSecondsPastEpoch());
	buffer->putInt((int32)_startupTime->getNanoSeconds());

	// NOTE: is it possible (very likely) that address is any local address ::ffff:0.0.0.0
	encodeAsIPv6Address(buffer, _serverAddress);
	buffer->putShort((int16)_serverPort);

	if (serverStatus != NULL)
	{
		// introspection interface + data
		IntrospectionRegistry::serializeFull(serverStatus->getField(), buffer, control);
		serverStatus->serialize(buffer, control);
	}
	else
	{
		IntrospectionRegistry::serializeFull(NULL, buffer, control);
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
	_timerNode->cancel();
}

void BeaconEmitter::start()
{
	_timer->scheduleAfterDelay(_timerNode, 0.0);
}

void BeaconEmitter::reschedule()
{
	const double period = (_beaconSequenceID >= _beaconCountLimit) ? _slowBeaconPeriod : _fastBeaconPeriod;
	if (period > 0)
	{
		_timer->scheduleAfterDelay(_timerNode, period);
	}
}

void BeaconEmitter::callback()
{
	_transport->enqueueSendRequest(this);
}

}}

