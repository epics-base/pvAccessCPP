/*
 * introspectionRegistryTest.cpp
 *
 *  Created on: Dec 23, 2010
 *      Author: Gasper Jansa
 */

#include <pv/introspectionRegistry.h>

#include <epicsAssert.h>
#include <epicsExit.h>
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <pv/CDRMonitor.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;

namespace epics {
namespace pvAccess {

class SerializableControlImpl : public SerializableControl,
    public NoDefaultMethods {
public:
    virtual void flushSerializeBuffer() {
    }

    virtual void ensureBuffer(int size) {
    }

    virtual void alignBuffer(int alignment) {
    }

    SerializableControlImpl() {
    }

    virtual ~SerializableControlImpl() {
    }
};

class DeserializableControlImpl : public DeserializableControl,
    public NoDefaultMethods {
public:
    virtual void ensureData(int size) {
    }

    virtual void alignData(int alignment) {
    }

    DeserializableControlImpl() {
    }

    virtual ~DeserializableControlImpl() {
    }
};

}
}

using namespace epics::pvAccess;
using namespace std;

static SerializableControl* flusher;
static DeserializableControl* control;
static ByteBuffer* buffer;

static PVDataCreate* pvDataCreate;
static FieldCreate* fieldCreate;
static StandardField *standardField;

static IntrospectionRegistry* registry;
static IntrospectionRegistry* clientRegistry;
static IntrospectionRegistry* serverRegistry;

vector<PVField*> pvFieldArray;

static string builder;

//helper methods
ScalarConstPtr getScalar(string name)
{
    ScalarConstPtr scalar =  standardField->scalar(name,pvFloat);
    PVField *pvField = pvDataCreate->createPVField(0,scalar);
    pvFieldArray.push_back(pvField);
    return scalar;
}

ScalarArrayConstPtr getScalarArray(string name)
{
    ScalarArrayConstPtr scalarArray =  standardField->scalarArray(name,pvFloat);
    PVField *pvField = pvDataCreate->createPVField(0,scalarArray);
    pvFieldArray.push_back(pvField);
    return scalarArray;
}

StructureConstPtr getStructure(string name)
{
    String properties("alarm");
    FieldConstPtrArray powerSupply = new FieldConstPtr[3];
    powerSupply[0] = standardField->scalar(
                         String("voltage"),pvDouble,properties);
    powerSupply[1] = standardField->scalar(
                         String("power"),pvDouble,properties);
    powerSupply[2] = standardField->scalar(
                         String("current"),pvDouble,properties);
    StructureConstPtr structure =  standardField->structure(name,3,powerSupply);
    PVField *pvField = pvDataCreate->createPVField(0,structure);
    pvFieldArray.push_back(pvField);
    return structure;
}

StructureArrayConstPtr getStructureArray(string name1, string name2)
{
    String properties("alarm");
    FieldConstPtrArray powerSupply = new FieldConstPtr[3];
    powerSupply[0] = standardField->scalar(
                         String("voltage"),pvDouble,properties);
    powerSupply[1] = standardField->scalar(
                         String("power"),pvDouble,properties);
    powerSupply[2] = standardField->scalar(
                         String("current"),pvDouble,properties);
    StructureConstPtr structure =  standardField->structure(name1,3,powerSupply);
    StructureArrayConstPtr structureArray = standardField->structureArray(name2,structure);
    PVField *pvField = pvDataCreate->createPVField(0,structureArray);
    pvFieldArray.push_back(pvField);
    return structureArray;
}

UnionConstPtr getUnion(string name)
{
    String properties("alarm");
    FieldConstPtrArray powerSupply = new FieldConstPtr[3];
    powerSupply[0] = standardField->scalar(
                         String("voltage"),pvDouble,properties);
    powerSupply[1] = standardField->scalar(
                         String("power"),pvDouble,properties);
    powerSupply[2] = standardField->scalar(
                         String("current"),pvDouble,properties);
    UnionConstPtr punion =  getFieldCreate()->createUnion(name,3,powerSupply);
    PVField *pvField = pvDataCreate->createPVField(0,punion);
    pvFieldArray.push_back(pvField);
    return punion;
}

