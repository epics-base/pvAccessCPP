/* pvaGetData.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.02
 */
#define epicsExportSharedSymbols

#include <typeinfo>

#include <sstream>
#include <pv/pva.h>
#include <pv/createRequest.h>
#include <pv/convert.h>


using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

namespace epics { namespace pva {


typedef std::tr1::shared_ptr<PVArray> PVArrayPtr;
static ConvertPtr convert = getConvert();
static string noStructure("no pvStructure ");
static string noValue("no value field");
static string noScalar("value is not a scalar");
static string notCompatibleScalar("value is not a compatible scalar");
static string noArray("value is not an array");
static string noScalarArray("value is not a scalarArray");
static string notDoubleArray("value is not a doubleArray");
static string notStringArray("value is not a stringArray");

PvaGetDataPtr PvaGetData::create(StructureConstPtr const & structure)
{
    PvaGetDataPtr epv(new PvaGetData(structure));
    return epv;
}

PvaGetData::PvaGetData(StructureConstPtr const & structure)
: structure(structure)
{}

void PvaGetData::checkValue()
{
    if(pvValue) return;
    throw std::runtime_error(messagePrefix + noValue);
}

void PvaGetData::setMessagePrefix(std::string const & value)
{
    messagePrefix = value + " ";
}

StructureConstPtr PvaGetData::getStructure()
{return structure;}

PVStructurePtr PvaGetData::getPVStructure()
{
    if(pvStructure) return pvStructure;
    throw std::runtime_error(messagePrefix + noStructure);
}

BitSetPtr PvaGetData::getBitSet()
{
    if(bitSet)return bitSet;
    throw std::runtime_error(messagePrefix + noStructure);
}

std::ostream & PvaGetData::showChanged(std::ostream & out)
{
    if(!bitSet) throw std::runtime_error(messagePrefix + noStructure);
    size_t nextSet = bitSet->nextSetBit(0);
    PVFieldPtr pvField;
    while(nextSet!=string::npos) {
        if(nextSet==0) {
             pvField = pvStructure;
        } else {
              pvField = pvStructure->getSubField(nextSet);
        }
        string name = pvField->getFullName();
        out << name << " = " << pvField << endl;
        nextSet = bitSet->nextSetBit(nextSet+1);
    }
    return out;
}

void PvaGetData::setData(
    PVStructurePtr const & pvStructureFrom,
    BitSetPtr const & bitSetFrom)
{
   pvStructure = pvStructureFrom;
   bitSet = bitSetFrom;
   pvValue = pvStructure->getSubField("value");
}

bool PvaGetData::hasValue()
{
    if(!pvValue) return false;
    return true;
}

bool PvaGetData::isValueScalar()
{
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalar) return true;
    return false;
}

bool PvaGetData::isValueScalarArray()
{
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalarArray) return true;
    return false;
}

PVFieldPtr  PvaGetData::getValue()
{
   checkValue();
   return pvValue;
}

PVScalarPtr  PvaGetData::getScalarValue()
{
    checkValue();
    PVScalarPtr pv = pvStructure->getSubField<PVScalar>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + noScalar);
    }
    return pv;
}

PVArrayPtr  PvaGetData::getArrayValue()
{
    checkValue();
    PVArrayPtr pv = pvStructure->getSubField<PVArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + noArray);
    }
    return pv;
}

PVScalarArrayPtr  PvaGetData::getScalarArrayValue()
{
    checkValue();
    PVScalarArrayPtr pv = pvStructure->getSubField<PVScalarArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + noScalarArray);
    }
    return pv;
}

double PvaGetData::getDouble()
{
    PVScalarPtr pvScalar = getScalarValue();
    ScalarType scalarType = pvScalar->getScalar()->getScalarType();
    if(scalarType==pvDouble) {
        PVDoublePtr pvDouble = static_pointer_cast<PVDouble>(pvScalar);
        return pvDouble->get();
    }
    if(!ScalarTypeFunc::isNumeric(scalarType)) {
        throw std::runtime_error(messagePrefix + notCompatibleScalar);
    }
    return convert->toDouble(pvScalar);
}

string PvaGetData::getString()
{
    PVScalarPtr pvScalar = getScalarValue();
    return convert->toString(pvScalar);
}

shared_vector<const double> PvaGetData::getDoubleArray()
{
    checkValue();
    PVDoubleArrayPtr pv = pvStructure->getSubField<PVDoubleArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notDoubleArray);
    }
    return pv->view();   
}

shared_vector<const string> PvaGetData::getStringArray()
{
    checkValue();
    PVStringArrayPtr pv = pvStructure->getSubField<PVStringArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notStringArray);
    }
    return pv->view();   

}

}}
