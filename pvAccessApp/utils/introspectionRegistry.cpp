/*
 * introspectionRegistry.cpp
 */

#include <pv/introspectionRegistry.h>
#include <pv/convert.h>

using namespace epics::pvData;
using namespace std;
using std::tr1::static_pointer_cast;

namespace epics { namespace pvAccess {

const int8 IntrospectionRegistry::NULL_TYPE_CODE = (int8)-1;
const int8 IntrospectionRegistry::ONLY_ID_TYPE_CODE = (int8)-2;
const int8 IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE = (int8)-3;
PVDataCreate* IntrospectionRegistry::_pvDataCreate = 0;
FieldCreate* IntrospectionRegistry::_fieldCreate = 0;

IntrospectionRegistry::IntrospectionRegistry(bool serverSide)
{
    // TODO not optimal
	_pvDataCreate = getPVDataCreate();
	_fieldCreate = getFieldCreate();

	_direction = serverSide ? 1 : -1;
	reset();
}

IntrospectionRegistry::~IntrospectionRegistry()
{
    reset();
}

void IntrospectionRegistry::reset()
{
	Lock guard(_mutex);
	_outgoingIdPointer = _direction;

	_registry.clear();
}

FieldConstPtr IntrospectionRegistry::getIntrospectionInterface(const short id)
{
	Lock guard(_mutex);
	_registryIter = _registry.find(id);
	if(_registryIter == _registry.end())
	{
           return FieldConstPtr();
	}
	return _registryIter->second;
}

void IntrospectionRegistry::registerIntrospectionInterface(const short id,FieldConstPtr field)
{
	Lock guard(_mutex);

	_registryIter = _registry.find(id);

	_registry[id] = field;

}

short IntrospectionRegistry::registerIntrospectionInterface(FieldConstPtr field, bool& existing)
{
	Lock guard(_mutex);
	short key;
	if(registryContainsValue(field, key))
	{
		existing = true;
	}
	else
	{
		existing = false;
		key = _outgoingIdPointer;
		_outgoingIdPointer += _direction;
		// wrap check
		if(_outgoingIdPointer * _direction < 0)
		{
			_outgoingIdPointer = _direction;
		}

		//first decrement reference on old value
		_registryIter = _registry.find(key);

		_registry[key] = field;

	}
	return key;
}

void IntrospectionRegistry::printKeysAndValues(string name)
{
	string buffer;
	cout << "############## print of all key/values of " << name.c_str() << " registry : ###################" << endl;
	for(_registryIter = _registry.begin(); _registryIter != _registry.end(); _registryIter++)
	{
		buffer.clear();
		cout << "\t" << "Key: "<< _registryIter->first << endl;
		cout << "\t" << "Value: " << _registryIter->second << endl;

		cout << "\t" << "References: " << buffer.c_str() << endl;
		buffer.clear();
		_registryIter->second->toString(&buffer);
		cout << "\t" << "Value toString: " << buffer.c_str() << endl;
	}
}

// TODO !!!!
bool IntrospectionRegistry::registryContainsValue(FieldConstPtr field, short& key)
{
	for(_registryRIter = _registry.rbegin(); _registryRIter != _registry.rend(); _registryRIter++)
	{
		if((*field) == (*_registryRIter->second))
		{
			key = _registryRIter->first;
			return true;
		}
	}
	return false;
}

void IntrospectionRegistry::serialize(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control)
{
        serialize(field, StructureConstPtr(), buffer, control, this);
}

FieldConstPtr IntrospectionRegistry::deserialize(ByteBuffer* buffer, DeserializableControl* control)
{
	return deserialize(buffer, control, this);
}

void IntrospectionRegistry::serializeFull(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control)
{
        serialize(field, StructureConstPtr(), buffer, control, NULL);
}

FieldConstPtr IntrospectionRegistry::deserializeFull(ByteBuffer* buffer, DeserializableControl* control)
{
	return deserialize(buffer, control, NULL);
}

void IntrospectionRegistry::serialize(FieldConstPtr field, StructureConstPtr parent, ByteBuffer* buffer,
							  SerializableControl* control, IntrospectionRegistry* registry)
{
	if (field == NULL)
	{
		control->ensureBuffer(1);
		buffer->putByte(IntrospectionRegistry::NULL_TYPE_CODE);
	}
	else
	{
		// use registry check
		// only top IFs and structures
		if (registry != NULL && (parent == NULL || field->getType() == epics::pvData::structure || field->getType() == epics::pvData::structureArray))
		{
			bool existing;
			const short key = registry->registerIntrospectionInterface(field, existing);
			if(existing)
			{
				control->ensureBuffer(1+sizeof(int16)/sizeof(int8));
				buffer->putByte(ONLY_ID_TYPE_CODE);
				buffer->putShort(key);
				return;
			}
			else
			{
				control->ensureBuffer(1+sizeof(int16)/sizeof(int8));
				buffer->putByte(FULL_WITH_ID_TYPE_CODE);	// could also be a mask
				buffer->putShort(key);
			}
		}

		// NOTE: high nibble is field.getType() ordinal, low nibble is scalar type ordinal; -1 is null
		switch (field->getType())
		{
		case epics::pvData::scalar:
		{
                        ScalarConstPtr scalar = static_pointer_cast<const Scalar>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::scalar << 4 | scalar->getScalarType()));
			SerializeHelper::serializeString(field->getFieldName(), buffer, control);
			break;
		}
		case epics::pvData::scalarArray:
		{
                        ScalarArrayConstPtr array = static_pointer_cast<const ScalarArray>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::scalarArray << 4 | array->getElementType()));
			SerializeHelper::serializeString(field->getFieldName(), buffer, control);
			break;
		}
		case epics::pvData::structure:
		{
                        StructureConstPtr structure = static_pointer_cast<const Structure>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::structure << 4));
			serializeStructureField(buffer, control, registry, structure);
			break;
		}
		case epics::pvData::structureArray:
		{
                        StructureArrayConstPtr structureArray = static_pointer_cast<const StructureArray>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::structureArray << 4));
			SerializeHelper::serializeString(field->getFieldName(), buffer, control);
			// we also need to serialize structure field...
                        StructureConstPtr structureElement = structureArray->getStructure();
			serializeStructureField(buffer, control, registry, structureElement);
			break;
		}
		}
	}
}

