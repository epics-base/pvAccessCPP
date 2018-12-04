/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef INTROSPECTIONREGISTRY_H
#define INTROSPECTIONREGISTRY_H

#include <map>
#include <iostream>

#ifdef epicsExportSharedSymbols
#   define introspectionRegistryEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/lock.h>
#include <pv/pvIntrospect.h>
#include <pv/pvData.h>
#include <pv/byteBuffer.h>
#include <pv/serialize.h>
#include <pv/serializeHelper.h>
#include <pv/status.h>
#include <pv/standardField.h>

#ifdef introspectionRegistryEpicsExportSharedSymbols
#   define epicsExportSharedSymbols
#	undef introspectionRegistryEpicsExportSharedSymbols
#endif

// TODO check for memory leaks

namespace epics {
namespace pvAccess {

typedef std::map<const short,epics::pvData::FieldConstPtr> registryMap_t;


/**
 * PVData Structure registry.
 * Registry is used to cache introspection interfaces to minimize network traffic.
 * @author gjansa
 */
class IntrospectionRegistry {
    EPICS_NOT_COPYABLE(IntrospectionRegistry)
public:
    IntrospectionRegistry();
    virtual ~IntrospectionRegistry();

    /**
     * Resets registry, i.e. must be done when transport is changed (server restarted).
     */
    void reset();

private:
    /**
     * Registers introspection interface and get it's ID. Always OUTGOING.
     * If it is already registered only preassigned ID is returned.
     *
     * TODO !!!!!!this can get very slow in large maps. We need to change this !!!!!!
     *
     * @param field introspection interface to register
     *
     * @return id of given introspection interface
     */
    epics::pvData::int16 registerIntrospectionInterface(epics::pvData::FieldConstPtr const & field, bool& existing);
public:
    /**
     * Serializes introspection interface
     *
     * @param field
     * @param buffer
     * @param control
     */
    void serialize(epics::pvData::FieldConstPtr const & field, epics::pvData::ByteBuffer* buffer, epics::pvData::SerializableControl* control);

    /**
     * Deserializes introspection interface
     *
     * TODO
     *
     * @param buffer
     * @param control
     *
     * @return <code>Field</code> deserialized from the buffer.
     */
    epics::pvData::FieldConstPtr deserialize(epics::pvData::ByteBuffer* buffer, epics::pvData::DeserializableControl* control);

    /**
     * Null type.
     */
    const static epics::pvData::int8 NULL_TYPE_CODE;

    /**
     * Serialization contains only an ID (that was assigned by one of the previous <code>FULL_WITH_ID</code> descriptions).
     */
    const static epics::pvData::int8 ONLY_ID_TYPE_CODE;

    /**
     * Serialization contains an ID (that can be used later, if cached) and full interface description.
     */
    const static epics::pvData::int8 FULL_WITH_ID_TYPE_CODE;

private:
    registryMap_t _registry;
    epics::pvData::int16 _pointer;

    /**
     * Field factory.
     */
    static epics::pvData::FieldCreatePtr _fieldCreate;

    bool registryContainsValue(epics::pvData::FieldConstPtr const & field, epics::pvData::int16& key);
};

}
}

#endif  /* INTROSPECTIONREGISTRY_H */
