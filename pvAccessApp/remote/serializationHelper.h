/*
 * serializationHelper.h
 *
 *  Created on: Jul 24, 2012
 *      Author: msekoranja
 */

#ifndef SERIALIZATIONHELPER_H_
#define SERIALIZATIONHELPER_H_

#include <pv/caConstants.h>

#include <pv/serialize.h>
#include <pv/pvData.h>
#include <pv/noDefaultMethods.h>
#include <pv/pvIntrospect.h>
#include <pv/byteBuffer.h>

#include <pv/pvAccess.h>


namespace epics {
    namespace pvData {

        class SerializationHelper : public NoDefaultMethods {
        public:

    		static epics::pvData::PVDataCreatePtr _pvDataCreate;

			/**
			 * Deserialize PVRequest.
			 * @param payloadBuffer data buffer.
			 * @return deserialized PVRequest, can be <code>null</code>.
			 */
			static PVStructure::shared_pointer deserializePVRequest(ByteBuffer* payloadBuffer, DeserializableControl* control);

			/**
			 * Deserialize Structure and create PVStructure instance.
			 * @param payloadBuffer data buffer.
			 * @param control deserialization control.
			 * @return PVStructure instance, can be <code>null</code>.
			 */
			static PVStructure::shared_pointer deserializeStructureAndCreatePVStructure(ByteBuffer* payloadBuffer, DeserializableControl* control);

			/**
			 * Deserialize Structure and create PVStructure instance, if necessary.
			 * @param payloadBuffer data buffer.
			 * @param control deserialization control.
			 * @param existingStructure if deserialized Field matches <code>existingStrcuture</code> Field, then
			 * 			<code>existingStructure</code> instance is returned. <code>null</code> value is allowed.
			 * @return PVStructure instance, can be <code>null</code>.
			 */
			static PVStructure deserializeStructureAndCreatePVStructure(ByteBuffer* payloadBuffer, DeserializableControl* control, PVStructure::shared_pointer const & existingStructure);

			/**
			 * Deserialize optional PVStructrue.
			 * @param payloadBuffer data buffer.
			 * @return deserialized PVStructure, can be <code>null</code>.
			 */
			static PVStructure::shared_pointer deserializeStructureFull(ByteBuffer* payloadBuffer, DeserializableControl* control);

			static void serializeNullField(ByteBuffer* buffer, SerializableControl* control);

			/**
			 * Serialize PVRequest.
			 * @param buffer data buffer.
			 */
			static void serializePVRequest(ByteBuffer* buffer, SerializableControl* control, PVStructure::shared_pointer const & pvRequest);

			/**
			 * Serialize optional PVStructrue.
			 * @param buffer data buffer.
			 */
			static void serializeStructureFull(ByteBuffer* buffer, SerializableControl* control, PVStructure::shared_pointer const & pvStructure);

};

}
}

#endif /* SERIALIZATIONHELPER_H_ */
