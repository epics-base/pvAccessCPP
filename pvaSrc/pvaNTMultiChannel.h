/* pvaNTMultiChannel.h */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.02
 */
#ifndef PVANTMULTIChannel_H
#define PVANTMULTIChannel_H

#ifdef epicsExportSharedSymbols
#   define pvaEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pva.h>

namespace epics { namespace pva { 

class PvaNTMultiChannel;
typedef std::tr1::shared_ptr<PvaNTMultiChannel> PvaNTMultiChannelPtr;

/**
 * @brief Support for multiple channels where each channel has a value field that
 * is a scalar, scalarArray, or enumerated structure.
 * The data is provided via normativeType NTMultiChannel.
 * If any problems arise an exception is thrown.
 *
 * @author mrk
 */
class epicsShareClass PvaNTMultiChannel 
{
public:
    POINTER_DEFINITIONS(PvaNTMultiChannel);
    /**
     * @brief Create a PvaNTMultiChannel.
     * @param &pva Interface to Pva
     * @param channelName PVStringArray of channelNames.
     * @param structure valid NTMultiChannel structure.
     * @param timeout Timeout for connecting.
     * @param providerName The provider for each channel.
     * @return The interface to PvaNTMultiChannel.
     */
    static PvaNTMultiChannelPtr create(
        PvaPtr const & pva,
        epics::pvData::PVStringArrayPtr const & channelName,
        epics::pvData::StructureConstPtr const & structure,
        double timeout = 5.0,
        std::string const & providerName = "pva");
    /**
     * @brief destructor
     */
    ~PvaNTMultiChannel();
    /** 
     * @brief destroy any resources used.
     */
    void destroy();
    /** 
     * @brief get the value of all the channels.
     * @return The data.
     */
    epics::nt::NTMultiChannelPtr get();
    /** 
     * @brief put a new value to each  channel.
     * @param value The data.
     */
    void put(epics::nt::NTMultiChannelPtr const &value);
    /** 
     * @brief Get the PvaMultiChannel.
     * @return The interface.
     */
    PvaMultiChannelPtr getPvaMultiChannel();
private:
    PvaNTMultiChannel(
        PvaMultiChannelPtr const & channelName,
        epics::nt::NTMultiChannelPtr const &ntMultiChannel);
    void createGet();
    void createPut();

    PvaMultiChannelPtr pvaMultiChannel;
    epics::nt::NTMultiChannelPtr ntMultiChannel;
    epics::pvData::PVUnionArrayPtr pvUnionArray;
    epics::pvData::PVDataCreatePtr pvDataCreate;
    std::vector<PvaGetPtr> pvaGet;
    std::vector<PvaPutPtr> pvaPut;
    epics::pvData::shared_vector<epics::pvData::int32> severity;
    epics::pvData::shared_vector<epics::pvData::int32> status;
    epics::pvData::shared_vector<std::string> message;
    epics::pvData::shared_vector<epics::pvData::int64> secondsPastEpoch;
    epics::pvData::shared_vector<epics::pvData::int32> nanoseconds;
    epics::pvData::shared_vector<epics::pvData::int32> userTag;
    epics::pvData::Alarm alarm;
    epics::pvData::PVAlarm pvAlarm;
    epics::pvData::TimeStamp timeStamp;;
    epics::pvData::PVTimeStamp pvTimeStamp;
};

}}

#endif // PVANTMULTIChannel_H
