/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#include <epicsVersion.h>
#include <sstream>
#include <alarm.h>
#include <alarmString.h>
#include <cadef.h>

#include <pv/alarm.h>
#include <pv/standardField.h>
#include <pv/logger.h>
#include <pv/pvAccess.h>
#include <pv/reftrack.h>
#include <pv/convert.h>
#include <pv/timeStamp.h>
#define epicsExportSharedSymbols
#include "caChannel.h"
#include "dbdToPv.h"

using namespace epics::pvData;
using std::string;
using std::ostringstream;
using std::cout;

namespace epics {
namespace pvAccess {
namespace ca {

#define CA_PRIORITY 50

// Macro used to avoid null pointer errors
#define GET_SUBFIELD_WITH_ERROR_CHECK(TYPE, VAR, SRC_PTR, FIELD, ERROR_MSG) \
    std::tr1::shared_ptr<TYPE> VAR = SRC_PTR->getSubField<TYPE>(FIELD); \
    if (!VAR) {                                                             \
        return Status(Status::STATUSTYPE_ERROR, ERROR_MSG);                 \
    }


DbdToPvPtr DbdToPv::create(
    CAChannelPtr const & caChannel,
    PVStructurePtr const & pvRequest,
    IOType ioType)
{
    DbdToPvPtr dbdToPv(new DbdToPv(ioType));
    dbdToPv->activate(caChannel,pvRequest);
    return dbdToPv;
}

DbdToPv::DbdToPv(IOType ioType)
:  ioType(ioType),
   dbfIsUCHAR(false),
   dbfIsUSHORT(false),
   dbfIsULONG(false),
   dbfIsINT64(false),
   dbfIsUINT64(false),
   valueRequested(false),
   alarmRequested(false),
   timeStampRequested(false),
   displayRequested(false),
   controlRequested(false),
   valueAlarmRequested(false),
   isArray(false),
   charArrayIsString(false),
   firstTime(true),
   caValueType(-1),
   caRequestType(-1),
   maxElements(0)
{
   caTimeStamp.secPastEpoch = 0;
   caTimeStamp.nsec = 0;
}

static ScalarType dbr2ST[] =
{
    pvString,   // DBR_STRING = 0
    pvShort,    // DBR_SHORT. DBR_INT = 1
    pvFloat,    // DBR_FLOAT = 2
    static_cast<ScalarType>(-1),         // DBR_ENUM = 3
    pvByte,     // DBR_CHAR = 4
    pvInt,      // DBR_LONG = 5
    pvDouble    // DBR_DOUBLE = 6
};

static chtype getDbrType(const ScalarType scalarType)
{
    switch(scalarType)
    {
         case pvString : return DBR_STRING;
         case pvByte : return DBR_CHAR;
         case pvUByte : return DBR_CHAR;
         case pvShort : return DBR_SHORT;
         case pvUShort : return DBR_SHORT;
         case pvInt : return DBR_LONG;
         case pvUInt : return DBR_LONG;
         case pvFloat : return DBR_FLOAT;
         case pvDouble : return DBR_DOUBLE;
         case pvLong : return DBR_DOUBLE;
         case pvULong : return DBR_DOUBLE;
         default: break;
    }
    throw  std::runtime_error("getDbr: illegal scalarType");
}

static dbr_short_t convertDBstatus(dbr_short_t dbStatus)
{
    switch(dbStatus) {
    case NO_ALARM:
        return noStatus;
    case READ_ALARM:
    case WRITE_ALARM:
    case HIHI_ALARM:
    case HIGH_ALARM:
    case LOLO_ALARM:
    case LOW_ALARM:
    case STATE_ALARM:
    case COS_ALARM:
    case HW_LIMIT_ALARM:
        return deviceStatus;
    case COMM_ALARM:
    case TIMEOUT_ALARM:
        return driverStatus;
    case CALC_ALARM:
    case SCAN_ALARM:
    case LINK_ALARM:
    case SOFT_ALARM:
    case BAD_SUB_ALARM:
        return recordStatus;
    case DISABLE_ALARM:
    case SIMM_ALARM:
    case READ_ACCESS_ALARM:
    case WRITE_ACCESS_ALARM:
        return dbStatus;
    case UDF_ALARM:
        return undefinedStatus;
    default:
        return undefinedStatus; // UNDEFINED
    }

}

void DbdToPv::activate(
    CAChannelPtr const & caChannel,
    PVStructurePtr const & pvRequest)
{
    chid channelID = caChannel->getChannelID();
    chtype channelType = ca_field_type(channelID);
    caValueType = (channelType==DBR_ENUM ? DBR_ENUM : getDbrType(dbr2ST[channelType]));
    if(!pvRequest) {
        string mess(caChannel->getChannelName());
            mess += " DbdToPv::activate pvRequest is null";
            throw  std::runtime_error(mess);
    }
    PVStructurePtr fieldPVStructure;
    if(pvRequest->getPVFields().size()==0) {
         fieldPVStructure = pvRequest;
    } else {
         fieldPVStructure = pvRequest->getSubField<PVStructure>("field");
    }
    if(!fieldPVStructure) {
        ostringstream mess;
        mess << caChannel->getChannelName()
          << " DbdToPv::activate illegal pvRequest " << pvRequest;
        throw std::runtime_error(mess.str());
    }
    if(fieldPVStructure->getPVFields().size()==0)
    {
        valueRequested = true;
        alarmRequested = true;
        timeStampRequested = true;
        displayRequested = true;
        controlRequested = true;
        valueAlarmRequested = true;
    } else {
        if(fieldPVStructure->getSubField("value")) valueRequested = true;
        if(fieldPVStructure->getSubField("alarm")) alarmRequested = true;
        if(fieldPVStructure->getSubField("timeStamp")) timeStampRequested = true;
        if(fieldPVStructure->getSubField("display")) displayRequested = true;
        if(fieldPVStructure->getSubField("control")) controlRequested = true;
        if(fieldPVStructure->getSubField("valueAlarm")) valueAlarmRequested = true;
    }
    switch(ioType)
    {
         case getIO : break;
         case putIO:
              alarmRequested = false;
              timeStampRequested = false;
              displayRequested = false;
              controlRequested = false;
              valueAlarmRequested = false;
              break;
         case monitorIO: break;
    }
    StandardFieldPtr standardField = getStandardField();
    if(channelType==DBR_ENUM)
    {
        displayRequested = false;
        controlRequested = false;
        valueAlarmRequested = false;
        string properties;
        if(alarmRequested && timeStampRequested) {
            properties += "alarm,timeStamp";
        } else if(timeStampRequested) {
            properties += "timeStamp";
        } else if(alarmRequested) {
            properties += "alarm";
        }
        caRequestType = (properties.size()==0 ? DBR_ENUM : DBR_TIME_ENUM);
        structure = standardField->enumerated(properties);

        return;
    }
    ScalarType st = dbr2ST[channelType];
    PVStringPtr pvValue = fieldPVStructure->getSubField<PVString>("value._options.dbtype");
    if(pvValue)
    {
         std::string value(pvValue->get());
         if(value.find("DBF_UCHAR")!=std::string::npos) {
             if(st==pvByte) {
                 dbfIsUCHAR = true;
                 st = pvUByte;
                 caValueType = DBR_CHAR;
             }
         } else if(value.find("DBF_USHORT")!=std::string::npos) {
             if(st==pvInt) {
                 dbfIsUSHORT = true;
                 st = pvUShort;
                 caValueType = DBR_SHORT;
             }
         } else if(value.find("DBF_ULONG")!=std::string::npos) {
             if(st==pvDouble) {
                 dbfIsULONG = true;
                 st = pvUInt;
                 caValueType = DBR_LONG;
             }
         } else if(value.find("DBF_INT64")!=std::string::npos) {
             if(st==pvDouble) {
                 dbfIsINT64 = true;
                 st = pvLong;
             }
         } else if(value.find("DBF_UINT64")!=std::string::npos) {
             if(st==pvDouble) {
                 dbfIsUINT64 = true;
                 st = pvULong;
             }
         }

    }
    if(st==pvString) {
        displayRequested = false;
        controlRequested = false;
        valueAlarmRequested = false;
    }
    maxElements = ca_element_count(channelID);
    if(maxElements!=1) isArray = true;
    if(isArray)
    {
         controlRequested = false;
         valueAlarmRequested = false;
         if(channelType==DBR_CHAR && fieldPVStructure)
         {
             PVStringPtr pvValue = fieldPVStructure->getSubField<PVString>("value._options.pvtype");
             if(pvValue) {
                 std::string value(pvValue->get());
                 if(value.find("pvString")!=std::string::npos) {
                     charArrayIsString = true;
                     st = pvString;
                 }
             }
         }
    }

    if(controlRequested || displayRequested || valueAlarmRequested) timeStampRequested = false;
    FieldCreatePtr fieldCreate(FieldCreate::getFieldCreate());
    PVDataCreatePtr pvDataCreate(PVDataCreate::getPVDataCreate());
    FieldBuilderPtr fieldBuilder(fieldCreate->createFieldBuilder());
    if(valueRequested) {
        if(isArray && !charArrayIsString) {
           fieldBuilder->addArray("value",st);
        } else {
           fieldBuilder->add("value",st);
        }
    }
    if(alarmRequested) fieldBuilder->add("alarm",standardField->alarm());
    if(timeStampRequested) fieldBuilder->add("timeStamp",standardField->timeStamp());
    if(displayRequested) fieldBuilder->add("display",standardField->display());
    if(controlRequested) fieldBuilder->add("control",standardField->control());
    if(valueAlarmRequested) {
        switch(st)
        {
           case pvByte:
           case pvUByte:
               fieldBuilder->add("valueAlarm",standardField->byteAlarm()); break;
           case pvShort:
           case pvUShort:
               fieldBuilder->add("valueAlarm",standardField->shortAlarm()); break;
           case pvInt:
           case pvUInt:
               fieldBuilder->add("valueAlarm",standardField->intAlarm()); break;
           case pvFloat:
               fieldBuilder->add("valueAlarm",standardField->floatAlarm()); break;
           case pvDouble:
           case pvLong:
           case pvULong:
               fieldBuilder->add("valueAlarm",standardField->doubleAlarm()); break;
           default:
               throw  std::runtime_error("DbDToPv::activate: bad type");
        }
    }
    structure = fieldBuilder->createStructure();
    caRequestType = caValueType;
    if(displayRequested || controlRequested || valueAlarmRequested)
    {
       caRequestType = dbf_type_to_DBR_CTRL(caValueType);
    } else if(timeStampRequested || alarmRequested) {
       caRequestType = dbf_type_to_DBR_TIME(caValueType);
    } else {
       caRequestType = dbf_type_to_DBR(caValueType);
    }

}

chtype DbdToPv::getRequestType()
{
    if(caRequestType<0) {
       throw  std::runtime_error("DbDToPv::getRequestType: bad type");
    }
    return caRequestType;
}

Structure::const_shared_pointer DbdToPv::getStructure()
{
    return structure;
}


static void enumChoicesHandler(struct event_handler_args args)
{
    DbdToPv *dbdToPv = static_cast<DbdToPv*>(args.usr);
    dbdToPv->getChoicesDone(args);
}

void DbdToPv::getChoicesDone(struct event_handler_args &args)
{
    if(args.status!=ECA_NORMAL)
    {
        string message("DbdToPv::getChoicesDone ca_message ");
        message += ca_message(args.status);
        throw std::runtime_error(message);
    }
    const dbr_gr_enum* dbr_enum_p = static_cast<const dbr_gr_enum*>(args.dbr);
    size_t num = dbr_enum_p->no_str;
    choices.reserve(num);
    for(size_t i=0; i<num; ++i) choices.push_back(string(&dbr_enum_p->strs[i][0]));
    choicesEvent.trigger();
}


void DbdToPv::getChoices(CAChannelPtr const & caChannel)
{
    if(caRequestType==DBR_ENUM||caRequestType==DBR_TIME_ENUM)
    {
        chid channelID = caChannel->getChannelID();
        Attach to(caChannel->caContext());
        int result = ca_array_get_callback(DBR_GR_ENUM, 1,
            channelID, enumChoicesHandler, this);
        if (result == ECA_NORMAL) {
            result = ca_flush_io();
            choicesEvent.wait();
        } else {
            string mess(caChannel->getChannelName());
            mess += " DbdToPv::activate getting enum cnoices ";
            mess += ca_message(result);
            throw  std::runtime_error(mess);
        }
    }
}

PVStructurePtr DbdToPv::createPVStructure()
{
    return getPVDataCreate()->createPVStructure(structure);
}

template<typename dbrT, typename pvT>
void copy_DBRScalar(const void * dbr, PVScalar::shared_pointer const & pvScalar)
{
    std::tr1::shared_ptr<pvT> value = std::tr1::static_pointer_cast<pvT>(pvScalar);
    value->put(static_cast<const dbrT*>(dbr)[0]);
}

template<typename dbrT, typename pvT>
void copy_DBRScalarArray(const void * dbr, unsigned count, PVScalarArray::shared_pointer const & pvArray)
{
    std::tr1::shared_ptr<pvT> value = std::tr1::static_pointer_cast<pvT>(pvArray);
    typename pvT::svector temp(value->reuse());
    temp.resize(count);
    std::copy(
        static_cast<const dbrT*>(dbr),
        static_cast<const dbrT*>(dbr) + count,
        temp.begin());
    value->replace(freeze(temp));
}

template<typename dbrT>
void get_DBRControl(const void * dbr, double *upper_ctrl_limit,double *lower_ctrl_limit)
{
    *upper_ctrl_limit =  static_cast<const dbrT*>(dbr)->upper_ctrl_limit;
    *lower_ctrl_limit =  static_cast<const dbrT*>(dbr)->lower_ctrl_limit;
}

template<typename dbrT>
void get_DBRDisplay(
    const void * dbr, double *upper_disp_limit,double *lower_disp_limit,string *units)
{
    *upper_disp_limit =  static_cast<const dbrT*>(dbr)->upper_disp_limit;
    *lower_disp_limit =  static_cast<const dbrT*>(dbr)->lower_disp_limit;
     *units = static_cast<const dbrT*>(dbr)->units;
}

template<typename dbrT>
void get_DBRValueAlarm(
    const void * dbr,
    double *upper_alarm_limit,double *upper_warning_limit,
    double *lower_warning_limit,double *lower_alarm_limit)
{
    *upper_alarm_limit =  static_cast<const dbrT*>(dbr)->upper_alarm_limit;
    *upper_warning_limit =  static_cast<const dbrT*>(dbr)->upper_warning_limit;
    *lower_warning_limit =  static_cast<const dbrT*>(dbr)->lower_warning_limit;
    *lower_alarm_limit =  static_cast<const dbrT*>(dbr)->lower_alarm_limit;
}

Status DbdToPv::getFromDBD(
     PVStructurePtr const & pvStructure,
     BitSet::shared_pointer const & bitSet,
     struct event_handler_args &args)
{
   if(args.status!=ECA_NORMAL)
   {
     Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
     return errorStatus;
   }
   if(valueRequested)
   {
       void * value = dbr_value_ptr(args.dbr,caRequestType);
       if(isArray) {
           long count = args.count;
           GET_SUBFIELD_WITH_ERROR_CHECK(PVScalarArray, pvValue, pvStructure, "value", "DbdToPv::getFromDBD logic error");
           switch(caValueType) {
           case DBR_STRING:
           {
                const dbr_string_t *dbrval = static_cast<const dbr_string_t *>(value);
                GET_SUBFIELD_WITH_ERROR_CHECK(PVStringArray, pvValue, pvStructure, "value", "DbdToPv::getFromDBD logic error");
                PVStringArray::svector arr(pvValue->reuse());
                arr.resize(count);
                std::copy(dbrval, dbrval + count, arr.begin());
                pvValue->replace(freeze(arr));
                break;
           }
           case DBR_CHAR:
               if(charArrayIsString)
               {
                   const char * pchar = static_cast<const char *>(value);
                   std::string str(pchar);
                   GET_SUBFIELD_WITH_ERROR_CHECK(PVString, pvValue, pvStructure, "value", "DbdToPv::getFromDBD logic error");
                   pvValue->put(str);
                   break;
               }
               if(dbfIsUCHAR)
               {
                   copy_DBRScalarArray<dbr_char_t,PVUByteArray>(value,count,pvValue);
                   break;
               }
               copy_DBRScalarArray<dbr_char_t,PVByteArray>(value,count,pvValue);
               break;
           case DBR_SHORT:
               if(dbfIsUSHORT)
               {
                   copy_DBRScalarArray<dbr_short_t,PVUShortArray>(value,count,pvValue);
                   break;
               }
               copy_DBRScalarArray<dbr_short_t,PVShortArray>(value,count,pvValue);
               break;
           case DBR_LONG:
               if(dbfIsULONG)
               {
                   copy_DBRScalarArray<dbr_long_t,PVUIntArray>(value,count,pvValue);
                   break;
               }
               copy_DBRScalarArray<dbr_long_t,PVIntArray>(value,count,pvValue);
               break;
           case DBR_FLOAT:
               copy_DBRScalarArray<dbr_float_t,PVFloatArray>(value,count,pvValue);
               break;
           case DBR_DOUBLE:
               if(dbfIsINT64)
               {
                   copy_DBRScalarArray<dbr_double_t,PVLongArray>(value,count,pvValue);
                   break;
               }
               if(dbfIsUINT64)
               {
                   copy_DBRScalarArray<dbr_double_t,PVULongArray>(value,count,pvValue);
                   break;
               }
               copy_DBRScalarArray<dbr_double_t,PVDoubleArray>(value,count,pvValue);
               break;
           default:
                Status errorStatus(
                    Status::STATUSTYPE_ERROR, string("DbdToPv::getFromDBD logic error"));
                return errorStatus;
           }
       } else {
           PVScalarPtr pvValue = pvStructure->getSubField<PVScalar>("value");
           if(!pvValue && caValueType != DBR_ENUM) {
               return Status(Status::STATUSTYPE_ERROR, string("DbdToPv::getFromDBD logic error"));
           }
           switch(caValueType) {
           case DBR_ENUM:
           {
                const dbr_enum_t *dbrval = static_cast<const dbr_enum_t *>(value);
                GET_SUBFIELD_WITH_ERROR_CHECK(PVInt, value, pvStructure, "value.index", "DbdToPv::getFromDBD logic error");
                value->put(*dbrval);
                GET_SUBFIELD_WITH_ERROR_CHECK(PVStringArray, pvChoices, pvStructure, "value.choices", "DbdToPv::getFromDBD logic error");
                if(pvChoices->getLength()==0)
                {
                     ConvertPtr convert = getConvert();
                     size_t n = choices.size();
                     pvChoices->setLength(n);
                     convert->fromStringArray(pvChoices,0,n,choices,0);
                     bitSet->set(pvStructure->getSubField("value")->getFieldOffset());
                } else {
                     bitSet->set(value->getFieldOffset());
                }
                break;
           }
           case DBR_STRING: copy_DBRScalar<dbr_string_t,PVString>(value,pvValue); break;
           case DBR_CHAR:
                if(dbfIsUCHAR)
                {
                   copy_DBRScalar<dbr_char_t,PVUByte>(value,pvValue);
                   break;
                }
                copy_DBRScalar<dbr_char_t,PVByte>(value,pvValue); break;
           case DBR_SHORT:
                if(dbfIsUSHORT)
                {
                   copy_DBRScalar<dbr_short_t,PVUShort>(value,pvValue);
                   break;
                }
                copy_DBRScalar<dbr_short_t,PVShort>(value,pvValue); break;
           case DBR_LONG:
                if(dbfIsULONG)
                {
                   copy_DBRScalar<dbr_long_t,PVUInt>(value,pvValue);
                   break;
                }
                copy_DBRScalar<dbr_long_t,PVInt>(value,pvValue); break;
           case DBR_FLOAT: copy_DBRScalar<dbr_float_t,PVFloat>(value,pvValue); break;
           case DBR_DOUBLE:
                if(dbfIsINT64)
                {
                   copy_DBRScalar<dbr_double_t,PVLong>(value,pvValue);
                   break;
                }
                if(dbfIsUINT64)
                {
                   copy_DBRScalar<dbr_double_t,PVULong>(value,pvValue);
                   break;
                }
                copy_DBRScalar<dbr_double_t,PVDouble>(value,pvValue); break;
           default:
                Status errorStatus(
                    Status::STATUSTYPE_ERROR, string("DbdToPv::getFromDBD logic error"));
                return errorStatus;
           }
       }
       if(caValueType!=DBR_ENUM) {
            bitSet->set(pvStructure->getSubField("value")->getFieldOffset());
       }
    }
    if(alarmRequested) {
        // Note that status and severity are aways the first two members of DBR_
        const dbr_sts_string *data = static_cast<const dbr_sts_string *>(args.dbr);
        dbr_short_t status = data->status;
        dbr_short_t severity = data->severity;
        bool statusChanged = false;
        bool severityChanged = false;
        GET_SUBFIELD_WITH_ERROR_CHECK(PVStructure, pvAlarm, pvStructure, "alarm", "DbdToPv::getFromDBD logic error");
        GET_SUBFIELD_WITH_ERROR_CHECK(PVInt, pvSeverity, pvAlarm, "severity", "DbdToPv::getFromDBD logic error");
        if(caAlarm.severity!=severity) {
            caAlarm.severity = severity;
            pvSeverity->put(severity);
            severityChanged = true;
        }
        GET_SUBFIELD_WITH_ERROR_CHECK(PVString, pvMessage, pvAlarm, "message", "DbdToPv::getFromDBD logic error");
        GET_SUBFIELD_WITH_ERROR_CHECK(PVInt, pvStatus, pvAlarm, "status", "DbdToPv::getFromDBD logic error");
        if(caAlarm.status!=status) {
            caAlarm.status = status;
            pvStatus->put(convertDBstatus(status));
            string message("UNKNOWN STATUS");
            if(status<=ALARM_NSTATUS) message = string(epicsAlarmConditionStrings[status]);
            pvMessage->put(message);
            statusChanged = true;
        }
        if(statusChanged&&severityChanged) {
            bitSet->set(pvAlarm->getFieldOffset());
        } else if(severityChanged) {
            bitSet->set(pvSeverity->getFieldOffset());
        } else if(statusChanged) {
            bitSet->set(pvStatus->getFieldOffset());
            bitSet->set(pvMessage->getFieldOffset());
        }
    }
    if(timeStampRequested) {
        // Note that epicsTimeStamp always follows status and severity
        const dbr_time_string *data = static_cast<const dbr_time_string *>(args.dbr);
        epicsTimeStamp stamp = data->stamp;
        GET_SUBFIELD_WITH_ERROR_CHECK(PVStructure, pvTimeStamp, pvStructure, "timeStamp", "DbdToPv::getFromDBD logic error");
        if(caTimeStamp.secPastEpoch!=stamp.secPastEpoch) {
            caTimeStamp.secPastEpoch = stamp.secPastEpoch;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVLong, pvSeconds, pvTimeStamp, "secondsPastEpoch", "DbdToPv::getFromDBD logic error");
            pvSeconds->put(stamp.secPastEpoch+posixEpochAtEpicsEpoch);
            bitSet->set(pvSeconds->getFieldOffset());
        }
        if(caTimeStamp.nsec!=stamp.nsec) {
            caTimeStamp.nsec = stamp.nsec;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVInt, pvNano, pvTimeStamp, "nanoseconds", "DbdToPv::getFromDBD logic error");
            pvNano->put(stamp.nsec);
            bitSet->set(pvNano->getFieldOffset());
        }
    }
    if(controlRequested)
    {
         double upper_ctrl_limit = 0.0;
         double lower_ctrl_limit = 0.0;
         switch(caRequestType) {
             case DBR_CTRL_CHAR:
                 get_DBRControl<dbr_ctrl_char>(args.dbr,&upper_ctrl_limit,&lower_ctrl_limit); break;
             case DBR_CTRL_SHORT:
                 get_DBRControl<dbr_ctrl_short>(args.dbr,&upper_ctrl_limit,&lower_ctrl_limit); break;
             case DBR_CTRL_LONG:
                 get_DBRControl<dbr_ctrl_long>(args.dbr,&upper_ctrl_limit,&lower_ctrl_limit); break;
             case DBR_CTRL_FLOAT:
                 get_DBRControl<dbr_ctrl_float>(args.dbr,&upper_ctrl_limit,&lower_ctrl_limit); break;
             case DBR_CTRL_DOUBLE:
                 get_DBRControl<dbr_ctrl_double>(args.dbr,&upper_ctrl_limit,&lower_ctrl_limit); break;
             default :
                 throw  std::runtime_error("DbdToPv::getFromDBD logic error");
         }
         GET_SUBFIELD_WITH_ERROR_CHECK(PVStructure, pvControl, pvStructure, "control", "DbdToPv::getFromDBD logic error");
         if(caControl.upper_ctrl_limit!=upper_ctrl_limit) {
             caControl.upper_ctrl_limit = upper_ctrl_limit;
             GET_SUBFIELD_WITH_ERROR_CHECK(PVDouble, pv, pvControl, "limitHigh", "DbdToPv::getFromDBD logic error");
             pv->put(upper_ctrl_limit);
             bitSet->set(pv->getFieldOffset());
         }
         if(caControl.lower_ctrl_limit!=lower_ctrl_limit) {
             caControl.lower_ctrl_limit = lower_ctrl_limit;
             GET_SUBFIELD_WITH_ERROR_CHECK(PVDouble, pv, pvControl, "limitLow", "DbdToPv::getFromDBD logic error");
             pv->put(lower_ctrl_limit);
             bitSet->set(pv->getFieldOffset());
         }
    }
    if(displayRequested)
    {
        string units;
        string format;
        double upper_disp_limit = 0.0;
        double lower_disp_limit = 0.0;
        switch(caRequestType) {
             case DBR_CTRL_CHAR:
                 get_DBRDisplay<dbr_ctrl_char>(args.dbr,&upper_disp_limit,&lower_disp_limit,&units);
                 format = "I4"; break;
             case DBR_CTRL_SHORT:
                 get_DBRDisplay<dbr_ctrl_short>(args.dbr,&upper_disp_limit,&lower_disp_limit,&units);
                 format = "I6"; break;
             case DBR_CTRL_LONG:
                 get_DBRDisplay<dbr_ctrl_long>(args.dbr,&upper_disp_limit,&lower_disp_limit,&units);
                 format = "I12"; break;
             case DBR_CTRL_FLOAT:
                 get_DBRDisplay<dbr_ctrl_float>(args.dbr,&upper_disp_limit,&lower_disp_limit,&units);
                 {
                 const dbr_ctrl_float *data = static_cast<const dbr_ctrl_float *>(args.dbr);
                 int prec = data->precision;
                 ostringstream s;
                 s << "F" << prec + 6 << "." << prec;
                 format = s.str();
                 }
                 break;
             case DBR_CTRL_DOUBLE:
                 get_DBRDisplay<dbr_ctrl_double>(args.dbr,&upper_disp_limit,&lower_disp_limit,&units);
                 {
                 const dbr_ctrl_double *data = static_cast<const dbr_ctrl_double *>(args.dbr);
                 int prec = data->precision;
                 ostringstream s;
                 s << "F" << prec + 6 << "." << prec;
                 format = s.str();
                 }
                 break;
             default :
                 throw  std::runtime_error("DbdToPv::getFromDBD logic error");
         }
         GET_SUBFIELD_WITH_ERROR_CHECK(PVStructure, pvDisplay, pvStructure, "display", "DbdToPv::getFromDBD logic error");
         if(caDisplay.lower_disp_limit!=lower_disp_limit) {
            caDisplay.lower_disp_limit = lower_disp_limit;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVDouble, pv, pvDisplay, "limitLow", "DbdToPv::getFromDBD logic error");
            pv->put(lower_disp_limit);
            bitSet->set(pv->getFieldOffset());
         }
         if(caDisplay.upper_disp_limit!=upper_disp_limit) {
            caDisplay.upper_disp_limit = upper_disp_limit;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVDouble, pv, pvDisplay, "limitHigh", "DbdToPv::getFromDBD logic error");
            pv->put(upper_disp_limit);
            bitSet->set(pv->getFieldOffset());
         }
         if(caDisplay.units!=units) {
            caDisplay.units = units;
            PVStringPtr pv = pvDisplay->getSubField<PVString>("units");
            if(pv) {
                pv->put(units);
                bitSet->set(pv->getFieldOffset());
            }
         }
         if(caDisplay.format!=format) {
            caDisplay.format = format;
            PVStringPtr pv = pvDisplay->getSubField<PVString>("format");
            if(pv) {
                pv->put(format);
                bitSet->set(pv->getFieldOffset());
            }
         }
    }
    if(valueAlarmRequested) {
        double upper_alarm_limit = 0.0;
        double upper_warning_limit = 0.0;
        double lower_warning_limit = 0.0;
        double lower_alarm_limit = 0.0;
        switch(caRequestType) {
             case DBR_CTRL_CHAR:
                 get_DBRValueAlarm<dbr_ctrl_char>(args.dbr,
                     &upper_alarm_limit,&upper_warning_limit,
                     &lower_warning_limit,&lower_alarm_limit);
                 break;
             case DBR_CTRL_SHORT:
                 get_DBRValueAlarm<dbr_ctrl_short>(args.dbr,
                     &upper_alarm_limit,&upper_warning_limit,
                     &lower_warning_limit,&lower_alarm_limit);
                 break;
             case DBR_CTRL_LONG:
                 get_DBRValueAlarm<dbr_ctrl_long>(args.dbr,
                     &upper_alarm_limit,&upper_warning_limit,
                     &lower_warning_limit,&lower_alarm_limit);
                 break;
             case DBR_CTRL_FLOAT:
                 get_DBRValueAlarm<dbr_ctrl_float>(args.dbr,
                     &upper_alarm_limit,&upper_warning_limit,
                     &lower_warning_limit,&lower_alarm_limit);
                 break;
             case DBR_CTRL_DOUBLE:
                 get_DBRValueAlarm<dbr_ctrl_double>(args.dbr,
                     &upper_alarm_limit,&upper_warning_limit,
                     &lower_warning_limit,&lower_alarm_limit);
                 break;
             default :
                 throw  std::runtime_error("DbdToPv::getFromDBD logic error");
        }
        ConvertPtr convert(getConvert());
        GET_SUBFIELD_WITH_ERROR_CHECK(PVStructure, pvValueAlarm, pvStructure, "valueAlarm", "DbdToPv::getFromDBD logic error");
        if(caValueAlarm.upper_alarm_limit!=upper_alarm_limit) {
            caValueAlarm.upper_alarm_limit = upper_alarm_limit;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVScalar, pv, pvValueAlarm, "highAlarmLimit", "DbdToPv::getFromDBD logic error");
            convert->fromDouble(pv,upper_alarm_limit);
            bitSet->set(pv->getFieldOffset());
        }
        if(caValueAlarm.upper_warning_limit!=upper_warning_limit) {
            caValueAlarm.upper_warning_limit = upper_warning_limit;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVScalar, pv, pvValueAlarm, "highWarningLimit", "DbdToPv::getFromDBD logic error");
            convert->fromDouble(pv,upper_warning_limit);
            bitSet->set(pv->getFieldOffset());
        }
        if(caValueAlarm.lower_warning_limit!=lower_warning_limit) {
            caValueAlarm.lower_warning_limit = lower_warning_limit;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVScalar, pv, pvValueAlarm, "lowWarningLimit", "DbdToPv::getFromDBD logic error");
            convert->fromDouble(pv,lower_warning_limit);
            bitSet->set(pv->getFieldOffset());
        }
        if(caValueAlarm.lower_alarm_limit!=lower_alarm_limit) {
            caValueAlarm.lower_alarm_limit = lower_alarm_limit;
            GET_SUBFIELD_WITH_ERROR_CHECK(PVScalar, pv, pvValueAlarm, "lowAlarmLimit", "DbdToPv::getFromDBD logic error");
            convert->fromDouble(pv,lower_alarm_limit);
            bitSet->set(pv->getFieldOffset());
        }
    }
    if(firstTime) {
        firstTime = false;
        bitSet->clear();
        bitSet->set(0);
    }
    return Status::Ok;
}



