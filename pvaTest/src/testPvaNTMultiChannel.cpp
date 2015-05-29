/*testPvaNTMultiChannel.cpp */
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

#include <pv/pvaNTMultiChannel.h>
#include <epicsUnitTest.h>
#include <testMain.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;
using namespace epics::nt;
using std::tr1::static_pointer_cast;


static void testGood(PvaPtr const &pva)
{
    PVDataCreatePtr pvDataCreate(getPVDataCreate());
    bool isOk = true;
    cout << "\nstarting testGood\n";
    try {
        PvaPtr pva(Pva::create());
        size_t num = 5;
        shared_vector<string> channelNames(num);
        channelNames[0] = "exampleDouble";
        channelNames[1] = "exampleDoubleArray";
        channelNames[2] = "exampleString";
        channelNames[3] = "exampleBoolean";
        channelNames[4] = "exampleEnum";
        PVStringArrayPtr pvNames = pvDataCreate->
            createPVScalarArray<PVStringArray>();
        pvNames->replace(freeze(channelNames));
        NTMultiChannelBuilderPtr builder = NTMultiChannel::createBuilder();
        StructureConstPtr structure = builder->
            addTimeStamp()->
            addSeverity() ->
            addStatus() ->
            addMessage() ->
            addSecondsPastEpoch() ->
            addNanoseconds() ->
            addUserTag() ->
            createStructure();
         PvaNTMultiChannelPtr multi = PvaNTMultiChannel::create(
             pva,pvNames,structure);
         NTMultiChannelPtr nt = multi->get();
         for(size_t numtimes=0; numtimes<3; ++numtimes) {
             PVUnionArrayPtr pvValue = nt->getPVStructure()->
                 getSubField<PVUnionArray>("value");
             cout << "initial\n" << nt->getPVStructure() << endl;
             shared_vector<PVUnionPtr> valueVector = pvValue->reuse();
             for(size_t i=0; i<num; ++i)
             {
                 PVFieldPtr pvField = valueVector[i]->get();
                 Type type = pvField->getField()->getType();
                 if(type==scalar) {
                     PVScalarPtr pvScalar = static_pointer_cast<PVScalar>(pvField);
                     ScalarType scalarType = pvScalar->getScalar()->getScalarType();
                     if(ScalarTypeFunc::isNumeric(scalarType)) {
                         double oldValue = pvScalar->getAs<double>();
                         oldValue++;
                         pvScalar->putFrom<double>(oldValue);
                     } else if(scalarType==pvString) {
                         PVStringPtr pv = static_pointer_cast<PVString>(pvField);
                         string val = pv->get();
                         val += " added";
                         pv->put(val);
                     } else if(scalarType==pvBoolean) {
                         PVBooleanPtr pv = static_pointer_cast<PVBoolean>(pvField);
                         bool val = pv->get();
                         pv->put(!val);
                     }
                 } else if(type==scalarArray) {
                     PVScalarArrayPtr pv =
                          static_pointer_cast<PVScalarArray>(pvField);
                     ScalarType scalarType = pv->getScalarArray()->getElementType();
                     if(scalarType==pvDouble) {
                           PVDoubleArrayPtr pvd = static_pointer_cast<PVDoubleArray>(pv);
                           shared_vector<double> valvec = pvd->reuse();
                           if(valvec.capacity()==0) {
                                valvec.resize(4);
                                for(size_t i=0; i<valvec.size(); ++i) valvec[i] = i;
                           }
                           for(size_t i=0; i<valvec.size(); ++i) valvec[i] = valvec[i] + 1.0;
                           pvd->replace(freeze(valvec));
                      }
                  } else if(type==epics::pvData::structure) {
                      PVStructurePtr pvStructure = static_pointer_cast<PVStructure>(pvField);
                      PVIntPtr pv = pvStructure->getSubField<PVInt>("index");
                      if(pv) {
                          PVStringArrayPtr choices = pvStructure->getSubField<PVStringArray>("choices");
                          if(choices) {
                               int32 nchoices = choices->getLength();
                               int32 oldval = pv->get();
                               int32 newval = (oldval==nchoices) ? 0 : ++oldval;
                               pv->put(newval);
                          }
                      }
                 }
             }
             pvValue->replace(freeze(valueVector));
             multi->put(nt);
             nt = multi->get();
         }
         cout << "final\n" << nt->getPVStructure() << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
        isOk = false;
    }
    testOk(isOk==true,"no problems");
}


MAIN(testPvaNTMultiChannel)
{
    cout << "\nstarting testPvaNTMultiChannel\n";
    testPlan(1);
    PvaPtr pva = Pva::create();
    testGood(pva);
    cout << "done\n";
    return 0;
}