UnionArrayConstPtr getUnionArray(string name1, string name2)
{
    String properties("alarm");
    FieldConstPtrArray powerSupply = new FieldConstPtr[3];
    powerSupply[0] = standardField->scalar(
                         String("voltage"),pvDouble,properties);
    powerSupply[1] = standardField->scalar(
                         String("power"),pvDouble,properties);
    powerSupply[2] = standardField->scalar(
                         String("current"),pvDouble,properties);
    UnionConstPtr punion =  getFieldCreate()->createUnion(name1,3,powerSupply);
    UnionArrayConstPtr unionArray = standardField->unionArray(name2,punion);
    PVField *pvField = pvDataCreate->createPVField(0,unionArray);
    pvFieldArray.push_back(pvField);
    return unionArray;
}

//test methods
void testRegistryPutGet()
{
    short id = 0;
    ScalarConstPtr scalarIn = getScalar("field1");
    registry->registerIntrospectionInterface(id,scalarIn);

    id++;
    ScalarArrayConstPtr scalarArrayIn = getScalarArray("fieldArray1");
    registry->registerIntrospectionInterface(id,scalarArrayIn);

    id++;
    StructureConstPtr structureIn = getStructure("struct1");
    registry->registerIntrospectionInterface(id,structureIn);

    id++;
    StructureArrayConstPtr structureArrayIn = getStructureArray("struct1","structArray1");
    registry->registerIntrospectionInterface(id,structureArrayIn);

    id = 0;
    ScalarConstPtr scalarOut = static_pointer_cast<const Scalar>(registry->getIntrospectionInterface(id));
    assert(scalarIn == scalarOut);

    id++;
    ScalarArrayConstPtr scalarArrayOut = static_pointer_cast<const ScalarArray>(registry->getIntrospectionInterface(id));
    assert(scalarArrayIn == scalarArrayOut);

    id++;
    StructureConstPtr structureOut = static_pointer_cast<const Structure>(registry->getIntrospectionInterface(id));
    assert(structureIn == structureOut);

    id++;
    StructureArrayConstPtr structureArrayOut = static_pointer_cast<const StructureArray>(registry->getIntrospectionInterface(id));
    assert(structureArrayIn == structureArrayOut);

    bool existing;
    id = registry->registerIntrospectionInterface(static_pointer_cast<const Field>(scalarIn),existing);
    assert(existing == true);
    assert(id == 0);

    id = registry->registerIntrospectionInterface(static_pointer_cast<const Field>(scalarArrayIn),existing);
    assert(existing == true);
    assert(id == 1);

    id = registry->registerIntrospectionInterface(static_pointer_cast<const Field>(structureIn),existing);
    assert(existing == true);
    assert(id == 2);

    id = registry->registerIntrospectionInterface(static_pointer_cast<const Field>(structureArrayIn),existing);
    assert(existing == true);
    assert(id == 3);

    //should exist
    ScalarConstPtr scalarInNew = getScalar("field1");
    id = registry->registerIntrospectionInterface(static_pointer_cast<const Field>(scalarInNew),existing);
    assert(existing == true);
    assert(id == 0);

    scalarOut = static_pointer_cast<const Scalar>(registry->getIntrospectionInterface(id));
    assert(scalarIn == scalarOut);
}

void testRegistryReset()
{
    registry->reset();

    short id = 0;
    assert(static_pointer_cast<const Scalar>(registry->getIntrospectionInterface(id)) == 0);
}

void serialize(FieldConstPtr field, IntrospectionRegistry* registry)
{
    buffer->clear();
    registry->serialize(field,buffer,flusher);
    //should be in registry
    bool existing;
    registry->registerIntrospectionInterface(field,existing);
    assert(existing == true);
}

FieldConstPtr deserialize(IntrospectionRegistry* registry)
{
    FieldConstPtr field = registry->deserialize(buffer,control);
    PVField *pvField = pvDataCreate->createPVField(0,field);
    pvFieldArray.push_back(pvField);
    //should be in registry
    bool existing;
    registry->registerIntrospectionInterface(field,existing);
    assert(existing == true);
    return field;
}

void checkTypeCode(int8 typeCodeIn)
{
    int8 typeCode = buffer->getByte();
//printf("%d == %d\n", typeCode, typeCodeIn);
    assert(typeCode == typeCodeIn);
    buffer->rewind();
}