void IntrospectionRegistry::serializeStructureField(ByteBuffer* buffer, SerializableControl* control,
		IntrospectionRegistry* registry, StructureConstPtr structure)
{
	SerializeHelper::serializeString(structure->getFieldName(), buffer, control);
	FieldConstPtrArray fields = structure->getFields();
	SerializeHelper::writeSize(structure->getNumberFields(), buffer, control);
	for (int i = 0; i < structure->getNumberFields(); i++)
	{
		serialize(fields[i], structure, buffer, control, registry);
	}
}

FieldConstPtr IntrospectionRegistry::deserialize(ByteBuffer* buffer, DeserializableControl* control, IntrospectionRegistry* registry)
{
	control->ensureData(1);
	const int8 typeCode = buffer->getByte();
	if(typeCode == IntrospectionRegistry::NULL_TYPE_CODE)
	{
                return FieldConstPtr();
	}
	else if(typeCode == IntrospectionRegistry::ONLY_ID_TYPE_CODE)
	{
		control->ensureData(sizeof(int16)/sizeof(int8));
		FieldConstPtr field = registry->getIntrospectionInterface(buffer->getShort());

	    return field;
	}

	// could also be a mask
	if(typeCode == IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE)
	{
		control->ensureData(sizeof(int16)/sizeof(int8));
		const short key = buffer->getShort();
		FieldConstPtr field = deserialize(buffer, control, registry);
		registry->registerIntrospectionInterface(key, field);
		return field;
	}


	// high nibble means scalar/array/structure
	const Type type = (Type)(typeCode >> 4);
	switch (type)
	{
	case scalar:
	{
		const ScalarType scalar = (ScalarType)(typeCode & 0x0F);
		const String scalarFieldName = SerializeHelper::deserializeString(buffer, control);
		return static_cast<FieldConstPtr>(_fieldCreate->createScalar(scalarFieldName,scalar));
	}
	case scalarArray:
	{
		const ScalarType element = (ScalarType)(typeCode & 0x0F);
		const String arrayFieldName = SerializeHelper::deserializeString(buffer, control);
		return static_cast<FieldConstPtr>(_fieldCreate->createScalarArray(arrayFieldName,element));
	}
	case structure:
	{
		return static_cast<FieldConstPtr>(deserializeStructureField(buffer, control, registry));
	}
	case structureArray:
	{
		const String structureArrayFieldName = SerializeHelper::deserializeString(buffer, control);
		const StructureConstPtr arrayElement = deserializeStructureField(buffer, control, registry);
		return  static_cast<FieldConstPtr>(_fieldCreate->createStructureArray(structureArrayFieldName, arrayElement));
	}
	default:
	{
	   // TODO log
           return FieldConstPtr();
	}
	}
}

