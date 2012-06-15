/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/introspectionRegistry.h>
#include <pv/convert.h>

using namespace epics::pvData;
using namespace std;
using std::tr1::static_pointer_cast;

namespace epics {
namespace pvAccess {

const int8 IntrospectionRegistry::NULL_TYPE_CODE = (int8)-1;
const int8 IntrospectionRegistry::ONLY_ID_TYPE_CODE = (int8)-2;
const int8 IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE = (int8)-3;
PVDataCreatePtr IntrospectionRegistry::_pvDataCreate (getPVDataCreate());
FieldCreatePtr IntrospectionRegistry::_fieldCreate(getFieldCreate());

IntrospectionRegistry::IntrospectionRegistry(bool serverSide)
{
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
		
		field->serialize(buffer, control);
	}
}

FieldConstPtr IntrospectionRegistry::deserialize(ByteBuffer* buffer, DeserializableControl* control, IntrospectionRegistry* registry)
{
	control->ensureData(1);
	size_t pos = buffer->getPosition();
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

    buffer->setPosition(pos);
    // TODO
    return getFieldCreate()->deserialize(buffer, control);
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
	PVStructurePtr pvStructure;
	FieldConstPtr structureField = deserialize(buffer, control);
	if (structureField.get() != NULL)
	{
        pvStructure = _pvDataCreate->createPVStructure(static_pointer_cast<const Structure>(structureField));
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
		return PVStructurePtr();

	return _pvDataCreate->createPVStructure(static_pointer_cast<const Structure>(field));
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

