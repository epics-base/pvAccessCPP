/*examplePvaNTMultiChannel.cpp */
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

using namespace std;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pva;
using namespace epics::nt;


static void example(PvaPtr const &pva)
{
    cout << "example ntMultiChannel\n";
    size_t num = 5;
    shared_vector<string> channelNames(num);
    channelNames[0] = "exampleDouble";
    channelNames[1] = "exampleDoubleArray";
    channelNames[2] = "exampleString";
    channelNames[3] = "exampleBoolean";
    channelNames[4] = "exampleEnum";
    PVStringArrayPtr pvNames =
        getPVDataCreate()->createPVScalarArray<PVStringArray>();
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
    try {
        NTMultiChannelPtr nt = multi->get();
        cout << "initial\n" << nt->getPVStructure() << endl;

    } catch (std::runtime_error e) {
        cout << "exception " << e.what() << endl;
    }

}

static void exampleCA(PvaPtr const &pva)
{
    cout << "example ntMultiChannel\n";
    size_t num = 5;
    shared_vector<string> channelNames(num);
    channelNames[0] = "double00";
    channelNames[1] = "doubleArray";
    channelNames[2] = "string00";
    channelNames[3] = "mbbiwierd";
    channelNames[4] = "enum01";
    PVStringArrayPtr pvNames =
        getPVDataCreate()->createPVScalarArray<PVStringArray>();
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
       pva,pvNames,structure,5.0,"ca");
    try {
        NTMultiChannelPtr nt = multi->get();
        cout << "initial\n" << nt->getPVStructure() << endl;

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
