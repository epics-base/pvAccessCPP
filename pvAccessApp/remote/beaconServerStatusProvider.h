/*
 * beaconServerStatusProvider.h
 */

#ifndef BEACONSERVERSTATUSPROVIDER_H
#define BEACONSERVERSTATUSPROVIDER_H

#include "pvData.h"

using namespace epics::pvData;

namespace epics { namespace pvAccess {

	class ServerContext;
	/**
	 * BeaconServerStatusProvider
	 */
	class BeaconServerStatusProvider
	{
	public:
		/**
		 * Constructor.
		 * @param context CA context.
		 */
		BeaconServerStatusProvider(ServerContext* context);
		/**
		 * Test Constructor (without context)
		 */
		BeaconServerStatusProvider();
		/**
		 * Destructor.
		 */
		virtual ~BeaconServerStatusProvider();
		/**
		 * Gets server status data.
		 */
		PVFieldPtr getServerStatusData();
	private:
		/**
		 * Initialize
		 */
		void initialize();


	private:
		PVStructurePtr _status;
		ServerContext* _context;
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
