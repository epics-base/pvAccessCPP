/*examplePvaPutGet.cpp */
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

#include <pv/pva.h>
#include <epicsUnitTest.h>
#include <testMain.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;


static void example(PvaPtr const &pva)
{
    cout << "\nstarting channelPutGet example\n";
    try {
        PvaChannelPtr pvaChannel = pva->createChannel("examplePowerSupply");
        pvaChannel->connect(2.0);
        testOk(true==true,"connected");
        PvaPutGetPtr putGet = pvaChannel->createPutGet(
            "putField(power.value,voltage.value)getField()");
        PvaPutDataPtr putData = putGet->getPutData();
        testOk(true==true,"put connected");
        PVStructurePtr pvStructure = putData->getPVStructure();
        PVDoublePtr power = pvStructure->getSubField<PVDouble>("power.value");
        PVDoublePtr voltage = pvStructure->getSubField<PVDouble>("voltage.value");
        power->put(5.0);
        voltage->put(5.0);
        putGet->putGet();
        PvaGetDataPtr getData = putGet->getGetData();
        pvStructure = getData->getPVStructure();
        BitSetPtr bitSet = getData->getBitSet();
        cout << "changed " << getData->showChanged(cout) << endl;
        cout << "bitSet " << *bitSet << endl;
        power->put(6.0);
        putGet->putGet();
        pvStructure = getData->getPVStructure();
        bitSet = getData->getBitSet();
        cout << "changed " << getData->showChanged(cout) << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }
}


MAIN(testPvaPutGet)
{
    cout << "\nstarting testPvaPutGet\n";
    testPlan(2);
    PvaPtr pva = Pva::create();
    example(pva);
    cout << "done\n";
    return 0;
}
