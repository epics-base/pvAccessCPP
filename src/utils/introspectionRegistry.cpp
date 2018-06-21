/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define epicsExportSharedSymbols
#include <pv/introspectionRegistry.h>
#include <pv/serializationHelper.h>

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
        SerializationHelper::serializeNullField(buffer, control);
    }
    else
    {
        // do not cache scalars, scalarArrays
        // ... and (array of) variant unions - not worth the complex condition,
        // unless bool Field.cache() would exist
        if (field->getType() != scalar &&
                field->getType() != scalarArray)
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
        control->ensureData(sizeof(int16));
        int16 id = buffer->getShort();

        registryMap_t::iterator registryIter = _registry.find(id);
        if(registryIter == _registry.end())
        {
            // peer gave us an id which we don't remember.
            // This isn't recoverable.
            throw std::runtime_error("IntrospectionRegistry miss.");
        }
        return registryIter->second;
    }
    // could also be a mask
    if(typeCode == IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE)
    {
        control->ensureData(sizeof(int16)/sizeof(int8));
        const short key = buffer->getShort();
        FieldConstPtr field = _fieldCreate->deserialize(buffer, control);
        _registry[key] = field;
        return field;
    }

    // return typeCode back
    buffer->setPosition(pos);
    return _fieldCreate->deserialize(buffer, control);
}

}
}

