/*
 * beaconHandler.cpp
 */

#include "beaconHandler.h"

using namespace std;

namespace epics { namespace pvAccess {

BeaconHandler::BeaconHandler(const ClientContextImpl* context, const osiSockAddr* responseFrom): _context(context), _responseFrom(responseFrom), _mutex(Mutex())
{

}

BeaconHandler::BeaconHandler(const osiSockAddr* responseFrom):  _responseFrom(responseFrom), _mutex(Mutex())
{

}

BeaconHandler::~BeaconHandler()
{

}

void BeaconHandler::beaconNotify(osiSockAddr* from, int8 remoteTransportRevision,
							 int64 timestamp, TimeStamp* startupTime, int32 sequentalID,
							 PVFieldPtr data)
{
	bool networkChanged = updateBeacon(remoteTransportRevision, timestamp, startupTime, sequentalID);
	if(networkChanged)
	{
		changedTransport();
	}
}

bool BeaconHandler::updateBeacon(int8 remoteTransportRevision, int64 timestamp,
												  TimeStamp* startupTime, int32 sequentalID)
{
	Lock guard(&_mutex);
	// first beacon notification check
	if (_serverStartupTime == NULL)
	{
		_serverStartupTime = startupTime;

		// new server up..
		//TODO
		//_context->beaconAnomalyNotify();

		// notify corresponding transport(s)
		beaconArrivalNotify();

		return false;
	}

	bool networkChange = !(*_serverStartupTime == *startupTime);
	if (networkChange)
	{
		//TODO
		//_context->beaconAnomalyNotify();
	}
	else
	{
		beaconArrivalNotify();
	}

	return networkChange;
}

void BeaconHandler::beaconArrivalNotify()
{
	int32 size;
	//TODO TCP name must be get from somewhere not hardcoded
	//TODO
	Transport** transports  = NULL;//_context->getTransportRegistry().get("TCP", _responseFrom, size);
	if (transports == NULL)
	{
		return;
	}

	// notify all
	for (int i = 0; i < size; i++)
	{
		transports[i]->aliveNotification();
	}
	delete transports;
}

void BeaconHandler::changedTransport()
{
	int32 size;
	//TODO TCP name must be get from somewhere not hardcoded
	//TODO
	Transport** transports = NULL;//_context->getTransportRegistry().get("TCP", _responseFrom, size);
	if (transports == NULL)
	{
		return;
	}

	// notify all
	for (int i = 0; i < size; i++)
	{
		transports[i]->changedTransport();
	}
	delete transports;
}

}}

