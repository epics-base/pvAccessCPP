/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/**
 * @author mrk
 * @date 2018.03
 */
#ifndef DbdToPv_H
#define DbdToPv_H

#include <shareLib.h>
#include <pv/pvAccess.h>
#include <cadef.h>
#include <pv/event.h>
#include "caChannel.h"

namespace epics {
namespace pvAccess {
namespace ca {

enum IOType {getIO,putIO,monitorIO};

class AlarmDbd;
typedef std::tr1::shared_ptr<AlarmDbd> AlarmDbdPtr;
class TimeStampDbd;
typedef std::tr1::shared_ptr<TimeStampDbd> TimeStampDbdPtr;
class DisplayDbd;
typedef std::tr1::shared_ptr<DisplayDbd> DisplayDbdPtr;
class ControlDbd;
typedef std::tr1::shared_ptr<ControlDbd> ControlDbdPtr;
class ValueAlarmDbd;
typedef std::tr1::shared_ptr<ValueAlarmDbd> ValueAlarmDbdPtr;

struct CaAlarm
{
    dbr_short_t	status;
    dbr_short_t	severity;
    CaAlarm() : status(0), severity(0) {}
};

struct CaDisplay
{
    double      lower_disp_limit;
    double      upper_disp_limit;
    std::string units;
    std::string format;
    CaDisplay() : lower_disp_limit(0),upper_disp_limit(0) {}
};

struct CaControl
{
    double upper_ctrl_limit;
    double lower_ctrl_limit;
    CaControl() : upper_ctrl_limit(0),lower_ctrl_limit(0) {}
};

struct CaValueAlarm
{
    double upper_alarm_limit;	
    double upper_warning_limit;
    double lower_warning_limit;
    double lower_alarm_limit;
    CaValueAlarm() :
       upper_alarm_limit(0),
       upper_warning_limit(0),
       lower_warning_limit(0),
       lower_alarm_limit(0)
    {}
};

class DbdToPv;
typedef std::tr1::shared_ptr<DbdToPv> DbdToPvPtr;

typedef void ( caCallbackFunc ) (struct event_handler_args);

/**
 * @brief  DbdToPv converts between DBD data and pvData.
 *
 * 
 */
class DbdToPv
{
public:
    POINTER_DEFINITIONS(DbdToPv);
    static DbdToPvPtr create(
        CAChannelPtr const & caChannel,
        epics::pvData::PVStructurePtr const & pvRequest,
        IOType ioType
    );
    epics::pvData::Structure::const_shared_pointer getStructure();
    void getChoices(CAChannelPtr const & caChannel);
    epics::pvData::PVStructurePtr createPVStructure();
    chtype getRequestType();
    epics::pvData::Status getFromDBD(
        epics::pvData::PVStructurePtr const & pvStructure,
        epics::pvData::BitSet::shared_pointer const & bitSet,
        struct event_handler_args &args
    );
    epics::pvData::Status putToDBD(
         CAChannelPtr const & caChannel,
         epics::pvData::PVStructurePtr const & pvStructure,
         bool block,
         caCallbackFunc putHandler,
         void *userArg
    );
    void getChoicesDone(struct event_handler_args &args);
private:
    DbdToPv(IOType ioType);
    void activate(
        CAChannelPtr const & caChannel,
        epics::pvData::PVStructurePtr const & pvRequest
    );
    IOType ioType;
    bool dbfIsUCHAR;
    bool dbfIsUSHORT;
    bool dbfIsULONG;
    bool dbfIsINT64;
    bool dbfIsUINT64;
    bool valueRequested;
    bool alarmRequested;
    bool timeStampRequested;
    bool displayRequested;
    bool controlRequested;
    bool valueAlarmRequested;
    bool isArray;
    bool charArrayIsString;
    bool firstTime;
    chtype caValueType;
    chtype caRequestType;
    unsigned long maxElements;
    epics::pvData::Event choicesEvent;
    epicsTimeStamp caTimeStamp;
    CaAlarm caAlarm;
    CaDisplay caDisplay;
    CaControl caControl;
    CaValueAlarm caValueAlarm;
    epics::pvData::Structure::const_shared_pointer structure;
    std::vector<std::string> choices;
    epics::pvData::PVDoubleArrayPtr pvDoubleArray; //for dbfIsINT64 and dbfIsUINT64
};

}
}
}

#endif  /* DbdToPv_H */
