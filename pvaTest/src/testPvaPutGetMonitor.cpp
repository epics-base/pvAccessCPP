/*testPvaPutGetMonitor.cpp */
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


class MyMonitor : public PvaMonitorRequester
{
public:
     MyMonitor() {}
     virtual ~MyMonitor() {}
     virtual void event(PvaMonitorPtr monitor)
     {
         while(true) {
            if(!monitor->poll()) return;
            PvaMonitorDataPtr pvaData = monitor->getData();
            cout << "changed\n";
            pvaData->showChanged(cout);
            cout << "overrun\n";
            pvaData->showOverrun(cout);
            monitor->releaseEvent();
            
         }
     }
};

static void exampleDouble(PvaPtr const &pva)
{
    cout << "\nstarting exampleDouble\n";
    try {
        cout << "long way\n";
        PvaChannelPtr pvaChannel = pva->createChannel("exampleDouble");
        pvaChannel->connect(2.0);
        testOk(true==true,"connected");
        PvaPutPtr put = pvaChannel->createPut();
        PvaPutDataPtr putData = put->getData();
        testOk(true==true,"put connected");
        PvaGetPtr get = pvaChannel->createGet();
        PvaGetDataPtr getData = get->getData();
        testOk(true==true,"get connected");
        PvaMonitorRequesterPtr requester(new MyMonitor());
        PvaMonitorPtr monitor = pvaChannel->monitor(requester);
        testOk(true==true,"monitor connected");
        double out;
        double in;
        for(size_t i=0 ; i< 5; ++i) {
             out = i;
             putData->putDouble(out);
             put->put();
             get->get();
             in = getData->getDouble();
             cout << "out " << out << " in " << in << endl;
        }
        PvaProcessPtr process = pvaChannel->createProcess();
        process->connect();
        process->process();
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }
}


MAIN(testPvaPutGetMonitor)
{
    cout << "\nstarting testPvaPutGetMonitor\n";
    testPlan(4);
    PvaPtr pva = Pva::create();
    exampleDouble(pva);
    cout << "done\n";
    return 0;
}
