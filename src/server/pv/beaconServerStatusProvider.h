/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef BEACONSERVERSTATUSPROVIDER_H
#define BEACONSERVERSTATUSPROVIDER_H

#ifdef epicsExportSharedSymbols
#   define beaconServerStatusProviderEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pvData.h>
#include <pv/sharedPtr.h>

#ifdef beaconServerStatusProviderEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#   undef beaconServerStatusProviderEpicsExportSharedSymbols
#endif

#include <shareLib.h>

namespace epics {
namespace pvAccess {

class ServerContext;

/**
 * BeaconServerStatusProvider
 */
class epicsShareClass BeaconServerStatusProvider
{
public:
    typedef std::tr1::shared_ptr<BeaconServerStatusProvider> shared_pointer;
    typedef std::tr1::shared_ptr<const BeaconServerStatusProvider> const_shared_pointer;

    virtual ~BeaconServerStatusProvider() {};

    /**
     * Gets server status data.
     */
    virtual epics::pvData::PVField::shared_pointer getServerStatusData() = 0;
};

/**
 * DefaultBeaconServerStatusProvider
 */
class epicsShareClass DefaultBeaconServerStatusProvider : public BeaconServerStatusProvider
{
public:
    /**
     * Constructor.
     * @param context PVA context.
     */
//              DefaultBeaconServerStatusProvider(ServerContext::shared_pointer const & context);
    DefaultBeaconServerStatusProvider(std::tr1::shared_ptr<ServerContext> const & context);
    /**
     * Destructor.
     */
    virtual ~DefaultBeaconServerStatusProvider();

    virtual epics::pvData::PVField::shared_pointer getServerStatusData();

private:
    epics::pvData::PVStructure::shared_pointer _status;
};

}
}

#endif  /* BEACONSERVERSTATUSPROVIDER_H */
