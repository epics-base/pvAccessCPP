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
FieldCreatePtr IntrospectionRegistry::_fieldCreate(getFieldCreate());

IntrospectionRegistry::IntrospectionRegistry()
{
	reset();
}

IntrospectionRegistry::~IntrospectionRegistry()
{
    reset();
}

void IntrospectionRegistry::reset()
{
	_pointer = 1;
	_registry.clear();
}

FieldConstPtr IntrospectionRegistry::getIntrospectionInterface(const int16 id)
{
	registryMap_t::iterator registryIter = _registry.find(id);
	if(registryIter == _registry.end())
	{
           return FieldConstPtr();
	}
	return registryIter->second;
}

void IntrospectionRegistry::registerIntrospectionInterface(const int16 id, FieldConstPtr const & field)
{
	_registry[id] = field;
}

int16 IntrospectionRegistry::registerIntrospectionInterface(FieldConstPtr const & field, bool& existing)
{
	int16 key;
	// TODO this is slow
	if(registryContainsValue(field, key))
	{
		existing = true;
	}
	else
	{
		existing = false;
		key = _pointer++;
		_registry[key] = field;
	}
	return key;
}

void IntrospectionRegistry::printKeysAndValues(string name)
{
	string buffer;
	cout << "############## print of all key/values of " << name.c_str() << " registry : ###################" << endl;
	for(registryMap_t::iterator registryIter = _registry.begin(); registryIter != _registry.end(); registryIter++)
	{
		buffer.clear();
		cout << "\t" << "Key: "<< registryIter->first << endl;
		cout << "\t" << "Value: " << registryIter->second << endl;

		cout << "\t" << "References: " << buffer.c_str() << endl;
		buffer.clear();
		registryIter->second->toString(&buffer);
		cout << "\t" << "Value toString: " << buffer.c_str() << endl;
	}
}

// TODO slow !!!!
bool IntrospectionRegistry::registryContainsValue(FieldConstPtr const & field, int16& key)
{
	for(registryMap_t::reverse_iterator registryRIter = _registry.rbegin(); registryRIter != _registry.rend(); registryRIter++)
	{
		if(*(field.get()) == *(registryRIter->second))
		{
			key = registryRIter->first;
			return true;
		}
	}
	return false;
}

void IntrospectionRegistry::serialize(FieldConstPtr const & field, ByteBuffer* buffer, SerializableControl* control)
{
	if (field.get() == NULL)
	{
		// TODO
		//SerializationHelper::serializeNullField(buffer, control);
		control->ensureBuffer(1);
		buffer->putByte(IntrospectionRegistry::NULL_TYPE_CODE);
	}
	else
	{
		// only structures registry check
		if (field->getType() == structure)
		{
			bool existing;
			const int16 key = registerIntrospectionInterface(field, existing);
			if (existing) {
				control->ensureBuffer(3);
				buffer->putByte(ONLY_ID_TYPE_CODE);
				buffer->putShort(key);
				return;
			}
			else {
				control->ensureBuffer(3);
				buffer->putByte(FULL_WITH_ID_TYPE_CODE);	// could also be a mask
				buffer->putShort(key);
			}
		}

		field->serialize(buffer, control);
	}
}

FieldConstPtr IntrospectionRegistry::deserialize(ByteBuffer* buffer, DeserializableControl* control)
{
	control->ensureData(1);
	size_t pos = buffer->getPosition();
	const int8 typeCode = buffer->getByte();

	if (typeCode == NULL_TYPE_CODE)
	{
        return FieldConstPtr();
	}
	else if (typeCode == ONLY_ID_TYPE_CODE)
	{
		control->ensureData(sizeof(int16)/sizeof(int8));
		return getIntrospectionInterface(buffer->getShort());
	}
	// could also be a mask
	if(typeCode == IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE)
	{
		control->ensureData(sizeof(int16)/sizeof(int8));
		const short key = buffer->getShort();
		FieldConstPtr field = _fieldCreate->deserialize(buffer, control);
		registerIntrospectionInterface(key, field);
		return field;
	}

	// return typeCode back
    buffer->setPosition(pos);
	return _fieldCreate->deserialize(buffer, control);
}

/*
void IntrospectionRegistry::serializeFull(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control)
{
        serialize(field, StructureConstPtr(), buffer, control, NULL);
}

FieldConstPtr IntrospectionRegistry::deserializeFull(ByteBuffer* buffer, DeserializableControl* control)
{
	return deserialize(buffer, control, NULL);
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
*/

}}

