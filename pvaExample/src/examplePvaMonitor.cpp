/*monitorPowerSupply.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 */

/* Author: Marty Kraimer */

#include <epicsThread.h>

#include <iostream>

#include <pv/pva.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;


static void exampleMonitor(PvaPtr const &pva)
{
    PvaMonitorPtr monitor = pva->channel("examplePowerSupply")->monitor("");
    PvaMonitorDataPtr pvaData = monitor->getData();
    while(true) {
         monitor->waitEvent();
         cout << "changed\n";
         pvaData->showChanged(cout);
         cout << "overrun\n";
         pvaData->showOverrun(cout);
         monitor->releaseEvent();
     }
}


int main(int argc,char *argv[])
{
    PvaPtr pva = Pva::create();
    exampleMonitor(pva);
    cout << "done\n";
    return 0;
}