void testSerializeCommon(FieldConstPtr serverField1, FieldConstPtr clientField2)
{
    //server serializes field 1
    serialize(serverField1,serverRegistry);

    //full should be serialized
    buffer->flip();
    checkTypeCode(IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE);

    //client deserializes 1
    FieldConstPtr clientField1 = deserialize(clientRegistry);
    assert(serverField1->getFieldName() == clientField1->getFieldName());
    assert(serverField1->getType() == clientField1->getType());

    //client serializes the same field
    serialize(serverField1,clientRegistry);

    //only id should be serialized
    buffer->flip();
    checkTypeCode(IntrospectionRegistry::ONLY_ID_TYPE_CODE);

    //server deserializes the same field
    serverField1 = deserialize(serverRegistry);
    assert(serverField1->getFieldName() == clientField1->getFieldName());
    assert(serverField1->getType() == clientField1->getType());

    //client requests new field
    serialize(clientField2,clientRegistry);

    //full should be serialized
    buffer->flip();
    checkTypeCode(IntrospectionRegistry::FULL_WITH_ID_TYPE_CODE);

    //server deserializes new field
    FieldConstPtr serverField2 = deserialize(serverRegistry);
    assert(serverField2->getFieldName() == clientField2->getFieldName());
    assert(serverField2->getType() == clientField2->getType());
}

void testSerialize()
{
    clientRegistry->reset();
    serverRegistry->reset();
    stringstream ss;
    string name1,name2,name3,name4;

    for(int i = 0, j = 0; i < 10 ; i++, j++)
    {
        name1.clear();
        name2.clear();
        ss.str("");
        ss << j;
        name1 = "field" + ss.str();
        ss.str("");
        j++;
        ss << j;
        name2 = "field" + ss.str();
        testSerializeCommon(static_pointer_cast<const Field>(getScalar(name1)),static_pointer_cast<const Field>(getScalar(name2)));

        name1.clear();
        name2.clear();
        ss.str("");
        ss << j;
        name1 = "fieldArray" + ss.str();
        ss.str("");
        j++;
        ss << j;
        name2 = "fieldArray" + ss.str();
        testSerializeCommon(static_pointer_cast<const Field>(getScalarArray(name1)),static_pointer_cast<const Field>(getScalarArray(name2)));

        name1.clear();
        name2.clear();
        ss.str("");
        ss << j;
        name1 = "structure" + ss.str();
        ss.str("");
        j++;
        ss << j;
        name2 = "structure" + ss.str();
        testSerializeCommon(static_pointer_cast<const Field>(getStructure(name1)),static_pointer_cast<const Field>(getStructure(name2)));

        name1.clear();
        name2.clear();
        name3.clear();
        name4.clear();
        ss.str("");
        ss << j;
        name1 = "structure" + ss.str();
        name2 = "structureArray" + ss.str();
        ss.str("");
        j++;
        ss << j;
        name3 = "structure" + ss.str();
        name4 = "structureArray" + ss.str();
        testSerializeCommon(static_pointer_cast<const Field>(getStructureArray(name1,name2)),static_pointer_cast<const Field>(getStructureArray(name3,name4)));

        name1.clear();
        name2.clear();
        ss.str("");
        ss << j;
        name1 = "union" + ss.str();
        ss.str("");
        j++;
        ss << j;
        name2 = "union" + ss.str();
        testSerializeCommon(static_pointer_cast<const Field>(getUnion(name1)),static_pointer_cast<const Field>(getUnion(name2)));

        name1.clear();
        name2.clear();
        name3.clear();
        name4.clear();
        ss.str("");
        ss << j;
        name1 = "union" + ss.str();
        name2 = "unionArray" + ss.str();
        ss.str("");
        j++;
        ss << j;
        name3 = "union" + ss.str();
        name4 = "unionArray" + ss.str();
        testSerializeCommon(static_pointer_cast<const Field>(getUnionArray(name1,name2)),static_pointer_cast<const Field>(getUnionArray(name3,name4)));

    }

    //serverRegistry->printKeysAndValues("server");
    //clientRegistry->printKeysAndValues("client");
}

