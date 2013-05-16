/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef BEACONHANDLER_H
#define BEACONHANDLER_H

#include <pv/remote.h>
#include <pv/pvAccess.h>

#include <pv/timeStamp.h>
#include <osiSock.h>
#include <pv/lock.h>

namespace epics {
namespace pvAccess {

	/**
	 * BeaconHandler
	 */
	class BeaconHandler
	{
	public:
		POINTER_DEFINITIONS(BeaconHandler);

		/**
		 * Constructor.
		 * @param transport	transport to be used to send beacons.
		 * @param context PVA context.
		 */
		BeaconHandler(Context::shared_pointer const & context, const osiSockAddr* responseFrom);
		/**
		 * Test Constructor (for testing)
		 * @param transport	transport to be used to send beacons.
		 */
		BeaconHandler(const osiSockAddr* responseFrom);
		virtual ~BeaconHandler();
		/**
		 * Update beacon period and do analitical checks (server restared, routing problems, etc.)
		 * @param from who is notifying.
		 * @param remoteTransportRevision encoded (major, minor) revision.
		 * @param timestamp time when beacon was received.
		 * @param startupTime server (reported) startup time.
		 * @param sequentalID sequential ID.
		 * @param data server status data, can be <code>NULL</code>.
		 */
		void beaconNotify(osiSockAddr* from,
		                     epics::pvData::int8 remoteTransportRevision,
							 epics::pvData::TimeStamp* timestamp,
							 epics::pvData::TimeStamp* startupTime,
							 epics::pvData::int16 sequentalID,
							 epics::pvData::PVFieldPtr data);
	private:
		/**
		 * Context instance.
		 */
        Context::weak_pointer _context;
		/**
		 * Remote address.
		 */
		const osiSockAddr _responseFrom;
		/**
		 * Mutex
		 */
		epics::pvData::Mutex _mutex;
		/**
		 * Server startup timestamp.
		 */
		epics::pvData::TimeStamp _serverStartupTime;
		/**
		 * Update beacon.
		 * @param remoteTransportRevision encoded (major, minor) revision.
		 * @param timestamp time when beacon was received.
		 * @param sequentalID sequential ID.
		 * @return network change (server restarted) detected.
		 */
		bool updateBeacon(epics::pvData::int8 remoteTransportRevision,
		                  epics::pvData::TimeStamp* timestamp,
					      epics::pvData::TimeStamp* startupTime,
					      epics::pvData::int16 sequentalID);
		/**
		 * Changed transport (server restarted) notify.
		 */
		void changedTransport();
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
