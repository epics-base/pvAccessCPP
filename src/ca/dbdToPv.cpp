/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#include <epicsVersion.h>
#include <sstream>
#include <alarm.h>
#include <alarmString.h>

#include <pv/standardField.h>
#include <pv/logger.h>
#include <pv/pvAccess.h>
#include <pv/reftrack.h>
#include <pv/convert.h>
#include <pv/timeStamp.h>
#include "caChannel.h"
#define epicsExportSharedSymbols
#include "dbdToPv.h"

using namespace epics::pvData;
using std::string;
using std::ostringstream;

namespace epics {
namespace pvAccess {
namespace ca {

#define CA_TIMEOUT 2.0
#define CA_PRIORITY 50

static void enumChoicesHandler(struct event_handler_args args)
{
    DbdToPv *dbdToPv = static_cast<DbdToPv*>(args.usr);
    dbdToPv->getChoicesDone(args);
}

static void description_connection_handler(struct connection_handler_args args)
{
    DbdToPv *dbdToPv = static_cast<DbdToPv*>(ca_puser(args.chid));
    dbdToPv->descriptionConnected(args);
}

static void descriptionHandler(struct event_handler_args args)
{
    DbdToPv *dbdToPv = static_cast<DbdToPv*>(args.usr);
    dbdToPv->getDescriptionDone(args);
}

static void putHandler(struct event_handler_args args)
{
    DbdToPv *dbdToPv = static_cast<DbdToPv*>(args.usr);
    dbdToPv->putDone(args);
}

DbdToPvPtr DbdToPv::create(
    CAChannelPtr const & caChannel,
    PVStructurePtr const & pvRequest,
    IOType ioType
    )
{
    DbdToPvPtr dbdToPv(new DbdToPv(ioType));
    dbdToPv->activate(caChannel,pvRequest);
    return dbdToPv;
}

DbdToPv::DbdToPv(IOType ioType)
:  ioType(ioType),
   fieldRequested(false),
   alarmRequested(false),
   timeStampRequested(false),
   displayRequested(false),
   controlRequested(false),
   valueAlarmRequested(false),
   isArray(false),
   firstTime(true),
   caValueType(-1),
   caRequestType(-1)
{
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
         case pvShort : return DBR_SHORT;
         case pvInt : return DBR_LONG;
         case pvFloat : return DBR_FLOAT;
         case pvDouble : return DBR_DOUBLE;
         default: break;
    }
    throw  std::runtime_error("getDbr: illegal scalarType");
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
        fieldRequested = true;
        alarmRequested = true;
        timeStampRequested = true;
        displayRequested = true;
        controlRequested = true;
        valueAlarmRequested = true;
    } else {
        if(fieldPVStructure->getSubField("value")) fieldRequested = true;
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
              timeStampRequested = false; // no break
         case monitorIO:
              displayRequested = false;
              controlRequested = false;
              valueAlarmRequested = false;
    }
    StandardFieldPtr standardField = getStandardField();
    if(channelType==DBR_ENUM)
    {
        displayRequested = false;
        controlRequested = false;
        valueAlarmRequested = false;
        caRequestType = DBR_ENUM;
        string properties;
        if(alarmRequested && timeStampRequested) {
            properties += "alarm,timeStamp";
            caRequestType += DBR_TIME_STRING  + caValueType;
        } else if(timeStampRequested) {
            properties += "timeStamp";
            caRequestType += DBR_TIME_STRING  + caValueType;
        } else if(alarmRequested) {
            properties += "alarm";
            caRequestType += DBR_STS_STRING  + caValueType;
        }
        structure = standardField->enumerated(properties);
        int result = ca_array_get_callback(DBR_GR_ENUM,
               1,
               channelID, enumChoicesHandler, this);
        if (result == ECA_NORMAL) result = ca_flush_io();
        if (result != ECA_NORMAL) {
            string mess(caChannel->getChannelName());
            mess += " DbdToPv::activate getting enum cnoices ";
            mess += ca_message(result);
            throw  std::runtime_error(mess);
        }
        // NOTE: we do not wait here, since all subsequent request (over TCP) is serialized
        // and will guarantee that enumChoicesHandler is called first
        return;
    }
    if(ca_element_count(channelID)!=1) isArray = true;
    if(isArray)
    {
         controlRequested = false;
         valueAlarmRequested = false;
    }
    ScalarType st = dbr2ST[channelType];
    if(st==pvString) {
        displayRequested = false;
        controlRequested = false;
        valueAlarmRequested = false;
    }
    if(controlRequested || displayRequested || valueAlarmRequested) timeStampRequested = false;
    
