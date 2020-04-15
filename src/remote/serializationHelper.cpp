/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsAssert.h>

#include <pv/serialize.h>
#include <pv/pvData.h>
#include <pv/noDefaultMethods.h>
#include <pv/pvIntrospect.h>
#include <pv/byteBuffer.h>

#define epicsExportSharedSymbols
#include <pv/serializationHelper.h>
#include <pv/introspectionRegistry.h>

using namespace epics::pvData;

namespace epics {
namespace pvAccess {

PVDataCreatePtr SerializationHelper::_pvDataCreate(getPVDataCreate());

PVStructure::shared_pointer SerializationHelper::deserializePVRequest(ByteBuffer* buffer, DeserializableControl* control)
{
    // for now ordinary structure, later can be changed
    return deserializeStructureFull(buffer, control);
}

PVStructure::shared_pointer SerializationHelper::deserializeStructureAndCreatePVStructure(ByteBuffer* buffer, DeserializableControl* control, PVStructure::shared_pointer const & existingStructure)
{
    FieldConstPtr field = control->cachedDeserialize(buffer);
    if (!field)
        return PVStructure::shared_pointer();

    if (existingStructure && *field == *existingStructure->getField())
        return existingStructure;
    else if(field->getType()==structure)
        return _pvDataCreate->createPVStructure(std::tr1::static_pointer_cast<const Structure>(field));
    else
        throw std::runtime_error("deserializeStructureAndCreatePVStructure expects a Structure");
}

PVStructure::shared_pointer SerializationHelper::deserializeStructureFull(ByteBuffer* buffer, DeserializableControl* control)
{
    PVField::shared_pointer ret(deserializeFull(buffer, control));
    if(!ret) return PVStructure::shared_pointer();
    else if(ret->getField()->getType()!=structure)
        throw std::runtime_error("deserializeStructureFull expects a Structure");
    return std::tr1::static_pointer_cast<PVStructure>(ret);
}

PVField::shared_pointer SerializationHelper::deserializeFull(ByteBuffer* buffer, DeserializableControl* control)
{
    PVField::shared_pointer pvField;
    FieldConstPtr field = control->cachedDeserialize(buffer);
    if (field)
    {
        pvField = _pvDataCreate->createPVField(field);
        pvField->deserialize(buffer, control);
    }
    return pvField;
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
    serializeFull(buffer, control, pvStructure);
}

void SerializationHelper::serializeFull(ByteBuffer* buffer, SerializableControl* control, PVField::const_shared_pointer const & pvField)
{
    if (!pvField)
    {
        serializeNullField(buffer, control);
    }
    else
    {
        control->cachedSerialize(pvField->getField(), buffer);
        pvField->serialize(buffer, control);
    }
}

}
}
