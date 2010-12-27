/*
 * introspectionRegistry.cpp
 */

#include "introspectionRegistry.h"

namespace epics { namespace pvAccess {

const int8 IntrospectionRegistry::NULL_TYPE_CODE = (int8)-1;
const int8 IntrospectionRegistry::ONLY_ID_TYPE_CODE = (int8)-2;
const int8 IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE = (int8)-3;
PVDataCreate* IntrospectionRegistry::_pvDataCreate = getPVDataCreate();
StatusCreate* IntrospectionRegistry::_statusCreate = getStatusCreate();
FieldCreate* IntrospectionRegistry::_fieldCreate = getFieldCreate();

IntrospectionRegistry::IntrospectionRegistry(bool serverSide) : _mutex(Mutex())
{

	_direction = serverSide ? 1 : -1;
	reset();
}

IntrospectionRegistry::~IntrospectionRegistry()
{
}

void IntrospectionRegistry::reset()
{
	Lock guard(&_mutex);
	_outgoingIdPointer = _direction;
	//decrement references
	for(_registryRIter = _registry.rbegin(); _registryRIter != _registry.rend(); _registryRIter++)
	{
		_registryRIter->second->decReferenceCount();
	}
	_registry.clear();
}

FieldConstPtr IntrospectionRegistry::getIntrospectionInterface(const short id)
{
	Lock guard(&_mutex);
	_registryIter = _registry.find(id);
	if(_registryIter == _registry.end())
	{
		throw EpicsException("missing interface with given id");
	}
	return _registryIter->second;
}

void IntrospectionRegistry::registerIntrospectionInterface(const short id,FieldConstPtr field)
{
	Lock guard(&_mutex);
	//first decrement reference on old value
	_registryIter = _registry.find(id);
	if(_registryIter != _registry.end())
	{
		_registryIter->second->decReferenceCount();
	}
	_registry[id] = field;
	field->incReferenceCount();
}

short IntrospectionRegistry::registerIntrospectionInterface(FieldConstPtr field, bool& existing)
{
	Lock guard(&_mutex);
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
		if(_registryIter != _registry.end())
		{
			cout << "deleting 2" << endl;
			_registryIter->second->decReferenceCount();
		}

		_registry[key] = field;
		field->incReferenceCount();
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
		_registryIter->second->toString(&buffer);
		cout << "\t" << "Value toString: " << buffer.c_str() << endl;
	}
}

bool IntrospectionRegistry::registryContainsValue(FieldConstPtr field, short& key)
{
	for(_registryRIter = _registry.rbegin(); _registryRIter != _registry.rend(); _registryRIter++)
	{
		if(compareFields(field,_registryRIter->second))
		{
			key = _registryRIter->first;
			return true;
		}
	}
	return false;
}

bool IntrospectionRegistry::compareFields(FieldConstPtr field1, FieldConstPtr field2)
{
	if(field1->getFieldName() != field2->getFieldName())
	{
		return false;
	}
	else
	{
		Type typ1 = field1->getType();
		Type typ2 = field2->getType();
		if(typ1 != typ2)
		{
			return false;
		}

		switch (typ1)
		{
		case epics::pvData::scalar:
		{
			if(static_cast<ScalarConstPtr>(field1)->getScalarType() == static_cast<ScalarConstPtr>(field1)->getScalarType())
			{
				return true;
			}
			break;
		}
		case epics::pvData::scalarArray:
		{
			if(static_cast<ScalarArrayConstPtr>(field1)->getElementType() == static_cast<ScalarArrayConstPtr>(field1)->getElementType())
			{
				return true;
			}
			break;
		}
		case epics::pvData::structure:
		{
			return true;
			break;
		}
		case epics::pvData::structureArray:
		{
			if(static_cast<StructureArrayConstPtr>(field1)->getStructure()->getFieldName() == static_cast<StructureArrayConstPtr>(field1)->getStructure()->getFieldName())
			{
				return true;
			}
			break;
		}
		}
		return false;
	}
}

void IntrospectionRegistry::serialize(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control)
{
	checkBufferAndSerializeControl(buffer, control);
	serialize(field, NULL, buffer, control, this);
}

FieldConstPtr IntrospectionRegistry::deserialize(ByteBuffer* buffer, DeserializableControl* control)
{
	checkBufferAndDeserializeControl(buffer, control);
	return deserialize(buffer, control, this);
}

void IntrospectionRegistry::serializeFull(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control)
{
	checkBufferAndSerializeControl(buffer, control);
	serialize(field, NULL, buffer, control, NULL);
}

FieldConstPtr IntrospectionRegistry::deserializeFull(ByteBuffer* buffer, DeserializableControl* control)
{
	checkBufferAndDeserializeControl(buffer, control);
	return deserialize(buffer, control, NULL);
}

void IntrospectionRegistry::serialize(FieldConstPtr field, StructureConstPtr parent, ByteBuffer* buffer,
							  SerializableControl* control, IntrospectionRegistry* registry)
{
	checkBufferAndSerializeControl(buffer, control);
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
			/*cout << "@@@@@@@@@" << endl;
			cout << field->getFieldName() << endl;
			cout << "address: " << field << endl;
			cout << "existing: " << existing << endl;
			cout << "key: " << key << endl;*/
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
			ScalarConstPtr scalar = static_cast<ScalarConstPtr>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::scalar << 4 | scalar->getScalarType()));
			SerializeHelper::serializeString(field->getFieldName(), buffer, control);
			break;
		}
		case epics::pvData::scalarArray:
		{
			ScalarArrayConstPtr array = static_cast<ScalarArrayConstPtr>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::scalarArray << 4 | array->getElementType()));
			SerializeHelper::serializeString(field->getFieldName(), buffer, control);
			break;
		}
		case epics::pvData::structure:
		{
			StructureConstPtr structure = static_cast<StructureConstPtr>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::structure << 4));
			serializeStructureField(buffer, control, registry, structure);
			break;
		}
		case epics::pvData::structureArray:
		{
			StructureArrayConstPtr structureArray = static_cast<StructureArrayConstPtr>(field);
			control->ensureBuffer(1);
			buffer->putByte((int8)(epics::pvData::structureArray << 4));
			SerializeHelper::serializeString(field->getFieldName(), buffer, control);
			// we also need to serialize structure field...
			const Structure* structureElement = structureArray->getStructure();
			serializeStructureField(buffer, control, registry, structureElement);
			break;
		}
		}
	}
}

