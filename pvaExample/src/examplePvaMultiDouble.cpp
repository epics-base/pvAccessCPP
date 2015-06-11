/*examplePvaMultiDouble.cpp */
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

#include <pv/pvaMultiDouble.h>

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;


static void example(PvaPtr const &pva)
{
    cout << "example multiDouble\n";
    size_t num = 5;
    shared_vector<string> channelNames(num);
    channelNames[0] = "exampleDouble01";
    channelNames[1] = "exampleDouble02";
    channelNames[2] = "exampleDouble03";
    channelNames[3] = "exampleDouble04";
    channelNames[4] = "exampleDouble05";
    PVStringArrayPtr pvNames =
        getPVDataCreate()->createPVScalarArray<PVStringArray>();
    pvNames->replace(freeze(channelNames));
    PvaMultiDoublePtr multiDouble(PvaMultiDouble::create(pva,pvNames));
    try {
        shared_vector<double> data = multiDouble->get();
        cout << "initial " << data << endl;
        for(size_t i=0; i<num; ++i) data[i] = data[i] + 1.1;
        multiDouble->put(data);
        data =  multiDouble->get();
        cout << "final " << data << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }

}

static void exampleCA(PvaPtr const &pva)
{
    cout << "example multiDouble\n";
    size_t num = 5;
    shared_vector<string> channelNames(num);
    channelNames[0] = "double01";
    channelNames[1] = "double02";
    channelNames[2] = "double03";
    channelNames[3] = "double04";
    channelNames[4] = "double05";
    PVStringArrayPtr pvNames =
        getPVDataCreate()->createPVScalarArray<PVStringArray>();
    pvNames->replace(freeze(channelNames));
    PvaMultiDoublePtr multiDouble(PvaMultiDouble::create(pva,pvNames,5.0,"ca"));
    try {
        shared_vector<double> data = multiDouble->get();
        cout << "initial " << data << endl;
        for(size_t i=0; i<num; ++i) data[i] = data[i] + 1.1;
        multiDouble->put(data);
        data =  multiDouble->get();
        cout << "final " << data << endl;
    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }

}


int main(int argc,char *argv[])
{
    PvaPtr pva = Pva::create();
    example(pva);
    exampleCA(pva);
    return 0;
}
