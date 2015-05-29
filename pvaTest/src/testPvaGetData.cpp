/*testPvaGetData.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 */

/* Author: Marty Kraimer */

#include <iostream>

#include <epicsUnitTest.h>
#include <testMain.h>

#include <pv/pva.h>
#include <pv/bitSet.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;

static PvaPtr pva = Pva::create();
static FieldCreatePtr fieldCreate = getFieldCreate();
static StandardFieldPtr standardField = getStandardField();
static PVDataCreatePtr pvDataCreate = getPVDataCreate();


void testDouble()
{
    cout << "\nstarting testDouble\n";
    StructureConstPtr structure =
       fieldCreate->createFieldBuilder()->
            add("alarm",standardField->alarm()) ->
            add("timeStamp",standardField->timeStamp()) ->
            add("value",pvDouble) ->
            createStructure();

    PvaGetDataPtr pvaData = PvaGetData::create(structure);
    PVStructurePtr pvStructure = pvDataCreate->createPVStructure(pvaData->getStructure());
    BitSetPtr bitSet = BitSetPtr(new BitSet(pvStructure->getNumberFields()));
    pvaData->setData(pvStructure,bitSet);
    PVDoublePtr pvDouble = pvStructure->getSubField<PVDouble>("value");
    size_t valueOffset = pvDouble->getFieldOffset();
    BitSetPtr change = pvaData->getBitSet();
    pvDouble->put(5.0);
    change->set(pvDouble->getFieldOffset());
    testOk(change->cardinality()==1,"num set bits 1");
    testOk(change->get(valueOffset)==true,"value changed");
    testOk(pvaData->hasValue()==true,"hasValue");
    testOk(pvaData->isValueScalar()==true,"isValueScalar");
    testOk(pvaData->isValueScalarArray()==false,"isValueScalarArray");
    bool result;
    result = false;
    if(pvaData->getValue()) result = true;
    testOk(result==true,"getValue");
    result = false;
    if(pvaData->getScalarValue()) result = true;
    testOk(result==true,"getScalarValue");
    try {
        pvaData->getArrayValue();
    } catch (std::runtime_error e) {
        cout << "getArrayValue " << e.what() << endl;
    }
    try {
        pvaData->getScalarArrayValue();
    } catch (std::runtime_error e) {
        cout << " getScalarArrayValue " << e.what() << endl;
    }
    cout << "as double " << pvaData->getDouble() << endl;
    cout << "as string " << pvaData->getString() << endl;
    try {
        shared_vector<const double> value = pvaData->getDoubleArray();
    } catch (std::runtime_error e) {
        cout << " getDoubleArray " << e.what() << endl;
    }
    try {
        shared_vector<const string> value = pvaData->getStringArray();
    } catch (std::runtime_error e) {
        cout << " getStringArray " << e.what() << endl;
    }
}

void testDoubleArray()
{
    cout << "\nstarting testDoubleArray\n";
    StructureConstPtr structure =
       fieldCreate->createFieldBuilder()->
            add("alarm",standardField->alarm()) ->
            add("timeStamp",standardField->timeStamp()) ->
            addArray("value",pvDouble) ->
            createStructure();

    PvaGetDataPtr pvaData = PvaGetData::create(structure);
    PVStructurePtr pvStructure = pvDataCreate->createPVStructure(pvaData->getStructure());
    BitSetPtr bitSet = BitSetPtr(new BitSet(pvStructure->getNumberFields()));
    pvaData->setData(pvStructure,bitSet);
    PVDoubleArrayPtr pvalue = pvaData->getPVStructure()->getSubField<PVDoubleArray>("value");
    BitSetPtr change = pvaData->getBitSet();
    size_t valueOffset = pvalue->getFieldOffset();
    size_t len = 5;
    shared_vector<double> value(len);
    for(size_t i=0; i<len; ++i) value[i] = i*10.0;
    pvalue->replace(freeze(value));
    change->set(valueOffset);
    testOk(change->cardinality()==1,"num set bits 1");
    testOk(change->get(valueOffset)==true,"value changed");
    testOk(pvaData->hasValue()==true,"hasValue");
    testOk(pvaData->isValueScalar()==false,"isValueScalar");
    testOk(pvaData->isValueScalarArray()==true,"isValueScalarArray");
    bool result;
    result = false;
    if(pvaData->getValue()) result = true;
    testOk(result==true,"getValue");
    result = false;
    if(pvaData->getArrayValue()) result = true;
    testOk(result==true,"getArrayValue");
    result = false;
    if(pvaData->getScalarArrayValue()) result = true;
    testOk(result==true,"getScalarValue");
    try {
        pvaData->getScalarValue();
    } catch (std::runtime_error e) {
        cout << " getScalarValue " << e.what() << endl;
    }
    try {
        cout << "as double " << pvaData->getDouble() << endl;
    } catch (std::runtime_error e) {
        cout << " getDouble " << e.what() << endl;
    }
    try {
        string val = pvaData->getString();
    } catch (std::runtime_error e) {
        cout << " getString " << e.what() << endl;
    }
    cout << "as doubleArray " << pvaData->getDoubleArray() << endl;
    try {
        shared_vector<const string> value = pvaData->getStringArray();
    } catch (std::runtime_error e) {
        cout << " getStringArray " << e.what() << endl;
    }
}

MAIN(testPvaGetData)
{
    cout << "\nstarting testPvaGetData\n";
    testPlan(15);
    testDouble();
    testDoubleArray();
    return 0;
}