StructureConstPtr IntrospectionRegistry::deserializeStructureField(ByteBuffer* buffer, DeserializableControl* control, IntrospectionRegistry* registry)
{
	const String structureFieldName = SerializeHelper::deserializeString(buffer, control);
	const int32 size = SerializeHelper::readSize(buffer, control);
	FieldConstPtrArray fields = NULL;
	if (size > 0)
	{
		fields = new FieldConstPtr[size];
		for(int i = 0; i < size; i++)
		{
		  try {
			fields[i] = deserialize(buffer, control, registry);
		  } catch (...) {
		      delete[] fields;
		      throw;
		  }
		}
	}

	StructureConstPtr structure = _fieldCreate->createStructure(structureFieldName, size, fields);
	return structure;
}

void IntrospectionRegistry::serializeStructure(ByteBuffer* buffer, SerializableControl* control, PVStructurePtr pvStructure)
{
	if (pvStructure == NULL)
	{
                serialize(StructureConstPtr(), buffer, control);
	}
	else
	{
		serialize(pvStructure->getField(), buffer, control);
		pvStructure->serialize(buffer, control);
	}
}

PVStructurePtr IntrospectionRegistry::deserializeStructure(ByteBuffer* buffer, DeserializableControl* control)
{
	PVStructurePtr pvStructure = NULL;
	FieldConstPtr structureField = deserialize(buffer, control);
	if (structureField != NULL)
	{
                pvStructure = _pvDataCreate->createPVStructure(NULL,
                                                               static_pointer_cast<const Structure>(structureField));
		pvStructure->deserialize(buffer, control);
	}
	return pvStructure;
}

void IntrospectionRegistry::serializePVRequest(ByteBuffer* buffer, SerializableControl* control, PVStructurePtr pvRequest)
{
	// for now ordinary structure, later can be changed
	serializeStructure(buffer, control, pvRequest);
}

PVStructurePtr IntrospectionRegistry::deserializePVRequest(ByteBuffer* buffer, DeserializableControl* control)
{
	// for now ordinary structure, later can be changed
	return deserializeStructure(buffer, control);
}

PVStructurePtr IntrospectionRegistry::deserializeStructureAndCreatePVStructure(ByteBuffer* buffer, DeserializableControl* control)
{
	FieldConstPtr field = deserialize(buffer, control);
	if (field == NULL)
	{
		return NULL;
	}
        PVStructurePtr retVal = _pvDataCreate->createPVStructure(NULL,
                                                                 static_pointer_cast<const Structure>(field));
	return retVal;
}

void IntrospectionRegistry::serializeStatus(ByteBuffer* buffer, SerializableControl* control, const Status& status)
{
    status.serialize(buffer, control);
}

void IntrospectionRegistry::deserializeStatus(Status &status, ByteBuffer* buffer, DeserializableControl* control)
{
	status.deserialize(buffer, control);
}


}}

