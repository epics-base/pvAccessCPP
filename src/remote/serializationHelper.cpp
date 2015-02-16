/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

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
    return std::tr1::static_pointer_cast<PVStructure>(deserializeFull(buffer, control));
}

PVField::shared_pointer SerializationHelper::deserializeFull(ByteBuffer* buffer, DeserializableControl* control)
{
    PVField::shared_pointer pvField;
    FieldConstPtr field = control->cachedDeserialize(buffer);
    if (field.get() != 0)
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

void SerializationHelper::serializeFull(ByteBuffer* buffer, SerializableControl* control, PVField::shared_pointer const & pvField)
{
    if (pvField.get() == 0)
    {
        serializeNullField(buffer, control);
    }
    else
    {
        control->cachedSerialize(pvField->getField(), buffer);
        pvField->serialize(buffer, control);
    }
}

ConvertPtr SerializationHelper::_convert(getConvert());

void SerializationHelper::copyUnchecked(
                 epics::pvData::PVField::shared_pointer const & from,
                 epics::pvData::PVField::shared_pointer const & to)
{
    switch(from->getField()->getType())
    {
    case scalar:
        {
             PVScalar::shared_pointer fromS = std::tr1::static_pointer_cast<PVScalar>(from);
             PVScalar::shared_pointer toS = std::tr1::static_pointer_cast<PVScalar>(to);
             toS->assign(*fromS.get());
             break;
        }
    case scalarArray:
        {
             PVScalarArray::shared_pointer fromS = std::tr1::static_pointer_cast<PVScalarArray>(from);
             PVScalarArray::shared_pointer toS = std::tr1::static_pointer_cast<PVScalarArray>(to);
             toS->assign(*fromS.get());
             break;
        }
    case structure:
        {
             PVStructure::shared_pointer fromS = std::tr1::static_pointer_cast<PVStructure>(from);
             PVStructure::shared_pointer toS = std::tr1::static_pointer_cast<PVStructure>(to);
             copyStructureUnchecked(fromS, toS);
             break;
        }
    case structureArray:
        {
             PVStructureArray::shared_pointer fromS = std::tr1::static_pointer_cast<PVStructureArray>(from);
             PVStructureArray::shared_pointer toS = std::tr1::static_pointer_cast<PVStructureArray>(to);
             toS->replace(fromS->view());
             break;
        }
    case union_:
        {
             PVUnion::shared_pointer fromS = std::tr1::static_pointer_cast<PVUnion>(from);
             PVUnion::shared_pointer toS = std::tr1::static_pointer_cast<PVUnion>(to);
             _convert->copyUnion(fromS, toS);
             break;
        }
    case unionArray:
        {
             PVUnionArray::shared_pointer fromS = std::tr1::static_pointer_cast<PVUnionArray>(from);
             PVUnionArray::shared_pointer  toS = std::tr1::static_pointer_cast<PVUnionArray>(to);
             toS->replace(fromS->view());
             break;
        }
    default:
        {
            throw std::logic_error("SerializationHelper::copyUnchecked unknown type");
        }
    }
}


void SerializationHelper::copyStructureUnchecked(
                 PVStructure::shared_pointer const & from,
                 PVStructure::shared_pointer const & to)
{

    if (from.get() == to.get())
        return;

    PVFieldPtrArray const & fromPVFields = from->getPVFields();
    PVFieldPtrArray const & toPVFields = to->getPVFields();

    size_t fieldsSize = fromPVFields.size();
    for(size_t i = 0; i<fieldsSize; i++) {
        PVFieldPtr pvField = fromPVFields[i];
        int32 inumberFields = static_cast<int32>(pvField->getNumberFields());

        // serialize field or fields
        if(inumberFields==1) {
            copyUnchecked(pvField, toPVFields[i]);
        } else {
            PVStructure::shared_pointer fromPVStructure = std::tr1::static_pointer_cast<PVStructure>(pvField);
            PVStructure::shared_pointer toPVStructure = std::tr1::static_pointer_cast<PVStructure>(toPVFields[i]);
            copyStructureUnchecked(fromPVStructure, toPVStructure);
       }
    }
}

void SerializationHelper::partialCopy(PVStructure::shared_pointer const & from,
                 PVStructure::shared_pointer const & to,
                 BitSet::shared_pointer const & maskBitSet,
                 bool inverse) {

    if (from.get() == to.get())
        return;

    size_t numberFields = from->getNumberFields();
    size_t offset = from->getFieldOffset();
    int32 next = inverse ?
                maskBitSet->nextClearBit(static_cast<uint32>(offset)) :
                maskBitSet->nextSetBit(static_cast<uint32>(offset));

    // no more changes or no changes in this structure
    if(next<0||next>=static_cast<int32>(offset+numberFields)) return;

    // entire structure
    if(static_cast<int32>(offset)==next) {
        copyStructureUnchecked(from, to);
        return;
    }

    PVFieldPtrArray const & fromPVFields = from->getPVFields();
    PVFieldPtrArray const & toPVFields = to->getPVFields();

    size_t fieldsSize = fromPVFields.size();
    for(size_t i = 0; i<fieldsSize; i++) {
        PVFieldPtr pvField = fromPVFields[i];
        offset = pvField->getFieldOffset();
        int32 inumberFields = static_cast<int32>(pvField->getNumberFields());
        next = inverse ?
                    maskBitSet->nextClearBit(static_cast<uint32>(offset)) :
                    maskBitSet->nextSetBit(static_cast<uint32>(offset));

        // no more changes
        if(next<0) return;
        //  no change in this pvField
        if(next>=static_cast<int32>(offset+inumberFields)) continue;

        // serialize field or fields
        if(inumberFields==1) {
            copyUnchecked(pvField, toPVFields[i]);
        } else {
            PVStructure::shared_pointer fromPVStructure = std::tr1::static_pointer_cast<PVStructure>(pvField);
            PVStructure::shared_pointer toPVStructure = std::tr1::static_pointer_cast<PVStructure>(toPVFields[i]);
            partialCopy(fromPVStructure, toPVStructure, maskBitSet);
       }
    }
}


}}