    FieldCreatePtr fieldCreate(FieldCreate::getFieldCreate());
    PVDataCreatePtr pvDataCreate(PVDataCreate::getPVDataCreate());
    FieldBuilderPtr fieldBuilder(fieldCreate->createFieldBuilder());
    if(fieldRequested) {
        if(isArray) {
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
               fieldBuilder->add("valueAlarm",standardField->byteAlarm()); break;
           case pvShort:
               fieldBuilder->add("valueAlarm",standardField->shortAlarm()); break;
           case pvInt:
               fieldBuilder->add("valueAlarm",standardField->intAlarm()); break;
           case pvFloat:
               fieldBuilder->add("valueAlarm",standardField->floatAlarm()); break;
           case pvDouble:
               fieldBuilder->add("valueAlarm",standardField->doubleAlarm()); break;
           default:
               throw  std::runtime_error("DbDToPv::activate: bad type");
        }
    }
    structure = fieldBuilder->createStructure();
    caRequestType = caValueType;
    if(displayRequested || controlRequested || valueAlarmRequested)
    {
       caRequestType =  DBR_CTRL_STRING + caValueType;  
    } else if(timeStampRequested) {
       caRequestType = DBR_TIME_STRING  + caValueType;
    } else if(alarmRequested) {
       caRequestType = DBR_STS_STRING  + caValueType;
    }
    if(displayRequested) {
         chid channelID;
         string name(caChannel->getChannelName() + ".DESC");
         int result = ca_create_channel(name.c_str(),
             description_connection_handler,
             this,
             CA_PRIORITY, // TODO mapping
             &channelID);
         if (result == ECA_NORMAL) result = ca_flush_io();
         if (result != ECA_NORMAL) {
            string mess(caChannel->getChannelName());
            mess += " DbdToPv::activate getting description ";
            mess += ca_message(result);
            throw  std::runtime_error(mess);
        }
    }
}

void DbdToPv::descriptionConnected(struct connection_handler_args args)
{
    if (args.op != CA_OP_CONN_UP) return;
    ca_array_get_callback(DBR_STRING,
         0,
         args.chid, descriptionHandler, this);
}

void DbdToPv::getDescriptionDone(struct event_handler_args &args)
{
    if(args.status!=ECA_NORMAL) return;
    const dbr_string_t *value = static_cast<const dbr_string_t *>(dbr_value_ptr(args.dbr,DBR_STRING));
    description = string(*value);
    ca_clear_channel(args.chid);
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
} 

chtype DbdToPv::getRequestType()
{
    if(caRequestType<0) {
       throw  std::runtime_error("DbDToPv::getRequestType: bad type");
    }
    return caRequestType;
}

PVStructurePtr DbdToPv::createPVStructure()
{
    return getPVDataCreate()->createPVStructure(structure);
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
   if(fieldRequested)
   {
       void * value = dbr_value_ptr(args.dbr,caRequestType);
       long count = args.count;
       switch(caValueType) {
           case DBR_ENUM:
           {
                const dbr_enum_t *dbrval = static_cast<const dbr_enum_t *>(value);
                PVIntPtr value = pvStructure->getSubField<PVInt>("value.index");
                value->put(*dbrval);
                PVStringArrayPtr pvChoices
                     = pvStructure->getSubField<PVStringArray>("value.choices");
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
           case DBR_STRING:
           {
                const dbr_string_t *dbrval = static_cast<const dbr_string_t *>(value);
                if(isArray) {
                    PVStringArrayPtr pvValue = pvStructure->getSubField<PVStringArray>("value");
                    PVStringArray::svector arr(pvValue->reuse());
                    arr.resize(count);
                    std::copy(dbrval, dbrval + count, arr.begin());
                    pvValue->replace(freeze(arr));
                } else {
                    PVStringPtr pvValue = pvStructure->getSubField<PVString>("value");
                    pvValue->put(*dbrval);
                }
                break;
           }
           case DBR_CHAR:
           {
               const dbr_char_t *dbrval = static_cast<const dbr_char_t *>(value);
               if(isArray) {
                    PVByteArrayPtr pvValue = pvStructure->getSubField<PVByteArray>("value");
                    PVByteArray::svector arr(pvValue->reuse());
                    arr.resize(count);
                    for(long i=0; i<count; ++i) arr[i] = *(dbrval++);
                    pvValue->replace(freeze(arr));
               } else {
                   PVBytePtr pvValue = pvStructure->getSubField<PVByte>("value");
                   pvValue->put(*dbrval);
               }
               break;
           }
           case DBR_SHORT:
           {
                const dbr_short_t *dbrval = static_cast<const dbr_short_t *>(value);
                if(isArray) {
                    PVShortArrayPtr pvValue = pvStructure->getSubField<PVShortArray>("value");
                    PVShortArray::svector arr(pvValue->reuse());
                    arr.resize(count);
                    for(long i=0; i<count; ++i) arr[i] = *(dbrval++);
                    pvValue->replace(freeze(arr));
                } else {
                    PVShortPtr pvValue = pvStructure->getSubField<PVShort>("value");
                    pvValue->put(*dbrval);
                }
                break;
           }
           case DBR_LONG:
           {
                const dbr_int_t *dbrval = static_cast<const dbr_int_t *>(value);
                if(isArray) {
                    PVIntArrayPtr pvValue = pvStructure->getSubField<PVIntArray>("value");
                    PVIntArray::svector arr(pvValue->reuse());
                    arr.resize(count);
                    for(long i=0; i<count; ++i) arr[i] = *(dbrval++);
                    pvValue->replace(freeze(arr));
                } else {
                    PVIntPtr pvValue = pvStructure->getSubField<PVInt>("value");
                    pvValue->put(*dbrval);
                }
                break;
           }
           case DBR_FLOAT:
           {
                const dbr_float_t *dbrval = static_cast<const dbr_float_t *>(value);
                if(isArray) {
                    PVFloatArrayPtr pvValue = pvStructure->getSubField<PVFloatArray>("value");
                    PVFloatArray::svector arr(pvValue->reuse());
                    arr.resize(count);
                    for(long i=0; i<count; ++i) arr[i] = *(dbrval++);
                    pvValue->replace(freeze(arr));
                } else {
                    PVFloatPtr pvValue = pvStructure->getSubField<PVFloat>("value");
                    pvValue->put(*dbrval);
                }
                break;
           }
           case DBR_DOUBLE:
           {
                const dbr_double_t *dbrval = static_cast<const dbr_double_t *>(value);
                if(isArray) {
                    PVDoubleArrayPtr pvValue 
                         = pvStructure->getSubField<PVDoubleArray>("value");
                    PVDoubleArray::svector arr(pvValue->reuse());
                    arr.resize(count);
                    for(long i=0; i<count; ++i) arr[i] = *(dbrval++);
                    pvValue->replace(freeze(arr));
                } else {
                    PVDoublePtr pvValue = pvStructure->getSubField<PVDouble>("value");
                    pvValue->put(*dbrval);
                }
                break;
           }
           default:
                Status errorStatus(
                    Status::STATUSTYPE_ERROR, string("DbdToPv::FromDBD logic error"));
                return errorStatus;
       }
       if(caValueType!=DBR_ENUM) {
            bitSet->set(pvStructure->getSubField("value")->getFieldOffset());
       }
    }
    chtype type = args.type;
    dbr_short_t	status = 0;
    dbr_short_t	severity = 0;
    epicsTimeStamp stamp = {0,0};
    if(caRequestType>=DBR_CTRL_STRING) {
        string units;
        string format;
        double upper_disp_limit = 0.0;
        double lower_disp_limit = 0.0;
        double upper_alarm_limit = 0.0;
        double upper_warning_limit = 0.0;
        double lower_warning_limit = 0.0;
        double lower_alarm_limit = 0.0;
        double upper_ctrl_limit = 0.0;
        double lower_ctrl_limit = 0.0;
        switch(type) {
            case DBR_CTRL_CHAR:
            {
               const dbr_ctrl_char *data = static_cast<const dbr_ctrl_char *>(args.dbr);
               status = data->status;
               severity = data->severity;
               units = data->units;
               upper_disp_limit = data->upper_disp_limit;
               lower_disp_limit = data->lower_disp_limit;
               upper_alarm_limit = data->upper_alarm_limit;
               upper_warning_limit = data->upper_warning_limit;
               lower_warning_limit = data->lower_warning_limit;
               lower_alarm_limit = data->lower_alarm_limit;
               upper_ctrl_limit = data->upper_ctrl_limit;
               lower_ctrl_limit = data->lower_ctrl_limit;
               format = "I4";
               break;
           }
           case DBR_CTRL_SHORT:
           {
               const dbr_ctrl_short *data = static_cast<const dbr_ctrl_short *>(args.dbr);
               status = data->status;
               severity = data->severity;
               units = data->units;
               upper_disp_limit = data->upper_disp_limit;
               lower_disp_limit = data->lower_disp_limit;
               upper_alarm_limit = data->upper_alarm_limit;
               upper_warning_limit = data->upper_warning_limit;
               lower_warning_limit = data->lower_warning_limit;
               lower_alarm_limit = data->lower_alarm_limit;
               upper_ctrl_limit = data->upper_ctrl_limit;
               lower_ctrl_limit = data->lower_ctrl_limit;
               format = "I6";
               break;
           }
           case DBR_CTRL_LONG:
           {
               const dbr_ctrl_long *data = static_cast<const dbr_ctrl_long *>(args.dbr);
               status = data->status;
               severity = data->severity;
               units = data->units;
               upper_disp_limit = data->upper_disp_limit;
               lower_disp_limit = data->lower_disp_limit;
               upper_alarm_limit = data->upper_alarm_limit;
               upper_warning_limit = data->upper_warning_limit;
               lower_warning_limit = data->lower_warning_limit;
               lower_alarm_limit = data->lower_alarm_limit;
               upper_ctrl_limit = data->upper_ctrl_limit;
               lower_ctrl_limit = data->lower_ctrl_limit;
               format = "I12";
               break;
           }
           case DBR_CTRL_FLOAT:
           {
               const dbr_ctrl_float *data = static_cast<const dbr_ctrl_float *>(args.dbr);
               status = data->status;
               severity = data->severity;
               units = data->units;
               upper_disp_limit = data->upper_disp_limit;
               lower_disp_limit = data->lower_disp_limit;
               upper_alarm_limit = data->upper_alarm_limit;
               upper_warning_limit = data->upper_warning_limit;
               lower_warning_limit = data->lower_warning_limit;
               lower_alarm_limit = data->lower_alarm_limit;
               upper_ctrl_limit = data->upper_ctrl_limit;
               lower_ctrl_limit = data->lower_ctrl_limit;
               int prec = data->precision;
               ostringstream s;
               s << "F" << prec + 6 << "." << prec;
               format = s.str();
               break;
           }
           case DBR_CTRL_DOUBLE:
           {
               const dbr_ctrl_double *data = static_cast<const dbr_ctrl_double *>(args.dbr);
               status = data->status;
               severity = data->severity;
               units = data->units;
               upper_disp_limit = data->upper_disp_limit;
               lower_disp_limit = data->lower_disp_limit;
               upper_alarm_limit = data->upper_alarm_limit;
               upper_warning_limit = data->upper_warning_limit;
               lower_warning_limit = data->lower_warning_limit;
               lower_alarm_limit = data->lower_alarm_limit;
               upper_ctrl_limit = data->upper_ctrl_limit;
               lower_ctrl_limit = data->lower_ctrl_limit;
               int prec = data->precision;
               ostringstream s;
               s << "F" << prec + 6 << "." << prec;
               format = s.str();
               break;
           }
           default :
              throw  std::runtime_error("DbdToPv::getDone logic error");
        }
        if(displayRequested)
        {
             PVStructurePtr pvDisplay(pvStructure->getSubField<PVStructure>("display"));
             if(caDisplay.lower_disp_limit!=lower_disp_limit) {
                caDisplay.lower_disp_limit = lower_disp_limit;
                PVDoublePtr pvDouble = pvDisplay->getSubField<PVDouble>("limitLow");
                pvDouble->put(lower_disp_limit);
                bitSet->set(pvDouble->getFieldOffset());
             }
             if(caDisplay.upper_disp_limit!=upper_disp_limit) {
                caDisplay.upper_disp_limit = upper_disp_limit;
                PVDoublePtr pvDouble = pvDisplay->getSubField<PVDouble>("limitHigh");
                pvDouble->put(upper_disp_limit);
                bitSet->set(pvDouble->getFieldOffset());
             }
             if(caDisplay.units!=units) {
                caDisplay.units = units;
                PVStringPtr pvString = pvDisplay->getSubField<PVString>("units");
                pvString->put(units);
                bitSet->set(pvString->getFieldOffset());
             }
             if(caDisplay.format!=format) {
                caDisplay.format = format;
                PVStringPtr pvString = pvDisplay->getSubField<PVString>("format");
                pvString->put(format);
                bitSet->set(pvString->getFieldOffset());
             }
             if(!description.empty())
             {
                 PVStringPtr pvString = pvDisplay->getSubField<PVString>("description");
                 if(description.compare(pvString->get()) !=0) {
                      pvString->put(description);
                      bitSet->set(pvString->getFieldOffset());
                 }
             }
        }
        if(controlRequested)
        {
             PVStructurePtr pvControl(pvStructure->getSubField<PVStructure>("control"));
             if(caControl.upper_ctrl_limit!=upper_ctrl_limit) {
                caControl.upper_ctrl_limit = upper_ctrl_limit;
                PVDoublePtr pv = pvControl->getSubField<PVDouble>("limitHigh");
                pv->put(upper_ctrl_limit);
                bitSet->set(pv->getFieldOffset());
             }
             if(caControl.lower_ctrl_limit!=lower_ctrl_limit) {
                caControl.lower_ctrl_limit = lower_ctrl_limit;
                PVDoublePtr pv = pvControl->getSubField<PVDouble>("limitLow");
                pv->put(lower_ctrl_limit);
                bitSet->set(pv->getFieldOffset());
             }
        }
        if(valueAlarmRequested) {
             ConvertPtr convert(getConvert()); 
             PVStructurePtr pvValueAlarm(pvStructure->getSubField<PVStructure>("valueAlarm"));
             if(caValueAlarm.upper_alarm_limit!=upper_alarm_limit) {
                caValueAlarm.upper_alarm_limit = upper_alarm_limit;
                PVScalarPtr pv = pvValueAlarm->getSubField<PVScalar>("highAlarmLimit");
                convert->fromDouble(pv,upper_alarm_limit);
                bitSet->set(pv->getFieldOffset());
             }
             if(caValueAlarm.upper_warning_limit!=upper_warning_limit) {
                caValueAlarm.upper_warning_limit = upper_warning_limit;
                PVScalarPtr pv = pvValueAlarm->getSubField<PVScalar>("highWarningLimit");
                convert->fromDouble(pv,upper_warning_limit);
                bitSet->set(pv->getFieldOffset());
             }
             if(caValueAlarm.lower_warning_limit!=lower_warning_limit) {
                caValueAlarm.lower_warning_limit = lower_warning_limit;
                PVScalarPtr pv = pvValueAlarm->getSubField<PVScalar>("lowWarningLimit");
                convert->fromDouble(pv,lower_warning_limit);
                bitSet->set(pv->getFieldOffset());
             }
             if(caValueAlarm.lower_alarm_limit!=lower_alarm_limit) {
                caValueAlarm.lower_alarm_limit = lower_alarm_limit;
                PVScalarPtr pv = pvValueAlarm->getSubField<PVScalar>("lowAlarmLimit");
                convert->fromDouble(pv,lower_alarm_limit);
                bitSet->set(pv->getFieldOffset());
             }
        }
    } else if(caRequestType>=DBR_TIME_STRING) {
        switch(type) {
            case DBR_TIME_STRING:
            {
                const dbr_time_string *data = static_cast<const dbr_time_string *>(args.dbr);
                status = data->status;
                severity = data->severity;
                stamp = data->stamp;
                break;
            }
            case DBR_TIME_CHAR:
            {
                const dbr_time_char *data = static_cast<const dbr_time_char *>(args.dbr);
                status = data->status;
                severity = data->severity;
                stamp = data->stamp;
                break;
            }
            case DBR_TIME_SHORT:
            {
                const dbr_time_short *data = static_cast<const dbr_time_short *>(args.dbr);
                status = data->status;
                severity = data->severity;
                stamp = data->stamp;
                break;
            }
            case DBR_TIME_LONG:
            {
                const dbr_time_long *data = static_cast<const dbr_time_long *>(args.dbr);
                status = data->status;
                severity = data->severity;
                stamp = data->stamp;
                break;
            }
            case DBR_TIME_FLOAT:
            {
                const dbr_time_float *data = static_cast<const dbr_time_float *>(args.dbr);
                status = data->status;
                severity = data->severity;
                stamp = data->stamp;
                break;
            }
            case DBR_TIME_DOUBLE:
            {
                const dbr_time_double *data = static_cast<const dbr_time_double *>(args.dbr);
                status = data->status;
                severity = data->severity;
                stamp = data->stamp;
                break;
            }
            default:
                throw  std::runtime_error("DbdToPv::getDone logic error");
        }
    } else if(caRequestType>=DBR_STS_STRING) {
        switch(type) {
            case DBR_STS_STRING:
            {
                const dbr_sts_string *data = static_cast<const dbr_sts_string *>(args.dbr);
                status = data->status;
                severity = data->severity;
                break;
            }
            case DBR_STS_CHAR:
            {
                const dbr_sts_char *data = static_cast<const dbr_sts_char *>(args.dbr);
                status = data->status;
                severity = data->severity;
                break;
            }
            case DBR_STS_SHORT:
            {
                const dbr_sts_short *data = static_cast<const dbr_sts_short *>(args.dbr);
                status = data->status;
                severity = data->severity;
                break;
            }
            case DBR_STS_LONG:
            {
                const dbr_sts_long *data = static_cast<const dbr_sts_long *>(args.dbr);
                status = data->status;
                severity = data->severity;
                break;
            }
            case DBR_STS_FLOAT:
            {
                const dbr_sts_float *data = static_cast<const dbr_sts_float *>(args.dbr);
                status = data->status;
                severity = data->severity;
                break;
            }
            case DBR_STS_DOUBLE:
            {
                const dbr_sts_double *data = static_cast<const dbr_sts_double *>(args.dbr);
                status = data->status;
                severity = data->severity;
                break;
            }
            default:
                throw  std::runtime_error("DbdToPv::getDone logic error");
        }       
    }
    if(alarmRequested) {
         bool statusChanged = false;
         bool severityChanged = false;
         PVStructurePtr pvAlarm(pvStructure->getSubField<PVStructure>("alarm"));
         PVIntPtr pvSeverity(pvAlarm->getSubField<PVInt>("severity"));
         if(caAlarm.severity!=severity) {
             caAlarm.severity = severity;
             pvSeverity->put(severity);
             severityChanged = true;
         }
         PVStringPtr pvMessage(pvAlarm->getSubField<PVString>("message"));
         PVIntPtr pvStatus(pvAlarm->getSubField<PVInt>("status"));
         if(caAlarm.status!=status) {
             caAlarm.status = status;
             pvStatus->put(status);
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
        PVStructurePtr pvTimeStamp(pvStructure->getSubField<PVStructure>("timeStamp"));
        if(caTimeStamp.secPastEpoch!=stamp.secPastEpoch) {
            caTimeStamp.secPastEpoch = stamp.secPastEpoch;
            PVLongPtr pvSeconds(pvTimeStamp->getSubField<PVLong>("secondsPastEpoch"));
            pvSeconds->put(stamp.secPastEpoch+posixEpochAtEpicsEpoch);
            bitSet->set(pvSeconds->getFieldOffset());
        }
        if(caTimeStamp.nsec!=stamp.nsec) {
            caTimeStamp.secPastEpoch = stamp.secPastEpoch;
            PVIntPtr pvNano(pvTimeStamp->getSubField<PVInt>("nanoseconds"));
            pvNano->put(stamp.nsec);
            bitSet->set(pvNano->getFieldOffset());
        }
        
    }
    if(firstTime) {
        firstTime = false;
        bitSet->clear();
        bitSet->set(0);
    }
    return Status::Ok;
}

Status DbdToPv::putToDBD(
     CAChannelPtr const & caChannel,
     PVStructurePtr const & pvStructure,
     bool block)
{
    chid channelID = caChannel->getChannelID();
    const void *pValue = NULL;
    unsigned long count = 1;
    dbr_enum_t   indexvalue(0);
    dbr_char_t   bvalue(0);
    dbr_short_t  svalue(0);
    dbr_long_t   ivalue(0);
    dbr_float_t  fvalue(0);
    dbr_double_t dvalue(0);
    char *ca_stringBuffer(0);
    
    
    switch(caValueType) {
       case DBR_ENUM:
       {
            indexvalue = pvStructure->getSubField<PVInt>("value.index")->get();
            pValue = &indexvalue;
            break;
       }
       case DBR_STRING:
       {
           if(isArray) {
               PVStringArrayPtr pvValue = pvStructure->getSubField<PVStringArray>("value");
               count = pvValue->getLength();
               if(count<1) break;
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
           } else {
               pValue = pvStructure->getSubField<PVString>("value")->get().c_str();
           }
           break;
       }
       case DBR_CHAR:
        {
            if(isArray) {
                PVByteArrayPtr pvValue = pvStructure->getSubField<PVByteArray>("value");
                count = pvValue->getLength();
                pValue = pvValue->view().data();
            } else {
                bvalue = pvStructure->getSubField<PVByte>("value")->get();
                pValue = &bvalue;
            }
            break;
       }
       case DBR_SHORT:
       {
            if(isArray) {
                PVShortArrayPtr pvValue = pvStructure->getSubField<PVShortArray>("value");
                count = pvValue->getLength();
                pValue = pvValue->view().data();
            } else {
                svalue = pvStructure->getSubField<PVShort>("value")->get();
                pValue = &svalue;
            }
            break;
       }
       case DBR_LONG:
       {
            if(isArray) {
                PVIntArrayPtr pvValue = pvStructure->getSubField<PVIntArray>("value");
                count = pvValue->getLength();
                pValue = pvValue->view().data();
            } else {
                ivalue = pvStructure->getSubField<PVInt>("value")->get();
                pValue = &ivalue;
            }
            break;
       }
       case DBR_FLOAT:
       {
            if(isArray) {
                PVFloatArrayPtr pvValue = pvStructure->getSubField<PVFloatArray>("value");
                count = pvValue->getLength();
                pValue = pvValue->view().data();
            } else {
                fvalue = pvStructure->getSubField<PVFloat>("value")->get();
                pValue = &fvalue;
            }
            break;
       }
       case DBR_DOUBLE:
       {
            if(isArray) {
                PVDoubleArrayPtr pvValue = pvStructure->getSubField<PVDoubleArray>("value");
                count = pvValue->getLength();
                pValue = pvValue->view().data();
            } else {
                dvalue = pvStructure->getSubField<PVDouble>("value")->get();
                pValue = &dvalue;
            }
            break;
       }
       default:
            Status errorStatus(
                Status::STATUSTYPE_ERROR, string("DbdToPv::FromDBD logic error"));
            return errorStatus;
   }
   int result = 0;
   if(block) {
        caChannel->attachContext();
        result = ca_array_put_callback(caValueType,count,channelID,pValue,putHandler,this);
        if(result==ECA_NORMAL) {
             ca_flush_io();
             if(!waitForCallback.wait(2.0)) {
                  throw  std::runtime_error("DbDToPv::putToDBD waitForCallback timeout");
             }
             return putStatus;
        }    
   } else {
        caChannel->attachContext();
        result = ca_array_put(caValueType,count,channelID,pValue);
        ca_flush_io();
   }
   if(ca_stringBuffer!=NULL) delete[] ca_stringBuffer;
   if(result==ECA_NORMAL) return Status::Ok;
   Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(result)));
   return errorStatus;
}

void DbdToPv::putDone(struct event_handler_args &args)
{
    if(args.status!=ECA_NORMAL)
    {
        string message("DbdToPv::putDone ca_message ");
        message += ca_message(args.status);
        putStatus = Status(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
    } else {
        putStatus = Status::Ok;
    }
    waitForCallback.signal();
}
    

}}}
