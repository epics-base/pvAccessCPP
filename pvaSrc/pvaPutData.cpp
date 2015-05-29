/* easyPutData.cpp */
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

class PvaPostHandlerPvt: public PostHandler
{
    PvaPutData * easyData;
    size_t fieldNumber;
public:
    PvaPostHandlerPvt(PvaPutData *easyData,size_t fieldNumber)
    : easyData(easyData),fieldNumber(fieldNumber){}
    void postPut() { easyData->postPut(fieldNumber);}
};


typedef std::tr1::shared_ptr<PVArray> PVArrayPtr;
static ConvertPtr convert = getConvert();
static string noValue("no value field");
static string notScalar("value is not a scalar");
static string notCompatibleScalar("value is not a compatible scalar");
static string notArray("value is not an array");
static string notScalarArray("value is not a scalarArray");
static string notDoubleArray("value is not a doubleArray");
static string notStringArray("value is not a stringArray");

PvaPutDataPtr PvaPutData::create(StructureConstPtr const & structure)
{
    PvaPutDataPtr epv(new PvaPutData(structure));
    return epv;
}

PvaPutData::PvaPutData(StructureConstPtr const & structure)
: structure(structure),
  pvStructure(getPVDataCreate()->createPVStructure(structure)),
  bitSet(BitSetPtr(new BitSet(pvStructure->getNumberFields())))
{
    size_t nfields = pvStructure->getNumberFields();
    postHandler.resize(nfields);
    PVFieldPtr pvField;
    for(size_t i =0; i<nfields; ++i)
    {
        postHandler[i] = PostHandlerPtr(new PvaPostHandlerPvt(this, i));
        if(i==0) {
            pvField = pvStructure;
        } else {
            pvField = pvStructure->getSubField(i);
        }
        pvField->setPostHandler(postHandler[i]);
    }
    pvValue = pvStructure->getSubField("value");
}

void PvaPutData::checkValue()
{
    if(pvValue) return;
    throw std::runtime_error(messagePrefix + noValue);
}

void PvaPutData::postPut(size_t fieldNumber)
{
    bitSet->set(fieldNumber);
}

void PvaPutData::setMessagePrefix(std::string const & value)
{
    messagePrefix = value + " ";
}

StructureConstPtr PvaPutData::getStructure()
{return structure;}

PVStructurePtr PvaPutData::getPVStructure()
{return pvStructure;}

BitSetPtr PvaPutData::getBitSet()
{return bitSet;}

std::ostream & PvaPutData::showChanged(std::ostream & out)
{
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

bool PvaPutData::hasValue()
{
    if(!pvValue) return false;
    return true;
}

bool PvaPutData::isValueScalar()
{
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalar) return true;
    return false;
}

bool PvaPutData::isValueScalarArray()
{
    if(!pvValue) return false;
    if(pvValue->getField()->getType()==scalarArray) return true;
    return false;
}

PVFieldPtr  PvaPutData::getValue()
{
   checkValue();
   return pvValue;
}

PVScalarPtr  PvaPutData::getScalarValue()
{
    checkValue();
    PVScalarPtr pv = pvStructure->getSubField<PVScalar>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notScalar);
    }
    return pv;
}

PVArrayPtr  PvaPutData::getArrayValue()
{
    checkValue();
    PVArrayPtr pv = pvStructure->getSubField<PVArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notArray);
    }
    return pv;
}

PVScalarArrayPtr  PvaPutData::getScalarArrayValue()
{
    checkValue();
    PVScalarArrayPtr pv = pvStructure->getSubField<PVScalarArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notScalarArray);
    }
    return pv;
}

double PvaPutData::getDouble()
{
    PVScalarPtr pvScalar = getScalarValue();
    ScalarType scalarType = pvScalar->getScalar()->getScalarType();
    if(scalarType==pvDouble) {
        PVDoublePtr pvDouble = static_pointer_cast<PVDouble>(pvScalar);
        return pvDouble->get();
    }
    if(!ScalarTypeFunc::isNumeric(scalarType)) {
        throw std::runtime_error(notCompatibleScalar);
    }
    return convert->toDouble(pvScalar);
}

string PvaPutData::getString()
{
    PVScalarPtr pvScalar = getScalarValue();
    return convert->toString(pvScalar);
}

shared_vector<const double> PvaPutData::getDoubleArray()
{
    checkValue();
    PVDoubleArrayPtr pv = pvStructure->getSubField<PVDoubleArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notDoubleArray);
    }
    return pv->view();   
}

shared_vector<const string> PvaPutData::getStringArray()
{
    checkValue();
    PVStringArrayPtr pv = pvStructure->getSubField<PVStringArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notStringArray);
    }
    return pv->view();   

}

void PvaPutData::putDouble(double value)
{
    PVScalarPtr pvScalar = getScalarValue();
    ScalarType scalarType = pvScalar->getScalar()->getScalarType();
    if(scalarType==pvDouble) {
        PVDoublePtr pvDouble = static_pointer_cast<PVDouble>(pvScalar);
         pvDouble->put(value);
    }
    if(!ScalarTypeFunc::isNumeric(scalarType)) {
        throw std::runtime_error(messagePrefix + notCompatibleScalar);
    }
    convert->fromDouble(pvScalar,value);
}

void PvaPutData::putString(std::string const & value)
{
    PVScalarPtr pvScalar = getScalarValue();
    convert->fromString(pvScalar,value);
}



void PvaPutData::putDoubleArray(shared_vector<const double> const & value)
{
    checkValue();
    PVDoubleArrayPtr pv = pvStructure->getSubField<PVDoubleArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notDoubleArray);
    }
    pv->replace(value);
}

void PvaPutData::putStringArray(shared_vector<const std::string> const & value)
{
    checkValue();
    PVStringArrayPtr pv = pvStructure->getSubField<PVStringArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notStringArray);
    }
    pv->replace(value);
}

void PvaPutData::putStringArray(std::vector<std::string> const & value)
{
    checkValue();
    PVScalarArrayPtr pv = pvStructure->getSubField<PVScalarArray>("value");
    if(!pv) {
        throw std::runtime_error(messagePrefix + notScalarArray);
    }
    convert->fromStringArray(pv,0,value.size(),value,0);
}

}}
