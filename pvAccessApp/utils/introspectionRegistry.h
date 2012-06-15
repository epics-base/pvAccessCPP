/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#ifndef INTROSPECTIONREGISTRY_H
#define INTROSPECTIONREGISTRY_H

#include <pv/lock.h>
#include <pv/pvIntrospect.h>
#include <pv/pvData.h>
#include <pv/byteBuffer.h>
#include <pv/serialize.h>
#include <pv/serializeHelper.h>
#include <pv/status.h>
#include <pv/standardField.h>

#include <map>
#include <iostream>

// TODO check for memory leaks

namespace epics {
namespace pvAccess {

typedef std::map<const short,epics::pvData::FieldConstPtr> registryMap_t;


	/**
	 * PVData Structure registry.
	 * Registry is used to cache introspection interfaces to minimize network traffic.
	 * @author gjansa
	 */
	class IntrospectionRegistry : public epics::pvData::NoDefaultMethods {
	public:
		IntrospectionRegistry(bool serverSide);
		virtual ~IntrospectionRegistry();

		void printKeysAndValues(std::string name);
		/**
		 * Resets registry, i.e. must be done when transport is changed (server restarted).
		 */
		void reset();
		/**
		 * Get introspection interface for given ID.
		 *
		 * @param id id of the introspection interface to get
		 *
		 * @return <code>Field</code> instance for given ID.
		 */
		epics::pvData::FieldConstPtr getIntrospectionInterface(const short id);

		/**
		 * Registers introspection interface with given ID. Always INCOMING.
		 *
		 * @param id id of the introspection interface to register
		 * @param field introspection interface to register
		 */
		void registerIntrospectionInterface(const short id, epics::pvData::FieldConstPtr field);

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
		short registerIntrospectionInterface(epics::pvData::FieldConstPtr field, bool& existing);

		/**
		 * Serializes introspection interface
		 *
		 * @param field
		 * @param buffer
		 * @param control
		 */
		void serialize(epics::pvData::FieldConstPtr field, epics::pvData::ByteBuffer* buffer, epics::pvData::SerializableControl* control);

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
		 * Serializes introspection interface. But this time really fully not like
		 * the serialize which only says it serializes but in fact does not. :)
		 *
		 * TODO
		 *
		 * @param field
		 * @param buffer
		 * @param control
		 */
		static void serializeFull(epics::pvData::FieldConstPtr field, epics::pvData::ByteBuffer* buffer, epics::pvData::SerializableControl* control);

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
		static epics::pvData::FieldConstPtr deserializeFull(epics::pvData::ByteBuffer* buffer, epics::pvData::DeserializableControl* control);

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


		static void serialize(epics::pvData::FieldConstPtr field, epics::pvData::StructureConstPtr parent, epics::pvData::ByteBuffer* buffer,
							  epics::pvData::SerializableControl* control, IntrospectionRegistry* registry);



		/**
		 * @param buffer
		 * @param control
		 * @param registry
		 * @param structure
		 */
		static epics::pvData::FieldConstPtr deserialize(epics::pvData::ByteBuffer* buffer, epics::pvData::DeserializableControl* control, IntrospectionRegistry* registry);

		/**
		 * Serialize optional PVStructrue.
		 * @param buffer data buffer.
		 */
		void serializeStructure(epics::pvData::ByteBuffer* buffer, epics::pvData::SerializableControl* control, epics::pvData::PVStructurePtr pvStructure);

		/**
		 * Deserialize optional PVStructrue.
		 * @param payloadBuffer data buffer.
		 * @return deserialized PVStructure, can be <code>null</code>.
		 */
		epics::pvData::PVStructurePtr deserializeStructure(epics::pvData::ByteBuffer* payloadBuffer, epics::pvData::DeserializableControl* control);

		/**
		 * Serialize PVRequest.
		 * @param buffer data buffer.
		 */
		void serializePVRequest(epics::pvData::ByteBuffer* buffer, epics::pvData::SerializableControl* control, epics::pvData::PVStructurePtr pvRequest);

		/**
		 * Deserialize PVRequest.
		 * @param payloadBuffer data buffer.
		 * @param control serialization control.
		 *
		 * @return deserialized PVRequest, can be <code>null</code>.
		 */
		epics::pvData::PVStructurePtr deserializePVRequest(epics::pvData::ByteBuffer* payloadBuffer, epics::pvData::DeserializableControl* control);

		/**
		 * Deserialize Structure and create PVStructure instance.
		 *
		 * @param payloadBuffer data buffer.
		 * @param control serialization control.
		 *
		 * @return PVStructure instance, can be <code>null</code>.
		 */
		epics::pvData::PVStructurePtr deserializeStructureAndCreatePVStructure(epics::pvData::ByteBuffer* payloadBuffer, epics::pvData::DeserializableControl* control);

		/**
		 * Serialize status.
		 *
		 * @param buffer data buffer.
		 * @param control serialization control.
		 * @param status status to serialize.
		 */
		void serializeStatus(epics::pvData::ByteBuffer* buffer, epics::pvData::SerializableControl* control, const epics::pvData::Status &status);

		/**
		 * Deserialize status.
		 *
		 * @param buffer data buffer.
		 * @param control serialization control.
		 */
		void deserializeStatus(epics::pvData::Status &status, epics::pvData::ByteBuffer* buffer, epics::pvData::DeserializableControl* control);

	private:
		registryMap_t _registry;
		registryMap_t::iterator _registryIter;
		registryMap_t::reverse_iterator _registryRIter;
		short _outgoingIdPointer;
		short _direction;
		epics::pvData::Mutex _mutex;

		/**
		 * PVField factory.
		 */
		static epics::pvData::PVDataCreatePtr _pvDataCreate;

		/**
		 * Field factory.
		 */
		static epics::pvData::FieldCreatePtr _fieldCreate;

		bool registryContainsValue(epics::pvData::FieldConstPtr field, short& key);
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