void testSerializeFull()
{
    buffer->clear();
    ScalarConstPtr scalarIn = getScalar("field1");
    IntrospectionRegistry::serializeFull(static_pointer_cast<const Field>(scalarIn),buffer,flusher);
    buffer->flip();
    ScalarConstPtr scalarOut = static_pointer_cast<const Scalar>(IntrospectionRegistry::deserializeFull(buffer,control));
    PVField *pvField = pvDataCreate->createPVField(0,scalarOut);
    pvFieldArray.push_back(pvField);
    assert(scalarIn->getFieldName() == scalarOut->getFieldName());
    assert(scalarIn->getType() == scalarOut->getType());

    buffer->clear();
    ScalarArrayConstPtr scalarArrayIn = getScalarArray("fieldArray1");
    IntrospectionRegistry::serializeFull(static_pointer_cast<const Field>(scalarArrayIn),buffer,flusher);
    buffer->flip();
    ScalarArrayConstPtr scalarArrayOut = static_pointer_cast<const ScalarArray>(IntrospectionRegistry::deserializeFull(buffer,control));
    pvField = pvDataCreate->createPVField(0,scalarArrayOut);
    pvFieldArray.push_back(pvField);
    assert(scalarArrayIn->getFieldName() == scalarArrayOut->getFieldName());
    assert(scalarArrayIn->getType() == scalarArrayOut->getType());

    buffer->clear();
    StructureConstPtr structureIn = getStructure("struct1");
    IntrospectionRegistry::serializeFull(static_pointer_cast<const Field>(structureIn),buffer,flusher);
    buffer->flip();
    StructureConstPtr structureOut = static_pointer_cast<const Structure>(IntrospectionRegistry::deserializeFull(buffer,control));
    pvField = pvDataCreate->createPVField(0,structureOut);
    pvFieldArray.push_back(pvField);
    assert(structureIn->getFieldName() == structureOut->getFieldName());
    assert(structureIn->getType() == structureOut->getType());

    buffer->clear();
    StructureArrayConstPtr structureArrayIn = getStructureArray("struct1","structArray1");
    IntrospectionRegistry::serializeFull(static_pointer_cast<const Field>(structureArrayIn),buffer,flusher);
    buffer->flip();
    StructureArrayConstPtr structureArrayOut = static_pointer_cast<const StructureArray>(IntrospectionRegistry::deserializeFull(buffer,control));
    pvField = pvDataCreate->createPVField(0,structureArrayOut);
    pvFieldArray.push_back(pvField);
    assert(structureArrayIn->getFieldName() == structureArrayOut->getFieldName());
    assert(structureArrayIn->getType() == structureArrayOut->getType());
}

void testSerializePVRequest()
{
    buffer->clear();
    registry->reset();
    PVStructurePtr pvStructureIn = pvDataCreate->createPVStructure(NULL,getStructure("structure1"));
    registry->serializePVRequest(buffer,flusher,pvStructureIn);

    buffer->flip();
    PVStructurePtr pvStructureOut = registry->deserializePVRequest(buffer,control);
    assert(*pvStructureIn == *pvStructureOut);
    delete pvStructureIn;
    delete pvStructureOut;
}

void testDeserializeStructureAndCreatePVStructure()
{
    buffer->clear();
    registry->reset();
    StructureConstPtr structureIn = getStructure("structure1");
    serialize(structureIn,registry);

    buffer->flip();
    PVStructurePtr pvStructureOut = registry->deserializeStructureAndCreatePVStructure(buffer,control);
    StructureConstPtr structureOut = pvStructureOut->getStructure();
    assert(structureIn->getFieldName() == structureOut->getFieldName());
    assert(structureIn->getType() == structureOut->getType());
    delete pvStructureOut;
}

void testSerializeStatus()
{
    buffer->clear();
    registry->reset();
    Status statusIn(Status::STATUSTYPE_WARNING, "msg", "dumpy");
    registry->serializeStatus(buffer,flusher,statusIn);

    buffer->flip();
    Status statusOut;
    registry->deserializeStatus(statusOut, buffer,control);
    assert(statusIn.getType() == statusOut.getType());
    assert(statusIn.getMessage() == statusOut.getMessage());
    assert(statusIn.getStackDump() == statusOut.getStackDump());
}

int main(int argc, char *argv[])
{
    pvDataCreate = getPVDataCreate();
    fieldCreate = getFieldCreate();
    standardField = getStandardField();


    flusher = new SerializableControlImpl();
    control = new DeserializableControlImpl();
    buffer = new ByteBuffer(1<<16);
    registry = new IntrospectionRegistry(true);
    clientRegistry = new IntrospectionRegistry(false);
    serverRegistry = new IntrospectionRegistry(true);

    testRegistryPutGet();
    testRegistryReset();
    testSerialize();
    testSerializeFull();
    testDeserializeStructureAndCreatePVStructure();
    testSerializeStatus();

    registry->reset();
    clientRegistry->reset();
    serverRegistry->reset();
    for (unsigned int i=0; i < pvFieldArray.size(); i++)
    {
        delete pvFieldArray[i];
    }
    pvFieldArray.clear();

    if(flusher) delete flusher;
    if(control) delete control;
    if(buffer) delete buffer;
    if(registry) delete registry;
    if(clientRegistry) delete clientRegistry;
    if(serverRegistry) delete serverRegistry;

    epicsExitCallAtExits();
    CDRMonitor::get().show(stdout, true);
    cout << "DONE" << endl;
    return 0;
}
