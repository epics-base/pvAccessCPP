/*
 * beaconHandler.cpp
 */

#include "beaconHandler.h"
#include "transportRegistry.h"

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;

namespace epics { namespace pvAccess {

BeaconHandler::BeaconHandler(Context::shared_pointer context,
                             const osiSockAddr* responseFrom)
    :_context(Context::weak_pointer(context))
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
		_context.lock()->beaconAnomalyNotify();

		// notify corresponding transport(s)
		beaconArrivalNotify();

		return false;
	}

	bool networkChange = !(_serverStartupTime == *startupTime);
	if (networkChange)
	{
		_context.lock()->beaconAnomalyNotify();
	}
	else
	{
		beaconArrivalNotify();
	}

	return networkChange;
}

void BeaconHandler::beaconArrivalNotify()
{
    auto_ptr<TransportRegistry::transportVector_t> transports =
        _context.lock()->getTransportRegistry()->get("TCP", &_responseFrom);
	if (!transports.get())
		return;

	// notify all
	for (TransportRegistry::transportVector_t::iterator iter = transports->begin();
         iter != transports->end();
         iter++)
	{
		(*iter)->aliveNotification();
	}
}

void BeaconHandler::changedTransport()
{
    auto_ptr<TransportRegistry::transportVector_t> transports =
        _context.lock()->getTransportRegistry()->get("TCP", &_responseFrom);
	if (!transports.get())
		return;
    
	// notify all
	for (TransportRegistry::transportVector_t::iterator iter = transports->begin();
         iter != transports->end();
         iter++)
	{
		(*iter)->changedTransport();
	}
}

}}

