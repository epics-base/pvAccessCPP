/* pvaNTMultiChannel.cpp */
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
#include <pv/pvaNTMultiChannel.h>

using std::tr1::static_pointer_cast;
using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::nt;
using namespace std;

namespace epics { namespace pva { 

PvaNTMultiChannelPtr PvaNTMultiChannel::create(
    PvaPtr const & pva,
    PVStringArrayPtr const & channelName,
    StructureConstPtr const &structure,
    double timeout,
    std::string const & providerName)
{
    PvaMultiChannelPtr pvaMultiChannel(
        PvaMultiChannel::create(pva,channelName,providerName));
    Status status = pvaMultiChannel->connect(timeout,0);
    if(!status.isOK()) throw std::runtime_error(status.getMessage());
    if(!NTMultiChannel::is_a(structure)) throw std::runtime_error("structure is not valid");
    PVStructurePtr pvStructure = getPVDataCreate()->createPVStructure(structure);
    pvStructure->getSubField<PVStringArray>("channelName")->
        replace(pvaMultiChannel->getChannelNames()->view());
    pvStructure->getSubField<PVBooleanArray>("isConnected")->
        replace(pvaMultiChannel->getIsConnected()->view());
    NTMultiChannelPtr ntMultiChannel(NTMultiChannel::wrap(pvStructure));
    return PvaNTMultiChannelPtr(new PvaNTMultiChannel(pvaMultiChannel,ntMultiChannel));
}

PvaNTMultiChannel::PvaNTMultiChannel(
        PvaMultiChannelPtr const &pvaMultiChannel,
        NTMultiChannelPtr const &ntMultiChannel)
:
   pvaMultiChannel(pvaMultiChannel),
   ntMultiChannel(ntMultiChannel),
   pvUnionArray(ntMultiChannel->getPVStructure()->getSubField<PVUnionArray>("value")),
   pvDataCreate(getPVDataCreate())
{}

PvaNTMultiChannel::~PvaNTMultiChannel()
{
}

void PvaNTMultiChannel::createGet()
{
    PVStructurePtr pvStructure = ntMultiChannel->getPVStructure();
    bool getAlarm = false;
    if(pvStructure->getSubField("severity")) getAlarm = true;
    if(pvStructure->getSubField("status")) getAlarm = true;
    if(pvStructure->getSubField("severity")) getAlarm = true;
    bool getTimeStamp = false;
    if(pvStructure->getSubField("secondsPastEpoch")) getTimeStamp = true;
    if(pvStructure->getSubField("nanoseconds")) getTimeStamp = true;
    if(pvStructure->getSubField("userTag")) getTimeStamp = true;
    string request = "value";
    if(getAlarm) request += ",alarm";
    if(getTimeStamp) request += ",timeStamp";
    PvaChannelArrayPtr pvaChannelArray = pvaMultiChannel->getPvaChannelArray().lock();
    if(!pvaChannelArray)  throw std::runtime_error("pvaChannelArray is gone");
    shared_vector<const PvaChannelPtr> pvaChannels = *pvaChannelArray;
    size_t numChannel = pvaChannels.size();
    pvaGet = std::vector<PvaGetPtr>(numChannel,PvaGetPtr());
    bool allOK = true;
    string message;
    for(size_t i=0; i<numChannel; ++i)
    {
        pvaGet[i] = pvaChannels[i]->createGet(request);
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

void PvaNTMultiChannel::createPut()
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

NTMultiChannelPtr PvaNTMultiChannel::get()
{
    if(pvaGet.empty()) createGet();
    PVStructurePtr pvStructure = ntMultiChannel->getPVStructure();
    shared_vector<const string> channelNames = pvaMultiChannel->getChannelNames()->view();
    size_t numChannel = channelNames.size();
    bool severityExists = false;
    bool statusExists = false;
    bool messageExists = false;
    bool secondsPastEpochExists = false;
    bool nanosecondsExists = false;
    bool userTagExists = false;
    if(pvStructure->getSubField("severity")) {
        severity.resize(numChannel);
        severityExists = true;
    }
    if(pvStructure->getSubField("status")) {
        status.resize(numChannel);
        statusExists = true;
    }
    if(pvStructure->getSubField("message")) {
        message.resize(numChannel);
        messageExists = true;
    }
    if(pvStructure->getSubField("secondsPastEpoch")) {
        secondsPastEpoch.resize(numChannel);
        secondsPastEpochExists = true;
    }
    if(pvStructure->getSubField("nanoseconds")) {
        nanoseconds.resize(numChannel);
        nanosecondsExists = true;
    }
    if(pvStructure->getSubField("userTag")) {
        userTag.resize(numChannel);
        userTagExists = true;
    }
    shared_vector<PVUnionPtr> valueVector(numChannel);
    for(size_t i=0; i<numChannel; ++i)
    {
        pvaGet[i]->issueGet();
    }
    for(size_t i=0; i<numChannel; ++i)
    {
        Status stat = pvaGet[i]->waitGet();
        if(!stat.isOK()) {
            string message = channelNames[i] + " " + stat.getMessage();
            throw std::runtime_error(message);
        }
        PVStructurePtr pvStructure = pvaGet[i]->getData()->getPVStructure();
        PVFieldPtr pvField = pvStructure->getSubField("value");
        if(!pvField) {
            string message = channelNames[i] + " no value field";
            throw std::runtime_error(message);
        }
        UnionConstPtr u = pvUnionArray->getUnionArray()->getUnion();
        if(u->isVariant()) {
              PVUnionPtr pvUnion = pvDataCreate->createPVVariantUnion();
              pvUnion->set(pvDataCreate->createPVField(pvField));
              valueVector[i] = pvUnion;
        } else {
              PVUnionPtr pvUnion = pvDataCreate->createPVUnion(u);
              pvUnion->set(pvField);
              valueVector[i] = pvUnion;
        }
        pvField = pvStructure->getSubField("alarm");
        if(pvField) {
            if(pvAlarm.attach(pvField)) {
                 pvAlarm.get(alarm);
                 if(severityExists) severity[i] = alarm.getSeverity();
                 if(statusExists) status[i] = alarm.getStatus();
                 if(messageExists) message[i] = alarm.getMessage();
            }
        }
        pvField = pvStructure->getSubField("timeStamp");
        if(pvField) {
            if(pvTimeStamp.attach(pvField)) {
                 pvTimeStamp.get(timeStamp);
                 if(secondsPastEpochExists) secondsPastEpoch[i] =
                     timeStamp.getSecondsPastEpoch();
                 if(nanosecondsExists) nanoseconds[i] =
                     timeStamp.getNanoseconds();
                 if(userTagExists) userTag[i] = timeStamp.getUserTag();
            }
        }
    }
    pvUnionArray->replace(freeze(valueVector));
    if(severityExists) {
         pvStructure->getSubField<PVIntArray>("severity")->replace(
             freeze(severity));
    }
    if(statusExists) {
         pvStructure->getSubField<PVIntArray>("status")->replace(
             freeze(status));
    }
    if(messageExists) {
         pvStructure->getSubField<PVStringArray>("message")->replace(freeze(message));
    }
    if(secondsPastEpochExists) {
         pvStructure->getSubField<PVLongArray>("secondsPastEpoch")->replace(freeze(secondsPastEpoch));
    }
    if(nanosecondsExists) {
         pvStructure->getSubField<PVIntArray>("nanoseconds")->replace(freeze(nanoseconds));
    }
    if(userTagExists) {
         pvStructure->getSubField<PVIntArray>("userTag")->replace(freeze(userTag));
    }
    return ntMultiChannel;
}

void PvaNTMultiChannel::put(NTMultiChannelPtr const &value)
{
    if(pvaPut.empty()) createPut();
    shared_vector<const string> channelNames = pvaMultiChannel->getChannelNames()->view();
    size_t numChannel = channelNames.size();
    PVUnionArrayPtr pvValue = value->getPVStructure()->
        getSubField<PVUnionArray>("value");
    shared_vector<const PVUnionPtr> valueVector = pvValue->view();
    for(size_t i=0; i<numChannel; ++i)
    {
        try {
            PVFieldPtr pvFrom = valueVector[i]->get();
            PVFieldPtr pvTo = pvaPut[i]->getData()->getValue();
            Type typeFrom = pvFrom->getField()->getType();
            Type typeTo = pvTo->getField()->getType();
            if(typeFrom==typeTo) {
                  if(typeFrom==scalar || typeFrom==scalarArray) {
                      pvTo->copy(*pvFrom);
                  }
            }
            pvaPut[i]->issuePut();
        } catch (std::exception e) {
            string message = channelNames[i] + " " + e.what();
            throw std::runtime_error(message);
        }
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
