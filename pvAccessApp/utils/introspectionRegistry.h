/*
 * introspectionRegistry.h
 */

#ifndef INTROSPECTIONREGISTRY_H
#define INTROSPECTIONREGISTRY_H

#include <lock.h>
#include <pvIntrospect.h>
#include <pvData.h>
#include <byteBuffer.h>
#include <serialize.h>
#include <serializeHelper.h>
#include <status.h>
#include <standardField.h>

#include <epicsMutex.h>

#include <map>
#include <iostream>


using namespace epics::pvData;
using namespace std;

namespace epics { namespace pvAccess {

typedef std::map<const short,const Field*> registryMap_t;


	/**
	 * PVData Structure registry.
	 * Registry is used to cache introspection interfaces to minimize network traffic.
	 * @author gjansa
	 */
	class IntrospectionRegistry {
	public:
		IntrospectionRegistry(bool serverSide);
		virtual ~IntrospectionRegistry();
		void printKeysAndValues(string name);
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
		FieldConstPtr getIntrospectionInterface(const short id);

		/**
		 * Registers introspection interface with given ID. Always INCOMING.
		 *
		 * @param id id of the introspection interface to register
		 * @param field introspection interface to register
		 */
		void registerIntrospectionInterface(const short id, FieldConstPtr field);

		/**
		 * Registers introspection interface and get it's ID. Always OUTGOING.
		 * If it is already registered only preassigned ID is returned.
		 * @param field introspection interface to register
		 *
		 * @return id of given introspection interface
		 */
		short registerIntrospectionInterface(FieldConstPtr field, bool& existing);

		/**
		 * Serializes introspection interface
		 *
		 * @param field
		 * @param buffer
		 * @param control
		 */
		void serialize(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control);

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
		FieldConstPtr deserialize(ByteBuffer* buffer, DeserializableControl* control);

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
		static void serializeFull(FieldConstPtr field, ByteBuffer* buffer, SerializableControl* control);

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
		static FieldConstPtr deserializeFull(ByteBuffer* buffer, DeserializableControl* control);

		/**
		 * Null type.
		 */
		const static int8 NULL_TYPE_CODE;

		/**
		 * Serialization contains only an ID (that was assigned by one of the previous <code>FULL_WITH_ID</code> descriptions).
		 */
		const static int8 ONLY_ID_TYPE_CODE;

		/**
		 * Serialization contains an ID (that can be used later, if cached) and full interface description.
		 */
		const static int8 FULL_WITH_ID_TYPE_CODE;


		static void serialize(FieldConstPtr field, StructureConstPtr parent, ByteBuffer* buffer,
							  SerializableControl* control, IntrospectionRegistry* registry);


		/**
		 * @param buffer
		 * @param control
		 * @param registry
		 * @param structure
		 */
		static void serializeStructureField(ByteBuffer* buffer, SerializableControl* control,
							IntrospectionRegistry* registry, StructureConstPtr structure);

		/**
		 * @param buffer
		 * @param control
		 * @param registry
		 * @param structure
		 */
		static FieldConstPtr deserialize(ByteBuffer* buffer, DeserializableControl* control, IntrospectionRegistry* registry);

		/**
		 * Deserialize Structure.
		 * @param buffer
		 * @param control
		 * @param registry
		 * @return deserialized Structure instance.
		 */
		static StructureConstPtr deserializeStructureField(ByteBuffer* buffer, DeserializableControl* control, IntrospectionRegistry* registry);

		/**
		 * Serialize optional PVStructrue.
		 * @param buffer data buffer.
		 */
		void serializeStructure(ByteBuffer* buffer, SerializableControl* control, PVStructurePtr pvStructure);

		/**
		 * Deserialize optional PVStructrue.
		 * @param payloadBuffer data buffer.
		 * @return deserialized PVStructure, can be <code>null</code>.
		 */
		PVStructurePtr deserializeStructure(ByteBuffer* payloadBuffer, DeserializableControl* control);

		/**
		 * Serialize PVRequest.
		 * @param buffer data buffer.
		 */
		void serializePVRequest(ByteBuffer* buffer, SerializableControl* control, PVStructurePtr pvRequest);

		/**
		 * Deserialize PVRequest.
		 * @param payloadBuffer data buffer.
		 * @param control
		 *
		 * @return deserialized PVRequest, can be <code>null</code>.
		 */
		PVStructurePtr deserializePVRequest(ByteBuffer* payloadBuffer, DeserializableControl* control);

		/**
		 * Deserialize Structure and create PVStructure instance.
		 * @param payloadBuffer data buffer.
		 * @return PVStructure instance, can be <code>null</code>.
		 */
		PVStructurePtr deserializeStructureAndCreatePVStructure(ByteBuffer* payloadBuffer, DeserializableControl* control);

		/**
		 * Serialize status.
		 * TODO optimize duplicates
		 * @param buffer data buffer.
		 * @param control serializaiton control instance.
		 * @param status status to serialize.
		 */
		void serializeStatus(ByteBuffer* buffer, SerializableControl* control, Status* status);

		/**
		 * Serialize status.
		 * TODO optimize duplicates
		 * @param buffer data buffer.
		 */
		Status* deserializeStatus(ByteBuffer* buffer, DeserializableControl* control);

	private:
		registryMap_t _registry;
		registryMap_t::iterator _registryIter;
		registryMap_t::reverse_iterator _registryRIter;
		short _outgoingIdPointer;
		short _direction;
		Mutex _mutex;

		/**
		 * PVField factory.
		 */
		static PVDataCreate* _pvDataCreate;

		/**
		 * Status factory.
		 */
		static StatusCreate* _statusCreate;

		/**
		 * Field factory.
		 */
		static FieldCreate* _fieldCreate;

		bool registryContainsValue(FieldConstPtr field, short& key);
		bool compareFields(FieldConstPtr field1, FieldConstPtr field2);
		static void checkBufferAndSerializeControl(ByteBuffer* buffer, SerializableControl* control);
		static void checkBufferAndDeserializeControl(ByteBuffer* buffer, DeserializableControl* control);
	};

}}

#endif  /* INTROSPECTIONREGISTRY_H */
