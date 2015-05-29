/* pvaMultiDouble.cpp */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.03
 */

#define epicsExportSharedSymbols
#include <pv/pvaMultiDouble.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace std;

namespace epics { namespace pva { 

PvaMultiDoublePtr PvaMultiDouble::create(
    PvaPtr const & pva,
    PVStringArrayPtr const & channelName,
    double timeout,
    std::string const & providerName)
{
    PvaMultiChannelPtr pvaMultiChannel(
        PvaMultiChannel::create(pva,channelName,providerName));
    Status status = pvaMultiChannel->connect(timeout,0);
    if(!status.isOK()) throw std::runtime_error(status.getMessage());
    return PvaMultiDoublePtr(new PvaMultiDouble(pvaMultiChannel));
}

PvaMultiDouble::PvaMultiDouble(PvaMultiChannelPtr const &pvaMultiChannel)
:
   pvaMultiChannel(pvaMultiChannel)
{}

PvaMultiDouble::~PvaMultiDouble()
{
}

void PvaMultiDouble::createGet()
{
    PvaChannelArrayPtr pvaChannelArray = pvaMultiChannel->getPvaChannelArray().lock();
    if(!pvaChannelArray)  throw std::runtime_error("pvaChannelArray is gone");
    shared_vector<const PvaChannelPtr> pvaChannels = *pvaChannelArray;
    size_t numChannel = pvaChannels.size();
    pvaGet = std::vector<PvaGetPtr>(numChannel,PvaGetPtr());
    bool allOK = true;
    string message;
    for(size_t i=0; i<numChannel; ++i)
    {
        pvaGet[i] = pvaChannels[i]->createGet("value");
        pvaGet[i]->issueConnect();
    }
    for(size_t i=0; i<numChannel; ++i)
    {
         Status status = pvaGet[i]->waitConnect();
         if(!status.isOK()) {
             message = "connect status " + status.getMessage();
             allOK = false;
             break;
         }
    }
    if(!allOK) throw std::runtime_error(message);
}

void PvaMultiDouble::createPut()
{
    PvaChannelArrayPtr pvaChannelArray = pvaMultiChannel->getPvaChannelArray().lock();
    if(!pvaChannelArray)  throw std::runtime_error("pvaChannelArray is gone");
    shared_vector<const PvaChannelPtr> pvaChannels = *pvaChannelArray;
    size_t numChannel = pvaChannels.size();
    pvaPut = std::vector<PvaPutPtr>(numChannel,PvaPutPtr());
    bool allOK = true;
    string message;
    for(size_t i=0; i<numChannel; ++i)
    {
        pvaPut[i] = pvaChannels[i]->createPut("value");
        pvaPut[i]->issueConnect();
    }
    for(size_t i=0; i<numChannel; ++i)
    {
         Status status = pvaPut[i]->waitConnect();
         if(!status.isOK()) {
             message = "connect status " + status.getMessage();
             allOK = false;
             break;
         }
    }
    if(!allOK) throw std::runtime_error(message);
}

epics::pvData::shared_vector<double> PvaMultiDouble::get()
{
    if(pvaGet.empty()) createGet();
    shared_vector<const string> channelNames = pvaMultiChannel->getChannelNames()->view();
    size_t numChannel = channelNames.size();
    epics::pvData::shared_vector<double> data(channelNames.size());
    for(size_t i=0; i<numChannel; ++i)
    {
        pvaGet[i]->issueGet();
    }
    for(size_t i=0; i<numChannel; ++i)
    {
        Status status = pvaGet[i]->waitGet();
        if(!status.isOK()) {
            string message = channelNames[i] + " " + status.getMessage();
            throw std::runtime_error(message);
        }
        data[i] = pvaGet[i]->getData()->getDouble();
    }
    return data;
}

void PvaMultiDouble::put(shared_vector<double> const &value)
{
    if(pvaPut.empty()) createPut();
    shared_vector<const string> channelNames = pvaMultiChannel->getChannelNames()->view();
    size_t numChannel = channelNames.size();
    for(size_t i=0; i<numChannel; ++i)
    {
        pvaPut[i]->getData()->putDouble(value[i]);
        pvaPut[i]->issuePut();
    }
    for(size_t i=0; i<numChannel; ++i)
    {
        Status status = pvaPut[i]->waitPut();
        if(!status.isOK()) {
            string message = channelNames[i] + " " + status.getMessage();
            throw std::runtime_error(message);
        }
    }
}


}}
