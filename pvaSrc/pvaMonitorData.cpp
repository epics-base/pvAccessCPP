/* pvaMonitorData.cpp */
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
static StructureConstPtr nullStructure;
static PVStructurePtr nullPVStructure;
static ConvertPtr convert = getConvert();
static string noStructure("no pvStructure ");
static string noValue("no value field");
static string noScalar("value is not a scalar");
static string notCompatibleScalar("value is not a compatible scalar");
static string noArray("value is not an array");
static string noScalarArray("value is not a scalarArray");
static string notDoubleArray("value is not a doubleArray");
static string notStringArray("value is not a stringArray");

PvaMonitorDataPtr PvaMonitorData::create(StructureConstPtr const & structure)
{
    PvaMonitorDataPtr epv(new PvaMonitorData(structure));
    return epv;
}

PvaMonitorData::PvaMonitorData(StructureConstPtr const & structure)
: structure(structure)
{}


void PvaMonitorData::checkValue()
{
    if(pvValue) return;
    throw std::runtime_error(messagePrefix + noValue);
}

void PvaMonitorData::setMessagePrefix(std::string const & value)
{
    messagePrefix = value + " ";
}

StructureConstPtr PvaMonitorData::getStructure()
{return structure;}

PVStructurePtr PvaMonitorData::getPVStructure()
{
    if(pvStructure) return pvStructure;
    throw std::runtime_error(messagePrefix + noStructure);
}

BitSetPtr PvaMonitorData::getChangedBitSet()
{
    if(!changedBitSet) throw std::runtime_error(messagePrefix + noStructure);
    return changedBitSet;
}

BitSetPtr PvaMonitorData::getOverrunBitSet()
{
    if(!overrunBitSet) throw std::runtime_error(messagePrefix + noStructure);
    return overrunBitSet;
}

std::ostream & PvaMonitorData::showChanged(std::ostream & out)
{
    if(!changedBitSet) throw std::runtime_error(messagePrefix + noStructure);
    size_t nextSet = changedBitSet->nextSetBit(0);
    PVFieldPtr pvField;
    while(nextSet!=string::npos) {
        if(nextSet==0) {
             pvField = pvStructure;
        } else {
              pvField = pvStructure->getSubField(nextSet);
        }
        string name = pvField->getFullName();
        out << name << " = " << pvField << endl;
        nextSet = changedBitSet->nextSetBit(nextSet+1);
    }
    return out;
}

std::ostream & PvaMonitorData::showOverrun(std::ostream & out)
{
    if(!overrunBitSet) throw std::runtime_error(messagePrefix + noStructure);
    size_t nextSet = overrunBitSet->nextSetBit(0);
    PVFieldPtr pvField;
    while(nextSet!=string::npos) {
        if(nextSet==0) {
             pvField = pvStructure;
        } else {
              pvField = pvStructure->getSubField(nextSet);
        }
        string name = pvField->getFullName();
        out << name << " = " << pvField << endl;
        nextSet = overrunBitSet->nextSetBit(nextSet+1);
    }
    return out;
}

void PvaMonitorData::setData(MonitorElementPtr const & monitorElement)
{
   pvStructure = monitorElement->pvStructurePtr;
   changedBitSet = monitorElement->changedBitSet;
   overrunBitSet = monitorElement->overrunBitSet;
   pvValue = pvStructure->getSubField("value");
}

bool PvaMonitorData::hasValue()
{
    if(!pvValue) return false;
    return true;
}

bool PvaMonitorData::isValueScalar()
{
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalar) return true;
    return false;
}

bool PvaMonitorData::isValueScalarArray()
{
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalarArray) return true;
    return false;
}

PVFieldPtr  PvaMonitorData::getValue()
{
   checkValue();
   return pvValue;
}

PVScalarPtr  PvaMonitorData::getScalarValue()
{
    checkValue();
    PVScalarPtr pv = pvStructure->getSubField<PVScalar>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + noScalar);
    }
    return pv;
}

PVArrayPtr  PvaMonitorData::getArrayValue()
{
    checkValue();
    PVArrayPtr pv = pvStructure->getSubField<PVArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + noArray);
    }
    return pv;
}

PVScalarArrayPtr  PvaMonitorData::getScalarArrayValue()
{
    checkValue();
    PVScalarArrayPtr pv = pvStructure->getSubField<PVScalarArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + noScalarArray);
    }
    return pv;
}

double PvaMonitorData::getDouble()
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

string PvaMonitorData::getString()
{
    PVScalarPtr pvScalar = getScalarValue();
    return convert->toString(pvScalar);
}

shared_vector<const double> PvaMonitorData::getDoubleArray()
{
    checkValue();
    PVDoubleArrayPtr pv = pvStructure->getSubField<PVDoubleArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notDoubleArray);
    }
    return pv->view();   
}

shared_vector<const string> PvaMonitorData::getStringArray()
{
    checkValue();
    PVStringArrayPtr pv = pvStructure->getSubField<PVStringArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notStringArray);
    }
    return pv->view();   

}

}}
