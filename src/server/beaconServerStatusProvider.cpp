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

DefaultBeaconServerStatusProvider::DefaultBeaconServerStatusProvider(ServerContext::shared_pointer const & context): _context(context)
{
    initialize();
}

DefaultBeaconServerStatusProvider::~DefaultBeaconServerStatusProvider()
{
}

void DefaultBeaconServerStatusProvider::initialize()
{
    FieldCreatePtr fieldCreate = getFieldCreate();

    StringArray fieldNames;
    fieldNames.resize(6);
    fieldNames[0] = "connections";
    fieldNames[1] = "allocatedMemory";
    fieldNames[2] = "freeMemory";
    fieldNames[3] = "threads";
    fieldNames[4] = "deadlocks";
    fieldNames[5] = "averageSystemLoad";

    FieldConstPtrArray fields;
    fields.resize(6);
    // TODO hierarchy can be used...
    fields[0] = fieldCreate->createScalar(pvInt);
    fields[1] = fieldCreate->createScalar(pvLong);
    fields[2] = fieldCreate->createScalar(pvLong);
    fields[3] = fieldCreate->createScalar(pvInt);
    fields[4] = fieldCreate->createScalar(pvInt);
    fields[5] = fieldCreate->createScalar(pvDouble);

    _status = getPVDataCreate()->createPVStructure(fieldCreate->createStructure(fieldNames, fields));
}

PVField::shared_pointer DefaultBeaconServerStatusProvider::getServerStatusData()
{
    //TODO implement (fill data)
    return _status;
}

}
}

