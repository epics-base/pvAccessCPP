/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/serverContext.h>
#include <pv/beaconServerStatusProvider.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

DefaultBeaconServerStatusProvider::DefaultBeaconServerStatusProvider(ServerContext::shared_pointer const & context)
    :_status(getPVDataCreate()->createPVStructure(getFieldCreate()->createFieldBuilder()
             ->add("connections", pvInt)
             ->add("connections", pvInt)
             ->add("allocatedMemory", pvLong)
             ->add("freeMemory", pvLong)
             ->add("threads", pvInt)
             ->add("deadlocks", pvInt)
             ->add("averageSystemLoad", pvDouble)
         ->createStructure()))
{}

DefaultBeaconServerStatusProvider::~DefaultBeaconServerStatusProvider() {}

PVField::shared_pointer DefaultBeaconServerStatusProvider::getServerStatusData()
{
    //TODO implement (fill data)
    return _status;
}

}
}
