/*
 * beaconEmitter.h
 */

#ifndef BEACONEMITTER_H
#define BEACONEMITTER_H

#include "timer.h"
#include "remote.h"
#include "beaconServerStatusProvider.h"
#include "inetAddressUtil.h"
#include "introspectionRegistry.h"
#include "serverContext.h"


#include <timeStamp.h>
#include <osiSock.h>
#include <errlog.h>

#include <algorithm>
#include <iostream>

using namespace epics::pvData;

namespace epics { namespace pvAccess {

	class ServerContextImpl;

	/**
	 * BeaconEmitter
	 *
	 * @author gjansa
	 */
	class BeaconEmitter: public TransportSender, public TimerCallback
	{
	public:
		/**
		 * Constructor.
		 * @param transport	transport to be used to send beacons.
		 * @param context CA context.
		 */
		BeaconEmitter(Transport* transport, ServerContextImpl* context);
		/**
		 * Test Constructor (ohne context)
		 * @param transport	transport to be used to send beacons.
		 */
		BeaconEmitter(Transport* transport,const osiSockAddr* serverAddress);
		virtual ~BeaconEmitter();

		/*
		 * @see TransportSender#lock()
		 */
		void lock();
		/*
		 * @see TransportSender#unlock()
		 */
		void unlock();
		
		void acquire();
		void release();

		void send(ByteBuffer* buffer, TransportSendControl* control);
		/**
		 * noop
		 */
		void timerStopped();
		/**
		 * noop
		 */
		void destroy();
		/**
		 * Start emitting.
		 */
		void start();
		/**
		 * Reschedule timer.
		 */
		void reschedule();
		/**
		 * Timer callback.
		 */
		void callback();

	private:
		/**
		 * Minimal (initial) CA beacon period (in seconds).
		 */
		static const float EPICS_CA_MIN_BEACON_PERIOD;

		/**
		 * Minimal CA beacon count limit.
		 */
		static const float EPICS_CA_MIN_BEACON_COUNT_LIMIT;

		/**
		 * Timer.
		 */
		Timer* _timer;

		/**
		 * Transport.
		 */
		Transport* _transport;

		/**
		 * Beacon sequence ID.
		 */
		int16 _beaconSequenceID;

		/**
		 * Startup timestamp (when clients detect a change, they will consider server restarted).
		 */
		TimeStamp* _startupTime;

		/**
		 * Fast (at startup) beacon period (in sec).
		 */
		double _fastBeaconPeriod;

		/**
		 * Slow (after beaconCountLimit is reached) beacon period (in sec).
		 */
		double _slowBeaconPeriod;

		/**
		 * Limit on number of beacons issued.
		 */
		int16 _beaconCountLimit;

		/**
		 * Server address.
		 */
		const osiSockAddr* _serverAddress;

		/**
		 * Server port.
		 */
		int32 _serverPort;

		/**
		 * Server status provider implementation (optional).
		 */
		BeaconServerStatusProvider* _serverStatusProvider;

		/**
		 * Timer task node.
		 */
		TimerNode* _timerNode;
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