void IntrospectionRegistry::serializeStructureField(ByteBuffer* buffer, SerializableControl* control,
		IntrospectionRegistry* registry, StructureConstPtr structure)
{
	checkBufferAndSerializeControl(buffer, control);
	if(structure == NULL)
	{
		throw EpicsException("null structure provided");
	}
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
	checkBufferAndDeserializeControl(buffer, control);
	control->ensureData(1);
	const int8 typeCode = buffer->getByte();
	if(typeCode == IntrospectionRegistry::NULL_TYPE_CODE)
	{
		return NULL;
	}
	else if(typeCode == IntrospectionRegistry::ONLY_ID_TYPE_CODE)
	{
		if (registry == NULL)
		{
			throw EpicsException("deserialization provided chached ID, but no registry provided");
		}
		control->ensureData(sizeof(int16)/sizeof(int8));
		return registry->getIntrospectionInterface(buffer->getShort());
	}

	// could also be a mask
	if(typeCode == IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE)
	{
		if(registry == NULL)
		{
			throw EpicsException("deserialization provided chached ID, but no registry provided");
		}
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
		std::string msg = "unsupported type: " + type;
		throw EpicsException(msg);
	}
	}
}

StructureConstPtr IntrospectionRegistry::deserializeStructureField(ByteBuffer* buffer, DeserializableControl* control, IntrospectionRegistry* registry)
{
	checkBufferAndDeserializeControl(buffer, control);
	const String structureFieldName = SerializeHelper::deserializeString(buffer, control);
	const int32 size = SerializeHelper::readSize(buffer, control);
	FieldConstPtrArray fields = NULL;
	if (size > 0)
	{
		fields = new FieldConstPtr[size];
		for(int i = 0; i < size; i++)
		{
			fields[i] = deserialize(buffer, control, registry);
		}
	}
	//TODO stucture constructor created new fields instead of taking this ones
	return _fieldCreate->createStructure(structureFieldName, size, fields);
}

void IntrospectionRegistry::serializeStructure(ByteBuffer* buffer, SerializableControl* control, PVStructurePtr pvStructure)
{
	checkBufferAndSerializeControl(buffer, control);
	if (pvStructure == NULL)
	{
		serialize(NULL, buffer, control);
	}
	else
	{
		serialize(pvStructure->getField(), buffer, control);
		pvStructure->serialize(buffer, control);
	}
}

PVStructurePtr IntrospectionRegistry::deserializeStructure(ByteBuffer* buffer, DeserializableControl* control)
{
	checkBufferAndDeserializeControl(buffer, control);
	PVStructurePtr pvStructure = NULL;
	FieldConstPtr structureField = deserialize(buffer, control);
	if (structureField != NULL)
	{
		pvStructure = _pvDataCreate->createPVStructure(NULL, static_cast<StructureConstPtr>(structureField));
		pvStructure->deserialize(buffer, control);
	}
	return pvStructure;
}

void IntrospectionRegistry::serializePVRequest(ByteBuffer* buffer, SerializableControl* control, PVStructurePtr pvRequest)
{
	checkBufferAndSerializeControl(buffer, control);
	// for now ordinary structure, later can be changed
	serializeStructure(buffer, control, pvRequest);
}

PVStructurePtr IntrospectionRegistry::deserializePVRequest(ByteBuffer* buffer, DeserializableControl* control)
{
	checkBufferAndDeserializeControl(buffer, control);
	// for now ordinary structure, later can be changed
	return deserializeStructure(buffer, control);
}

PVStructurePtr IntrospectionRegistry::deserializeStructureAndCreatePVStructure(ByteBuffer* buffer, DeserializableControl* control)
{
	checkBufferAndDeserializeControl(buffer, control);
	FieldConstPtr field = deserialize(buffer, control);
	if (field == NULL)
	{
		return NULL;
	}
	return _pvDataCreate->createPVStructure(NULL,static_cast<StructureConstPtr>(field));
}

void IntrospectionRegistry::serializeStatus(ByteBuffer* buffer, SerializableControl* control, Status* status)
{
	checkBufferAndSerializeControl(buffer, control);
	if(status != NULL)
	{
		status->serialize(buffer, control);
	}
	else
	{
		throw EpicsException("null status provided");
	}
}

Status* IntrospectionRegistry::deserializeStatus(ByteBuffer* buffer, DeserializableControl* control)
{
	checkBufferAndDeserializeControl(buffer, control);
	return _statusCreate->deserializeStatus(buffer, control);
}

void IntrospectionRegistry::checkBufferAndSerializeControl(ByteBuffer* buffer, SerializableControl* control)
{
	if(buffer == NULL)
	{
		throw EpicsException("null buffer provided");
	}
	if(control == NULL)
	{
		throw EpicsException("null control provided");
	}
}

void IntrospectionRegistry::checkBufferAndDeserializeControl(ByteBuffer* buffer, DeserializableControl* control)
{
	if(buffer == NULL)
	{
		throw EpicsException("null buffer provided");
	}
	if(control == NULL)
	{
		throw EpicsException("null control provided");
	}
}

}}

