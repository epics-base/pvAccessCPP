/*examplePvaProcess.cpp */
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


static void exampleProcess(PvaPtr const &pva)
{
    cout << "example process\n";
    PvaChannelPtr channel = pva->channel("exampleDouble");
    PvaProcessPtr process = channel->createProcess();
    try {
        process->process();
        cout <<  channel->get("field()")->getData()->showChanged(cout) << endl;
        process->process();
        cout <<  channel->get("field()")->getData()->showChanged(cout) << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }
}


int main(int argc,char *argv[])
{
    PvaPtr pva = Pva::create();
    exampleProcess(pva);
    return 0;
}
