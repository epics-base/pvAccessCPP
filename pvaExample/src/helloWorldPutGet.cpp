/*helloWorldPutGet.cpp */
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

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;


static void example(PvaPtr const &pva)
{
    cout << "helloWorldPutGet\n";
    try {
        PvaChannelPtr channel = pva->channel("exampleHello");
        PvaPutGetPtr putGet = channel->createPutGet();
        putGet->connect();
        PvaPutDataPtr putData = putGet->getPutData();
        PVStructurePtr arg = putData->getPVStructure();
        PVStringPtr pvValue = arg->getSubField<PVString>("argument.value");
        pvValue->put("World");
        putGet->putGet();
        PvaGetDataPtr getData = putGet->getGetData();
        cout << getData->getPVStructure() << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }
}


int main(int argc,char *argv[])
{
    PvaPtr pva = Pva::create();
    example(pva);
    return 0;
}
