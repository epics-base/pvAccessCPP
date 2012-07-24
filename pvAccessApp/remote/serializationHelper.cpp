/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/serializationHelper.h>
#include <pv/introspectionRegistry.h>
#include <pv/convert.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

PVDataCreatePtr SerializationHelper::_pvDataCreate(getPVDataCreate());

PVStructure::shared_pointer SerializationHelper::deserializePVRequest(ByteBuffer* buffer, DeserializableControl* control)
{
	// for now ordinary structure, later can be changed
	return deserializeStructureFull(buffer, control);
}

PVStructure::shared_pointer SerializationHelper::deserializeStructureAndCreatePVStructure(ByteBuffer* buffer, DeserializableControl* control)
{
	return deserializeStructureAndCreatePVStructure(buffer, control, PVStructure::shared_pointer());
}

PVStructure::shared_pointer SerializationHelper::deserializeStructureAndCreatePVStructure(ByteBuffer* buffer, DeserializableControl* control, PVStructure::shared_pointer const & existingStructure)
{
	FieldConstPtr field = control->cachedDeserialize(buffer);
	if (field.get() == 0)
		return PVStructure::shared_pointer();

	if (existingStructure.get() != 0 && *(field.get()) == *(existingStructure->getField()))
		return existingStructure;
	else
		return _pvDataCreate->createPVStructure(std::tr1::static_pointer_cast<const Structure>(field));
}

PVStructure::shared_pointer SerializationHelper::deserializeStructureFull(ByteBuffer* buffer, DeserializableControl* control)
{
    PVStructure::shared_pointer pvStructure;
	FieldConstPtr structureField = control->cachedDeserialize(buffer);
    if (structureField.get() != 0)
    {
    	pvStructure = _pvDataCreate->createPVStructure(std::tr1::static_pointer_cast<const Structure>(structureField));
    	pvStructure->deserialize(buffer, control);
    }
    return pvStructure;
}

void SerializationHelper::serializeNullField(ByteBuffer* buffer, SerializableControl* control)
{
	control->ensureBuffer(1);
	buffer->putByte(IntrospectionRegistry::NULL_TYPE_CODE);
}

void SerializationHelper::serializePVRequest(ByteBuffer* buffer, SerializableControl* control, PVStructure::shared_pointer const & pvRequest)
{
	// for now ordinary structure, later can be changed
	serializeStructureFull(buffer, control, pvRequest);
}

void SerializationHelper::serializeStructureFull(ByteBuffer* buffer, SerializableControl* control, PVStructure::shared_pointer const & pvStructure)
{
	if (pvStructure.get() == 0)
	{
    	serializeNullField(buffer, control);
	}
	else
	{
		control->cachedSerialize(pvStructure->getField(), buffer);
		pvStructure->serialize(buffer, control);
	}
}

}}


