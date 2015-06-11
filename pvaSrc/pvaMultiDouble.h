/* pvaMultiDouble.h */
/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * EPICS pvData is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2015.02
 */
#ifndef PVAMULTIDOUBLE_H
#define PVAMULTIDOUBLE_H

#ifdef epicsExportSharedSymbols
#   define pvaEpicsExportSharedSymbols
#   undef epicsExportSharedSymbols
#endif

#include <pv/pva.h>

namespace epics { namespace pva { 

class PvaMultiDouble;
typedef std::tr1::shared_ptr<PvaMultiDouble> PvaMultiDoublePtr;

/**
 * @brief Support for multiple channels where each channel has a value field that is a scalar double.
 * If any problems arise an exception is thrown.
 *
 * @author mrk
 */
class epicsShareClass PvaMultiDouble 
{
public:
    POINTER_DEFINITIONS(PvaMultiDouble);
    /**
     * @brief Create a PvaMultiDouble.
     * @param &pva Interface to Pva
     * @param channelName PVStringArray of channelNames.
     * @param timeout The timeout in seconds for connecting.
     * @param providerName The name of the channelProvider for each channel.
     * @return The interface to PvaMultiDouble.
     */
    static PvaMultiDoublePtr create(
        PvaPtr const & pva,
        epics::pvData::PVStringArrayPtr const & channelName,
        double timeout = 5.0,
        std::string const & providerName = "pva");
    /**
     * @brief destructor
     */
    ~PvaMultiDouble();
    /** 
     * @brief destroy any resources used.
     */
    void destroy();
    /** 
     * @brief get the value of all the channels.
     * @return The data.
     */
    epics::pvData::shared_vector<double> get();
    /** 
     * @brief put a new value to each  channel.
     * @param value The data.
     */
    void put(epics::pvData::shared_vector<double> const &value);
    PvaMultiChannelPtr getPvaMultiChannel();
private:
    PvaMultiDouble(
        PvaMultiChannelPtr const & channelName);
    void createGet();
    void createPut();

    PvaMultiChannelPtr pvaMultiChannel;
    std::vector<PvaGetPtr> pvaGet;
    std::vector<PvaPutPtr> pvaPut;
};

}}

#endif // PVAMULTIDOUBLE_H