template<typename dbrT, typename pvT>
const void * put_DBRScalar(dbrT *val,PVScalar::shared_pointer const & pvScalar)
{
    if(pvScalar) {
        std::tr1::shared_ptr<pvT> value = std::tr1::static_pointer_cast<pvT>(pvScalar);
        *val = value->get();
    }
    return val;
}

template<typename dbrT, typename pvT>
const void * put_DBRScalarArray(unsigned long*count, PVScalarArray::shared_pointer const & pvArray)
{
    if(pvArray) {
        std::tr1::shared_ptr<pvT> value = std::tr1::static_pointer_cast<pvT>(pvArray);
        *count = value->getLength();
        return value->view().data();
    }
    return NULL;
}


Status DbdToPv::putToDBD(
     CAChannelPtr const & caChannel,
     PVStructurePtr const & pvStructure,
     bool block,
     caCallbackFunc putHandler,
     void * userarg)
{
    chid channelID = caChannel->getChannelID();
    const void *pValue = NULL;
    unsigned long count = 1;
    char *ca_stringBuffer(0);
    dbr_char_t   bvalue(0);
    dbr_short_t  svalue(0);
    dbr_enum_t   evalue(0);
    dbr_long_t   lvalue(0);
    dbr_float_t  fvalue(0);
    dbr_double_t dvalue(0);
    if(isArray) {
       GET_SUBFIELD_WITH_ERROR_CHECK(PVScalarArray, pvValue, pvStructure, "value", "DbdToPv::putToDBD logic error");
       switch(caValueType) {
           case DBR_STRING:
           {
               GET_SUBFIELD_WITH_ERROR_CHECK(PVStringArray, pvValue, pvStructure, "value", "DbdToPv::putToDBD logic error");
               count = pvValue->getLength();
               if(count<1) break;
               if(count>maxElements) count = maxElements;
               int nbytes = count*MAX_STRING_SIZE;
               ca_stringBuffer = new char[nbytes];
               memset(ca_stringBuffer, 0, nbytes);
               pValue = ca_stringBuffer;
               PVStringArray::const_svector stringArray(pvValue->view());
               char  *pnext = ca_stringBuffer;
               for(size_t i=0; i<count; ++i) {
                   string value = stringArray[i];
                   size_t len = value.length();
                   if (len >= MAX_STRING_SIZE) len = MAX_STRING_SIZE - 1;
                   memcpy(pnext, value.c_str(), len);
                   pnext += MAX_STRING_SIZE;
               }
               break;
           }
           case DBR_CHAR:
               if(charArrayIsString)
               {
                   GET_SUBFIELD_WITH_ERROR_CHECK(PVString, pvValue, pvStructure, "value", "DbdToPv::putToDBD logic error");
                   const char * pchar = pvValue->get().c_str();
                   pValue = pchar;
                   count = pvValue->get().length();
                   break;
               }
               if(dbfIsUCHAR)
               {
                    pValue = put_DBRScalarArray<dbr_char_t,PVUByteArray>(&count,pvValue);
                    break;
               }
               pValue = put_DBRScalarArray<dbr_char_t,PVByteArray>(&count,pvValue);
               break;
           case DBR_SHORT:
               if(dbfIsUSHORT)
               {
                    pValue = put_DBRScalarArray<dbr_short_t,PVUShortArray>(&count,pvValue);
                    break;
               }
               pValue = put_DBRScalarArray<dbr_short_t,PVShortArray>(&count,pvValue);
               break;
           case DBR_LONG:
               if(dbfIsULONG)
               {
                    pValue = put_DBRScalarArray<dbr_long_t,PVUIntArray>(&count,pvValue);
                    break;
               }
               pValue = put_DBRScalarArray<dbr_long_t,PVIntArray>(&count,pvValue);
               break;
           case DBR_FLOAT:
               pValue = put_DBRScalarArray<dbr_float_t,PVFloatArray>(&count,pvValue);
               break;
           case DBR_DOUBLE:
               if(dbfIsINT64)
               {
                   GET_SUBFIELD_WITH_ERROR_CHECK(PVLongArray, pvValue, pvStructure, "value", "DbdToPv::putToDBD logic error");
                   PVLongArray::const_svector sv(pvValue->view());
                   pvDoubleArray = PVDoubleArrayPtr(getPVDataCreate()->createPVScalarArray<PVDoubleArray>());
                   pvDoubleArray->putFrom(sv);
                   const double * pdouble = pvDoubleArray->view().data();
                   count = pvValue->getLength();
                   pValue = pdouble;
                   break;
               }
               if(dbfIsUINT64)
               {
                   GET_SUBFIELD_WITH_ERROR_CHECK(PVULongArray, pvValue, pvStructure, "value", "DbdToPv::putToDBD logic error");
                   PVULongArray::const_svector sv(pvValue->view());
                   pvDoubleArray = PVDoubleArrayPtr(getPVDataCreate()->createPVScalarArray<PVDoubleArray>());
                   pvDoubleArray->putFrom(sv);
                   const double * pdouble = pvDoubleArray->view().data();
                   count = pvValue->getLength();
                   pValue = pdouble;
                   break;
               }
               pValue = put_DBRScalarArray<dbr_double_t,PVDoubleArray>(&count,pvValue);
               break;
           default:
                Status errorStatus(
                    Status::STATUSTYPE_ERROR, string("DbdToPv::putToDBD logic error"));
                return errorStatus;
           }
           if(!pValue) {
               return Status(Status::STATUSTYPE_ERROR, string("DbdToPv::putToDBD logic error"));
           }
    } else {
        PVScalarPtr pvValue = pvStructure->getSubField<PVScalar>("value");
        if(!pvValue && caValueType != DBR_ENUM) {
            return Status(Status::STATUSTYPE_ERROR, string("DbdToPv::putToDBD logic error"));
        }
        switch(caValueType) {
           case DBR_ENUM:
           {
               GET_SUBFIELD_WITH_ERROR_CHECK(PVInt, pvValue, pvStructure, "value.index", "DbdToPv::putToDBD logic error");
               pValue = put_DBRScalar<dbr_enum_t,PVUShort>(&evalue,pvValue);
               break;
           }
           case DBR_STRING: 
           {
               GET_SUBFIELD_WITH_ERROR_CHECK(PVString, pvValue, pvStructure, "value", "DbdToPv::putToDBD logic error");
               pValue = pvValue->get().c_str();
               break;
           }
           case DBR_CHAR:
               if(dbfIsUCHAR)
               {
                    pValue = put_DBRScalar<dbr_char_t,PVUByte>(&bvalue,pvValue);
                    break;
               }
               pValue = put_DBRScalar<dbr_char_t,PVByte>(&bvalue,pvValue); break;
           case DBR_SHORT:
               if(dbfIsUSHORT)
               {
                    pValue = put_DBRScalar<dbr_short_t,PVUShort>(&svalue,pvValue);
                    break;
               }
               pValue = put_DBRScalar<dbr_short_t,PVShort>(&svalue,pvValue); break;
           case DBR_LONG:
               if(dbfIsULONG)
               {
                    pValue = put_DBRScalar<dbr_long_t,PVUInt>(&lvalue,pvValue);
                    break;
               }
               pValue = put_DBRScalar<dbr_long_t,PVInt>(&lvalue,pvValue); break;
           case DBR_FLOAT: pValue = put_DBRScalar<dbr_float_t,PVFloat>(&fvalue,pvValue); break;
           case DBR_DOUBLE:
               if(dbfIsINT64)
               {
                    pValue = put_DBRScalar<dbr_double_t,PVLong>(&dvalue,pvValue);
                    break;
               }
               if(dbfIsUINT64)
               {
                    pValue = put_DBRScalar<dbr_double_t,PVULong>(&dvalue,pvValue);
                    break;
               }
               pValue = put_DBRScalar<dbr_double_t,PVDouble>(&dvalue,pvValue); break;
           default:
                Status errorStatus(
                    Status::STATUSTYPE_ERROR, string("DbdToPv::putToDBD logic error"));
                return errorStatus;
         }
         if(!pValue) {
             return Status(Status::STATUSTYPE_ERROR, string("DbdToPv::putToDBD logic error"));
         }
    }
    Status status = Status::Ok;
    int result = 0;
    Attach to(caChannel->caContext());
    if (block) {
        result = ca_array_put_callback(caValueType,count,channelID,pValue,putHandler,userarg);
    }
    else {
        result = ca_array_put(caValueType,count,channelID,pValue);
    }
    if (result == ECA_NORMAL) {
        ca_flush_io();
    }
    else {
        status = Status(Status::STATUSTYPE_ERROR, string(ca_message(result)));
    }
    if (ca_stringBuffer != NULL)
        delete[] ca_stringBuffer;
    return status;
}

#undef GET_SUBFIELD_WITH_ERROR_CHECK

}}}
