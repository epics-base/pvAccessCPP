/*examplePvaPut.cpp */
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


static void examplePut(PvaPtr const &pva)
{
    cout << "example put\n";
    PvaChannelPtr channel = pva->channel("exampleDouble");
    PvaPutPtr put = channel->put();
    PvaPutDataPtr putData = put->getData();
    try {
        putData->putDouble(3.0); put->put();
        cout <<  channel->get("field()")->getData()->showChanged(cout) << endl;
        putData->putDouble(4.0); put->put();
        cout <<  channel->get("field()")->getData()->showChanged(cout) << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }
}


int main(int argc,char *argv[])
{
    PvaPtr pva = Pva::create();
    examplePut(pva);
    return 0;
}
