/*
 * beaconHandler.cpp
 */

#include "beaconHandler.h"

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;

namespace epics { namespace pvAccess {

BeaconHandler::BeaconHandler(ClientContextImpl* context,
                             const osiSockAddr* responseFrom)
    :_context(context)
    ,_responseFrom(*responseFrom)
    ,_mutex()
    ,_serverStartupTime(0)
{

}

BeaconHandler::BeaconHandler(const osiSockAddr* responseFrom)
    :_responseFrom(*responseFrom)
    ,_mutex()
    , _serverStartupTime(0)
{

}

BeaconHandler::~BeaconHandler()
{

}

void BeaconHandler::beaconNotify(osiSockAddr* from, int8 remoteTransportRevision,
							 TimeStamp* timestamp, TimeStamp* startupTime, int16 sequentalID,
							 PVFieldPtr data)
{
	bool networkChanged = updateBeacon(remoteTransportRevision, timestamp, startupTime, sequentalID);
	if(networkChanged)
	{
		changedTransport();
	}
}

bool BeaconHandler::updateBeacon(int8 remoteTransportRevision, TimeStamp* timestamp,
			                     TimeStamp* startupTime, int16 sequentalID)
{
	Lock guard(_mutex);
	// first beacon notification check
	if (_serverStartupTime.getSecondsPastEpoch() == 0)
	{
		_serverStartupTime = *startupTime;

		// new server up..
		_context->beaconAnomalyNotify();

		// notify corresponding transport(s)
		beaconArrivalNotify();

		return false;
	}

	bool networkChange = !(_serverStartupTime == *startupTime);
	if (networkChange)
	{
		_context->beaconAnomalyNotify();
	}
	else
	{
		beaconArrivalNotify();
	}

	return networkChange;
}

void BeaconHandler::beaconArrivalNotify()
{
	int32 size = 0;
	//TODO TCP name must be get from somewhere not hardcoded
	Transport** transports  = _context->getTransportRegistry()->get("TCP", &_responseFrom, size);
	if (transports == NULL)
	{
		return;
	}

	// notify all
	for (int i = 0; i < size; i++)
	{
		transports[i]->aliveNotification();
	}
	delete[] transports;
}

void BeaconHandler::changedTransport()
{
	int32 size = 0;
	//TODO TCP name must be get from somewhere not hardcoded
	Transport** transports = _context->getTransportRegistry()->get("TCP", &_responseFrom, size);
	if (transports == NULL)
	{
		return;
	}

	// notify all
	for (int i = 0; i < size; i++)
	{
		transports[i]->changedTransport();
	}
	delete[] transports;
}

}}

