/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef BEACONHANDLER_H
#define BEACONHANDLER_H

#ifdef epicsExportSharedSymbols
#   define beaconHandlerEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <osiSock.h>

#include <pv/timeStamp.h>
#include <pv/lock.h>

#ifdef beaconHandlerEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef beaconHandlerEpicsExportSharedSymbols
#endif

#include <pv/pvaDefs.h>
#include <pv/remote.h>
#include <pv/pvAccess.h>

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
     */
    BeaconHandler(Context::shared_pointer const & context,
                  const osiSockAddr* responseFrom);

    virtual ~BeaconHandler();

    /**
     * Update beacon period and do analitical checks (server restared, routing problems, etc.)
     * @param from who is notifying.
     * @param remoteTransportRevision encoded (major, minor) revision.
     * @param guid server GUID.
     * @param sequentalID sequential ID.
     * @param changeCount change count.
     * @param data server status data, can be <code>NULL</code>.
     */
    void beaconNotify(osiSockAddr* from,
                      epics::pvData::int8 remoteTransportRevision,
                      epics::pvData::TimeStamp* timestamp,
                      ServerGUID const &guid,
                      epics::pvData::int16 sequentalID,
                      epics::pvData::int16 changeCount,
                      const epics::pvData::PVFieldPtr& data);
private:
    /**
     * Context instance.
     */
    Context::weak_pointer _context;
    /**
     * Remote address.
     */
    /**
     * Mutex
     */
    epics::pvData::Mutex _mutex;
    /**
     * Server GUID.
     */
    ServerGUID _serverGUID;
    /**
     * Server startup timestamp.
     */
    epics::pvData::int16 _serverChangeCount;
    /**
     * First beacon flag.
     */
    bool _first;

    /**
     * Update beacon.
     * @param remoteTransportRevision encoded (major, minor) revision.
     * @param timestamp time when beacon was received.
     * @param guid server GUID.
     * @param sequentalID sequential ID.
     * @param changeCount change count.
     * @return network change (server restarted) detected.
     */
    bool updateBeacon(epics::pvData::int8 remoteTransportRevision,
                      epics::pvData::TimeStamp* timestamp,
                      ServerGUID const &guid,
                      epics::pvData::int16 sequentalID,
                      epics::pvData::int16 changeCount);
};

}
}

#endif  /* INTROSPECTIONREGISTRY_H */
