/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/beaconServerStatusProvider.h>
#include <pv/serverContext.h>

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
	FieldCreate* fieldCreate = getFieldCreate();
	FieldConstPtrArray fields = new FieldConstPtr[6];
	// TODO hierarchy can be used...
	fields[0] = fieldCreate->createScalar("connections",pvInt);
	fields[1] = fieldCreate->createScalar("allocatedMemory",pvLong);
	fields[2] = fieldCreate->createScalar("freeMemory",pvLong);
	fields[3] = fieldCreate->createScalar("threads",pvInt);
	fields[4] = fieldCreate->createScalar("deadlocks",pvInt);
	fields[5] = fieldCreate->createScalar("averageSystemLoad",pvDouble);

	_status.reset(getPVDataCreate()->createPVStructure(0,"status",6,fields));
}

PVField::shared_pointer DefaultBeaconServerStatusProvider::getServerStatusData()
{
	//TODO implement (fill data)
	return _status;
}

}}

