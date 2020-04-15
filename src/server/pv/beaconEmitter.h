/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef BEACONEMITTER_H
#define BEACONEMITTER_H

#ifdef epicsExportSharedSymbols
#   define beaconEmitterEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <osiSock.h>

#include <pv/timer.h>
#include <pv/timeStamp.h>
#include <pv/sharedPtr.h>

#ifdef beaconEmitterEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#       undef beaconEmitterEpicsExportSharedSymbols
#endif

#include <pv/remote.h>
#include <pv/beaconServerStatusProvider.h>
//#include <pv/serverContext.h>

namespace epics {
namespace pvAccess {

class ServerContextImpl;

/**
 * BeaconEmitter
 *
 * @author gjansa
 */
class BeaconEmitter:
    public TransportSender,
    public epics::pvData::TimerCallback,
    public std::tr1::enable_shared_from_this<BeaconEmitter>
{
public:
    typedef std::tr1::shared_ptr<BeaconEmitter> shared_pointer;
    typedef std::tr1::shared_ptr<const BeaconEmitter> const_shared_pointer;

    /**
     * Constructor.
     * @param protocol a protocol (transport) name to report.
     * @param transport transport to be used to send beacons.
     * @param context PVA context.
     */
//              BeaconEmitter(std::sting const & protocol,
//          Transport::shared_pointer const & transport, ServerContextImpl::shared_pointer const & context);
    BeaconEmitter(std::string const & protocol,
                  Transport::shared_pointer const & transport, std::tr1::shared_ptr<ServerContextImpl>& context);

    virtual ~BeaconEmitter();

    void send(epics::pvData::ByteBuffer* buffer, TransportSendControl* control);

    void timerStopped();

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

    void destroy();

private:

    /**
     * Minimal (initial) PVA beacon period (in seconds).
     */
    static const float EPICS_PVA_MIN_BEACON_PERIOD;

    /**
     * Minimal PVA beacon count limit.
     */
    static const float EPICS_PVA_MIN_BEACON_COUNT_LIMIT;

    /**
     * Protocol.
     */
    const std::string _protocol;

    /**
     * Transport.
     */
    Transport::shared_pointer _transport;

    /**
     * Beacon sequence ID.
     */
    epics::pvData::int8 _beaconSequenceID;

    /**
     * Server GUID.
     */
    const ServerGUID _guid;

    /**
     * Fast (at startup) beacon period (in sec).
     */
    const double _fastBeaconPeriod;

    /**
     * Slow (after beaconCountLimit is reached) beacon period (in sec).
     */
    const double _slowBeaconPeriod;

    /**
     * Limit on number of beacons issued.
     */
    const epics::pvData::int16 _beaconCountLimit;

    /**
     * Server address.
     */
    const osiSockAddr _serverAddress;

    /**
     * Server port.
     */
    const epics::pvData::int32 _serverPort;

    /**
     * Server status provider implementation (optional).
     */
    BeaconServerStatusProvider::shared_pointer _serverStatusProvider;

    /** Timer is referenced by server context, which also references us.
     *  We will also be queuing ourselves, and be referenced by Timer.
     *  So keep only a weak ref to Timer to avoid possible ref. loop.
     */
    epics::pvData::Timer::weak_pointer _timer;
};

}
}

#endif  /* BEACONEMITTER_H */
