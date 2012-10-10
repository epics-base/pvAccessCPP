/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/beaconHandler.h>
#include <pv/transportRegistry.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;

namespace epics {
namespace pvAccess {

BeaconHandler::BeaconHandler(Context::shared_pointer const & context,
                             const osiSockAddr* responseFrom) :
    _context(Context::weak_pointer(context)),
    _responseFrom(*responseFrom),
    _mutex(),
    _serverStartupTime(0)
{

}

BeaconHandler::BeaconHandler(const osiSockAddr* responseFrom) :
    _responseFrom(*responseFrom),
    _mutex(),
    _serverStartupTime(0)
{

}

BeaconHandler::~BeaconHandler()
{

}

void BeaconHandler::beaconNotify(osiSockAddr* /*from*/, int8 remoteTransportRevision,
							 TimeStamp* timestamp, TimeStamp* startupTime, int16 sequentalID,
							 PVFieldPtr /*data*/)
{
	bool networkChanged = updateBeacon(remoteTransportRevision, timestamp, startupTime, sequentalID);
	if (networkChanged)
		changedTransport();
}

bool BeaconHandler::updateBeacon(int8 /*remoteTransportRevision*/, TimeStamp* /*timestamp*/,
			                     TimeStamp* startupTime, int16 /*sequentalID*/)
{
	Lock guard(_mutex);
	// first beacon notification check
	if (_serverStartupTime.getSecondsPastEpoch() == 0)
	{
		_serverStartupTime = *startupTime;

		// new server up..
		_context.lock()->newServerDetected();

		return false;
	}

	bool networkChange = !(_serverStartupTime == *startupTime);
	if (networkChange)
	{
		// update startup time
		_serverStartupTime = *startupTime;

		_context.lock()->newServerDetected();

		return true;
	}

	return false;
}

void BeaconHandler::changedTransport()
{
	// TODO why only TCP, actually TCP does not need this
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

