/*
 * beaconServerStatusProvider.cpp
 */

#include "beaconServerStatusProvider.h"

namespace epics { namespace pvAccess {

BeaconServerStatusProvider::BeaconServerStatusProvider( ServerContext* context): _context(context)
{
	if(context == NULL)
	{
		throw EpicsException("null context");
	}
	initialize();
}

BeaconServerStatusProvider::BeaconServerStatusProvider()
{
	initialize();
}

BeaconServerStatusProvider::~BeaconServerStatusProvider()
{
}

void BeaconServerStatusProvider::initialize()
{
	PVDataCreate* pvDataCreate = getPVDataCreate();
	FieldCreate* fieldCreate = getFieldCreate();
	FieldConstPtrArray fields = new FieldConstPtr[6];
	// TODO hierarchy can be used...
	fields[0] = fieldCreate->createScalar("connections",pvInt);
	fields[1] = fieldCreate->createScalar("allocatedMemory",pvLong);
	fields[2] = fieldCreate->createScalar("freeMemory",pvLong);
	fields[3] = fieldCreate->createScalar("threads",pvInt);
	fields[4] = fieldCreate->createScalar("deadlocks",pvInt);
	fields[5] = fieldCreate->createScalar("averageSystemLoad",pvDouble);

	_status = pvDataCreate->createPVStructure(NULL,"status",6,fields);
}

PVFieldPtr BeaconServerStatusProvider::getServerStatusData()
{
	//TODO implement
	return static_cast<PVFieldPtr>(_status);
}

}}

