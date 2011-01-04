/*
 * beaconHandler.h
 */

#ifndef BEACONHANDLER_H
#define BEACONHANDLER_H

#include "remote.h"
#include "inetAddressUtil.h"
#include "pvAccess.h"

#include <timeStamp.h>
#include <osiSock.h>
#include <lock.h>

#include <iostream>

using namespace epics::pvData;

namespace epics { namespace pvAccess {
	//TODO delete this
	class ClientContextImpl;
	/**
	 * BeaconHandler
	 */
	class BeaconHandler
	{
	public:
		/**
		 * Constructor.
		 * @param transport	transport to be used to send beacons.
		 * @param context CA context.
		 */
		BeaconHandler(const ClientContextImpl* context, const osiSockAddr* responseFrom);
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
		void beaconNotify(osiSockAddr* from, int8 remoteTransportRevision,
							 int64 timestamp, TimeStamp* startupTime, int32 sequentalID,
							 PVFieldPtr data);
	private:
		/**
		 * Context instance.
		 */
		const ClientContextImpl* _context;
		/**
		 * Remote address.
		 */
		const osiSockAddr* _responseFrom;
		/**
		 * Server startup timestamp.
		 */
		TimeStamp* _serverStartupTime;
		/**
		 * Mutex
		 */
		Mutex _mutex;
		/**
		 * Update beacon.
		 * @param remoteTransportRevision encoded (major, minor) revision.
		 * @param timestamp time when beacon was received.
		 * @param sequentalID sequential ID.
		 * @return network change (server restarted) detected.
		 */
		bool updateBeacon(int8 remoteTransportRevision, int64 timestamp,
												  TimeStamp* startupTime, int32 sequentalID);
		/**
		 * Notify transport about beacon arrival.
		 */
		void beaconArrivalNotify();
		/**
		 * Changed transport (server restarted) notify.
		 */
		void changedTransport();
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
