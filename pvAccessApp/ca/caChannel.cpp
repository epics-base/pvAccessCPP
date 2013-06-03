/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/logger.h>
#include <pv/caChannel.h>
#include <pv/standardPVField.h>

using namespace epics::pvData;
using namespace epics::pvAccess;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

#define PVACCESS_REFCOUNT_MONITOR_DEFINE(name)
#define PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(name)
#define PVACCESS_REFCOUNT_MONITOR_DESTRUCT(name)

PVACCESS_REFCOUNT_MONITOR_DEFINE(caChannel);

CAChannel::shared_pointer CAChannel::create(ChannelProvider::shared_pointer const & channelProvider,
                                            epics::pvData::String const & channelName,
                                            short priority,
                                            ChannelRequester::shared_pointer const & channelRequester)
{
    CAChannel::shared_pointer thisPtr(new CAChannel(channelProvider, channelRequester));
    thisPtr->activate(channelName, priority);
    return thisPtr;
}

static void ca_connection_handler(struct connection_handler_args args)
{
    CAChannel *channel = static_cast<CAChannel*>(ca_puser(args.chid));

    if (args.op == CA_OP_CONN_UP)
        channel->connected();
    else if (args.op == CA_OP_CONN_DOWN)
        channel->disconnected();
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


static PVStructure::shared_pointer createPVStructure(CAChannel::shared_pointer const & channel, String const & properties)
{
    // TODO
    StandardPVFieldPtr standardPVField = getStandardPVField();
    PVStructure::shared_pointer pvStructure;

    chtype channelType = channel->getNativeType();
    if (channelType != DBR_ENUM)
    {
        ScalarType st = dbr2ST[channelType];
        pvStructure = (channel->getElementCount() > 1) ?
                       standardPVField->scalarArray(st, properties) :
                       standardPVField->scalar(st, properties);
    }
    else
    {
        // TODO handle enum array
        //     introduce ackConnected(pvStructure), if non-enum directly call, else when labels are retrieved
        StringArray labels;
        pvStructure = standardPVField->enumerated(labels, properties);
    }

    return pvStructure;
}


static PVStructure::shared_pointer createPVStructure(CAChannel::shared_pointer const & channel, chtype dbrType)
{
    // TODO constants
    // TODO value is always there
    String properties;
    if (dbrType >= DBR_CTRL_STRING)      // 28
    {
        if (dbrType != DBR_CTRL_STRING && dbrType != DBR_CTRL_ENUM)
            properties = "value,alarm,display,valueAlarm,control";
        else
            properties = "value,alarm";
    }
    else if (dbrType >= DBR_GR_STRING)   // 21
    {
        if (dbrType != DBR_GR_STRING && dbrType != DBR_GR_ENUM)
            properties = "value,alarm,display,valueAlarm";
        else
            properties = "value,alarm";
    }
    else if (dbrType >= DBR_TIME_STRING) // 14
        properties = "value,timeStamp";
    else if (dbrType >= DBR_STS_STRING)  // 7
        properties = "value,alarm";
    else
        properties = "value";

    return createPVStructure(channel, properties);
}


void CAChannel::connected()
{
    // TODO sync
    // we assume array if element count > 1
    elementCount = ca_element_count(channelID);
    channelType = ca_field_type(channelID);

    // no valueAlarm and control,display for non-numeric type
    String allProperties =
            (channelType != DBR_STRING && channelType != DBR_ENUM) ?
                "value,timeStamp,alarm,display,valueAlarm,control" :
                "value,timeStamp,alarm";
    PVStructure::shared_pointer pvStructure = createPVStructure(shared_from_this(), allProperties);

    // TODO thread sync
    // TODO we need only Structure here
    this->pvStructure = pvStructure;

    // TODO call channelCreated if structure has changed
    EXCEPTION_GUARD(channelRequester->channelStateChange(shared_from_this(), Channel::CONNECTED));
}

void CAChannel::disconnected()
{
    EXCEPTION_GUARD(channelRequester->channelStateChange(shared_from_this(), Channel::DISCONNECTED));
}

CAChannel::CAChannel(ChannelProvider::shared_pointer const & _channelProvider,
                     ChannelRequester::shared_pointer const & _channelRequester) :
    channelProvider(_channelProvider),
    channelRequester(_channelRequester),
    channelID(0),
    channelType(0),
    elementCount(0)
{
    PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(caChannel);
}

void CAChannel::activate(epics::pvData::String const & channelName, short priority)
{
    int result = ca_create_channel(channelName.c_str(),
                                   ca_connection_handler,
                                   this,
                                   priority, // TODO mapping
                                   &channelID);
    if (result == ECA_NORMAL)
    {
        // TODO be sure that ca_connection_handler is not called before this call
        EXCEPTION_GUARD(channelRequester->channelCreated(Status::Ok, shared_from_this()));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(result)));
        EXCEPTION_GUARD(channelRequester->channelCreated(errorStatus, shared_from_this()));
    }
}

CAChannel::~CAChannel()
{
    PVACCESS_REFCOUNT_MONITOR_DESTRUCT(caChannel);
}


chid CAChannel::getChannelID()
{
    return channelID;
}


chtype CAChannel::getNativeType()
{
    return channelType;
}


unsigned CAChannel::getElementCount()
{
    return elementCount;
}

PVStructure::shared_pointer CAChannel::getPVStructure()
{
    return pvStructure;
}


std::tr1::shared_ptr<ChannelProvider> CAChannel::getProvider()
{
    return channelProvider;
}


epics::pvData::String CAChannel::getRemoteAddress()
{
    return epics::pvData::String(ca_host_name(channelID));
}


static Channel::ConnectionState cs2CS[] =
{
    Channel::NEVER_CONNECTED,    // cs_never_conn
    Channel::DISCONNECTED,       // cs_prev_conn
    Channel::CONNECTED,          // cs_conn
    Channel::DESTROYED           // cs_closed
};

Channel::ConnectionState CAChannel::getConnectionState()
{
    return cs2CS[ca_state(channelID)];
}


epics::pvData::String CAChannel::getChannelName()
{
    return epics::pvData::String(ca_name(channelID));
}


std::tr1::shared_ptr<ChannelRequester> CAChannel::getChannelRequester()
{
    return channelRequester;
}


bool CAChannel::isConnected()
{
    return (ca_state(channelID) == cs_conn);
}


void CAChannel::getField(GetFieldRequester::shared_pointer const & requester,
                         epics::pvData::String const & subField)
{
    PVField::shared_pointer pvField =
            subField.empty() ?
                std::tr1::static_pointer_cast<PVField>(pvStructure) :
                pvStructure->getSubField(subField);

    if (pvField)
    {
        EXCEPTION_GUARD(requester->getDone(Status::Ok, pvField->getField()));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, "field '" + subField + "' not found");
        EXCEPTION_GUARD(requester->getDone(errorStatus, FieldConstPtr()));
    }
}


AccessRights CAChannel::getAccessRights(epics::pvData::PVField::shared_pointer const & /*pvField*/)
{
    if (ca_write_access(channelID))
        return readWrite;
    else if (ca_read_access(channelID))
        return read;
    else
        return none;
}


ChannelProcess::shared_pointer CAChannel::createChannelProcess(
        ChannelProcessRequester::shared_pointer const & channelProcessRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not supported");
    ChannelProcess::shared_pointer nullChannelProcess;
    EXCEPTION_GUARD(channelProcessRequester->channelProcessConnect(errorStatus, nullChannelProcess));
    return nullChannelProcess;
}

ChannelGet::shared_pointer CAChannel::createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelGet::create(shared_from_this(), channelGetRequester, pvRequest);
}


ChannelPut::shared_pointer CAChannel::createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelPut::create(shared_from_this(), channelPutRequester, pvRequest);
}

ChannelPutGet::shared_pointer CAChannel::createChannelPutGet(
        ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not supported");
    ChannelPutGet::shared_pointer nullChannelPutGet;
    EXCEPTION_GUARD(channelPutGetRequester->channelPutGetConnect(errorStatus, nullChannelPutGet,
                                                                 PVStructure::shared_pointer(), PVStructure::shared_pointer()));
    return nullChannelPutGet;
}


ChannelRPC::shared_pointer CAChannel::createChannelRPC(
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not supported");
    ChannelRPC::shared_pointer nullChannelRPC;
    EXCEPTION_GUARD(channelRPCRequester->channelRPCConnect(errorStatus, nullChannelRPC));
    return nullChannelRPC;
}


epics::pvData::Monitor::shared_pointer CAChannel::createMonitor(
        epics::pvData::MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    Monitor::shared_pointer nullMonitor;
    EXCEPTION_GUARD(monitorRequester->monitorConnect(errorStatus, nullMonitor,
                                                     Structure::shared_pointer()));
    return nullMonitor;
}


ChannelArray::shared_pointer CAChannel::createChannelArray(
        ChannelArrayRequester::shared_pointer const & channelArrayRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not supported");
    ChannelArray::shared_pointer nullChannelArray;
    EXCEPTION_GUARD(channelArrayRequester->channelArrayConnect(errorStatus, nullChannelArray,
                                                               PVArray::shared_pointer()));
    return nullChannelArray;
}


void CAChannel::printInfo()
{
    String info;
    printInfo(&info);
    std::cout << info.c_str() << std::endl;
}


void CAChannel::printInfo(epics::pvData::StringBuilder out)
{
    out->append(  "CHANNEL  : "); out->append(getChannelName());
    ConnectionState state = getConnectionState();
    out->append("\nSTATE    : "); out->append(ConnectionStateNames[state]);
    if (state == CONNECTED)
    {
        out->append("\nADDRESS  : "); out->append(getRemoteAddress());
        //out->append("\nRIGHTS   : "); out->append(getAccessRights());
    }
    out->append("\n");
}


/* --------------- epics::pvData::Requester --------------- */


String CAChannel::getRequesterName()
{
    return getChannelName();
}


void CAChannel::message(String const & message,MessageType messageType)
{
    std::cout << "[" << getRequesterName() << "] message(" << message << ", " << getMessageTypeName(messageType) << ")" << std::endl;
}


/* --------------- epics::pvData::Destroyable --------------- */


void CAChannel::destroy()
{
    // TODO
}

















ChannelGet::shared_pointer CAChannelGet::create(
        CAChannel::shared_pointer const & channel,
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelGet::shared_pointer thisPtr(new CAChannelGet(channel, channelGetRequester, pvRequest));
    static_cast<CAChannelGet*>(thisPtr.get())->activate();
    return thisPtr;
}


CAChannelGet::~CAChannelGet()
{
    // TODO
}


static chtype getDBRType(PVStructure::shared_pointer const & pvRequest, chtype nativeType)
{
    // get "field" sub-structure
    PVStructure::shared_pointer fieldSubField =
            std::tr1::dynamic_pointer_cast<PVStructure>(pvRequest->getSubField("field"));
    if (!fieldSubField)
        fieldSubField = pvRequest;
    Structure::const_shared_pointer fieldStructure = fieldSubField->getStructure();

    // no fields or control -> DBR_CTRL_<type>
    if (fieldStructure->getNumberFields() == 0 ||
          fieldStructure->getField("control"))
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_CTRL_STRING);

    // display/valueAlarm -> DBR_GR_<type>
    if (fieldStructure->getField("display") || fieldStructure->getField("valueAlarm"))
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_GR_STRING);

    // alarm -> DBR_STS_<type>
    if (fieldStructure->getField("alarm"))
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_STS_STRING);

    // timeStamp -> DBR_TIME_<type>
    // NOTE: that only DBR_TIME_<type> type holds timestamp, therefore if you request for
    // the fields above, you will never get timestamp
    if (fieldStructure->getField("timeStamp"))
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_TIME_STRING);

    return nativeType;
}


CAChannelGet::CAChannelGet(CAChannel::shared_pointer const & _channel,
                           ChannelGetRequester::shared_pointer const & _channelGetRequester,
                           epics::pvData::PVStructure::shared_pointer const & pvRequest) :
    channel(_channel),
    channelGetRequester(_channelGetRequester),
    getType(getDBRType(pvRequest, _channel->getNativeType())),
    pvStructure(createPVStructure(_channel, getType)),
    bitSet(new BitSet(pvStructure->getStructure()->getNumberFields()))
{
    // TODO
    bitSet->set(0);
}

void CAChannelGet::activate()
{
    EXCEPTION_GUARD(channelGetRequester->channelGetConnect(Status::Ok, shared_from_this(),
                                                           pvStructure, bitSet));
}


/* --------------- epics::pvAccess::ChannelGet --------------- */


static void ca_get_handler(struct event_handler_args args)
{
    CAChannelGet *channelGet = static_cast<CAChannelGet*>(args.usr);
    channelGet->getDone(args);
}

typedef void (*copyDBRtoPVStructure)(const void * from, unsigned count, PVStructure::shared_pointer const & to);


// template<primitive type, ScalarType, scalar Field, array Field>
template<typename pT, epics::pvData::ScalarType sT, typename sF, typename aF>
void copy_DBR(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<sF> value = std::tr1::static_pointer_cast<sF>(pvStructure->getSubField("value"));
        value->put(static_cast<const pT*>(dbr)[0]);
    }
    else
    {
        std::tr1::shared_ptr<aF> value =
                std::tr1::static_pointer_cast<aF>(pvStructure->getScalarArrayField("value", sT));
        value->put(0, count, static_cast<const pT*>(dbr), 0);
    }
}

#ifdef vxWorks
// dbr_long_t is defined as "int", pvData uses int32 which can be defined as "long int" (32-bit)
// template<primitive type, ScalarType, scalar Field, array Field>
template<>
void copy_DBR<dbr_long_t, pvInt, PVInt, PVIntArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVInt> value = std::tr1::static_pointer_cast<PVInt>(pvStructure->getSubField("value"));
        value->put(static_cast<const int32*>(dbr)[0]);
    }
    else
    {
        std::tr1::shared_ptr<PVIntArray> value =
                std::tr1::static_pointer_cast<PVIntArray>(pvStructure->getScalarArrayField("value", pvInt));
        value->put(0, count, static_cast<const int32*>(dbr), 0);
    }
}
#endif

// string specialization
template<>
void copy_DBR<String, pvString, PVString, PVStringArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVString> value = std::tr1::static_pointer_cast<PVString>(pvStructure->getSubField("value"));
        value->put(String(static_cast<const char*>(dbr)));
    }
    else
    {
        std::tr1::shared_ptr<PVStringArray> value =
                std::tr1::static_pointer_cast<PVStringArray>(pvStructure->getScalarArrayField("value", pvString));
        const dbr_string_t* dbrStrings = static_cast<const dbr_string_t*>(dbr);
        StringArray sA;
        sA.reserve(count);
        for (unsigned i = 0; i < count; i++)
            sA.push_back(dbrStrings[i]);
        value->put(0, count, sA, 0);
    }
}

// enum specialization
template<>
void copy_DBR<dbr_enum_t, pvString, PVString, PVStringArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVInt> value = std::tr1::static_pointer_cast<PVInt>(pvStructure->getSubField("value.index"));
        value->put(static_cast<const dbr_enum_t*>(dbr)[0]);
    }
    else
    {
        // TODO
        /*
        std::tr1::shared_ptr<PVStringArray> value =
                std::tr1::static_pointer_cast<PVStringArray>(pvStructure->getScalarArrayField("value", pvString));
        const dbr_string_t* dbrStrings = static_cast<const dbr_string_t*>(dbr);
        StringArray sA;
        sA.reserve(count);
        for (unsigned i = 0; i < count; i++)
            sA.push_back(dbrStrings[i]);
        value->put(0, count, sA, 0);
        */
    }
}

static String dbrStatus2alarmMessage[] = {
    "NO_ALARM",     // 0 ..
    "READ_ALARM",
    "WRITE_ALARM",
    "HIHI_ALARM",
    "HIGH_ALARM",
    "LOLO_ALARM",
    "LOW_ALARM",
    "STATE_ALARM",
    "COS_ALARM",
    "COMM_ALARM",
    "TIMEOUT_ALARM",
    "HW_LIMIT_ALARM",
    "CALC_ALARM",
    "SCAN_ALARM",
    "LINK_ALARM",
    "SOFT_ALARM",
    "BAD_SUB_ALARM",
    "UDF_ALARM",
    "DISABLE_ALARM",
    "SIMM_ALARM",
    "READ_ACCESS_ALARM",
    "WRITE_ACCESS_ALARM"        // .. 21
};

// template<DBR type, primitive type, ScalarType, scalar Field, array Field>
template<typename T, typename pT, epics::pvData::ScalarType sT, typename sF, typename aF>
void copy_DBR_STS(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer alarm = pvStructure->getStructureField("alarm");
    // no mapping needed
    alarm->getIntField("status")->put(0);
    alarm->getIntField("severity")->put(data->severity);
    alarm->getStringField("message")->put(dbrStatus2alarmMessage[data->status]);

    copy_DBR<pT, sT, sF, aF>(&data->value, count, pvStructure);
}

// template<DBR type, primitive type, ScalarType, scalar Field, array Field>
template<typename T, typename pT, epics::pvData::ScalarType sT, typename sF, typename aF>
void copy_DBR_TIME(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer ts = pvStructure->getStructureField("timeStamp");
    epics::pvData::int64 spe = data->stamp.secPastEpoch;
    spe += 7305*86400;
    ts->getLongField("secondsPastEpoch")->put(spe);
    ts->getIntField("nanoSeconds")->put(data->stamp.nsec);

    copy_DBR<pT, sT, sF, aF>(&data->value, count, pvStructure);
}


template <typename T>
void copy_format(const void * /*dbr*/, PVStructure::shared_pointer const & pvDisplayStructure)
{
    pvDisplayStructure->getStringField("format")->put("%d");
}

template <>
void copy_format<dbr_time_string>(const void * /*dbr*/, PVStructure::shared_pointer const & pvDisplayStructure)
{
    pvDisplayStructure->getStringField("format")->put("%s");
}

#define COPY_FORMAT_FOR(T) \
template <> \
void copy_format<T>(const void * dbr, PVStructure::shared_pointer const & pvDisplayStructure) \
{ \
    const T* data = static_cast<const T*>(dbr); \
\
    if (data->precision) \
    { \
        char fmt[16]; \
        sprintf(fmt, "%%.%df", data->precision); \
        pvDisplayStructure->getStringField("format")->put(String(fmt)); \
    } \
    else \
    { \
        pvDisplayStructure->getStringField("format")->put("%f"); \
    } \
}

COPY_FORMAT_FOR(dbr_gr_float)
COPY_FORMAT_FOR(dbr_ctrl_float)
COPY_FORMAT_FOR(dbr_gr_double)
COPY_FORMAT_FOR(dbr_ctrl_double)

#undef COPY_FORMAT_FOR

// template<DBR type, primitive type, ScalarType, scalar Field, array Field>
template<typename T, typename pT, epics::pvData::ScalarType sT, typename sF, typename aF>
void copy_DBR_GR(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer alarm = pvStructure->getStructureField("alarm");
    alarm->getIntField("status")->put(0);
    alarm->getIntField("severity")->put(data->severity);
    alarm->getStringField("message")->put(dbrStatus2alarmMessage[data->status]);

    PVStructure::shared_pointer disp = pvStructure->getStructureField("display");
    disp->getStringField("units")->put(String(data->units));
    disp->getDoubleField("limitHigh")->put(data->upper_disp_limit);
    disp->getDoubleField("limitLow")->put(data->lower_disp_limit);

    copy_format<T>(dbr, disp);

    PVStructure::shared_pointer va = pvStructure->getStructureField("valueAlarm");
    va->getDoubleField("highAlarmLimit")->put(data->upper_alarm_limit);
    va->getDoubleField("highWarningLimit")->put(data->upper_warning_limit);
    va->getDoubleField("lowWarningLimit")->put(data->lower_warning_limit);
    va->getDoubleField("lowAlarmLimit")->put(data->lower_alarm_limit);

    copy_DBR<pT, sT, sF, aF>(&data->value, count, pvStructure);
}

// template<DBR type, primitive type, ScalarType, scalar Field, array Field>
template<typename T, typename pT, epics::pvData::ScalarType sT, typename sF, typename aF>
void copy_DBR_CTRL(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer alarm = pvStructure->getStructureField("alarm");
    alarm->getIntField("status")->put(0);
    alarm->getIntField("severity")->put(data->severity);
    alarm->getStringField("message")->put(dbrStatus2alarmMessage[data->status]);

    PVStructure::shared_pointer disp = pvStructure->getStructureField("display");
    disp->getStringField("units")->put(String(data->units));
    disp->getDoubleField("limitHigh")->put(data->upper_disp_limit);
    disp->getDoubleField("limitLow")->put(data->lower_disp_limit);

    copy_format<T>(dbr, disp);

    PVStructure::shared_pointer va = pvStructure->getStructureField("valueAlarm");
    va->getDoubleField("highAlarmLimit")->put(data->upper_alarm_limit);
    va->getDoubleField("highWarningLimit")->put(data->upper_warning_limit);
    va->getDoubleField("lowWarningLimit")->put(data->lower_warning_limit);
    va->getDoubleField("lowAlarmLimit")->put(data->lower_alarm_limit);

    PVStructure::shared_pointer ctrl = pvStructure->getStructureField("control");
    ctrl->getDoubleField("limitHigh")->put(data->upper_ctrl_limit);
    ctrl->getDoubleField("limitLow")->put(data->lower_ctrl_limit);

    copy_DBR<pT, sT, sF, aF>(&data->value, count, pvStructure);
}
static copyDBRtoPVStructure copyFuncTable[] =
{
    copy_DBR<String, pvString, PVString, PVStringArray>,          // DBR_STRING
    copy_DBR<dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_INT, DBR_SHORT
    copy_DBR<dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_FLOAT
    copy_DBR<dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_ENUM
    copy_DBR<int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_CHAR
    copy_DBR<dbr_long_t, pvInt, PVInt, PVIntArray>,          // DBR_LONG
    copy_DBR<dbr_double_t, pvDouble, PVDouble, PVDoubleArray>,          // DBR_DOUBLE

    copy_DBR_STS<dbr_sts_string, String, pvString, PVString, PVStringArray>,          // DBR_STS_STRING
    copy_DBR_STS<dbr_sts_short, dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_STS_INT, DBR_STS_SHORT
    copy_DBR_STS<dbr_sts_float, dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_STS_FLOAT
    copy_DBR_STS<dbr_sts_enum, dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_STS_ENUM
    copy_DBR_STS<dbr_sts_char, int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_STS_CHAR
    copy_DBR_STS<dbr_sts_long, dbr_long_t, pvInt, PVInt, PVIntArray>,          // DBR_STS_LONG
    copy_DBR_STS<dbr_sts_double, dbr_double_t, pvDouble, PVDouble, PVDoubleArray>,          // DBR_STS_DOUBLE

    copy_DBR_TIME<dbr_time_string, String, pvString, PVString, PVStringArray>,          // DBR_TIME_STRING
    copy_DBR_TIME<dbr_time_short, dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_TIME_INT, DBR_TIME_SHORT
    copy_DBR_TIME<dbr_time_float, dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_TIME_FLOAT
    copy_DBR_TIME<dbr_time_enum, dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_TIME_ENUM
    copy_DBR_TIME<dbr_time_char, int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_TIME_CHAR
    copy_DBR_TIME<dbr_time_long, dbr_long_t, pvInt, PVInt, PVIntArray>,          // DBR_TIME_LONG
    copy_DBR_TIME<dbr_time_double, dbr_double_t, pvDouble, PVDouble, PVDoubleArray>,          // DBR_TIME_DOUBLE

    copy_DBR_STS<dbr_sts_string, String, pvString, PVString, PVStringArray>,          // DBR_GR_STRING -> DBR_STS_STRING
    copy_DBR_GR<dbr_gr_short, dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_GR_INT, DBR_GR_SHORT
    copy_DBR_GR<dbr_gr_float, dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_GR_FLOAT
    copy_DBR_STS<dbr_sts_enum, dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_GR_ENUM -> DBR_STS_ENUM
    copy_DBR_GR<dbr_gr_char, int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_GR_CHAR
    copy_DBR_GR<dbr_gr_long, dbr_long_t, pvInt, PVInt, PVIntArray>,          // DBR_GR_LONG
    copy_DBR_GR<dbr_gr_double, dbr_double_t, pvDouble, PVDouble, PVDoubleArray>,          // DBR_GR_DOUBLE

    copy_DBR_STS<dbr_sts_string, String, pvString, PVString, PVStringArray>,          // DBR_CTRL_STRING -> DBR_STS_STRING
    copy_DBR_CTRL<dbr_ctrl_short, dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_CTRL_INT, DBR_CTRL_SHORT
    copy_DBR_CTRL<dbr_ctrl_float, dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_CTRL_FLOAT
    copy_DBR_STS<dbr_sts_enum, dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_CTRL_ENUM -> DBR_STS_ENUM
    copy_DBR_CTRL<dbr_ctrl_char, int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_CTRL_CHAR
    copy_DBR_CTRL<dbr_ctrl_long, dbr_long_t, pvInt, PVInt, PVIntArray>,          // DBR_CTRL_LONG
    copy_DBR_CTRL<dbr_ctrl_double, dbr_double_t, pvDouble, PVDouble, PVDoubleArray>          // DBR_CTRL_DOUBLE
};

void CAChannelGet::getDone(struct event_handler_args &args)
{
    if (args.status == ECA_NORMAL)
    {
        copyDBRtoPVStructure copyFunc = copyFuncTable[getType];
        if (copyFunc)
            copyFunc(args.dbr, args.count, pvStructure);
        else
        {
            // TODO remove
            std::cout << "no copy func implemented" << std::endl;
        }

        EXCEPTION_GUARD(channelGetRequester->getDone(Status::Ok));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(args.status)));
        EXCEPTION_GUARD(channelGetRequester->getDone(errorStatus));
    }
}


void CAChannelGet::get(bool lastRequest)
{
    int result = ca_array_get_callback(getType, channel->getElementCount(),
                                       channel->getChannelID(), ca_get_handler, this);
    if (result == ECA_NORMAL)
    {
        ca_flush_io();
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(result)));
        EXCEPTION_GUARD(channelGetRequester->getDone(errorStatus));
    }

    if (lastRequest)
        destroy();
}


/* --------------- epics::pvData::Destroyable --------------- */


void CAChannelGet::destroy()
{
    // TODO
}


/* --------------- epics::pvData::Lockable --------------- */


void CAChannelGet::lock()
{
    // TODO
}


void CAChannelGet::unlock()
{
    // TODO
}












ChannelPut::shared_pointer CAChannelPut::create(
        CAChannel::shared_pointer const & channel,
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelPut::shared_pointer thisPtr(new CAChannelPut(channel, channelPutRequester, pvRequest));
    static_cast<CAChannelPut*>(thisPtr.get())->activate();
    return thisPtr;
}


CAChannelPut::~CAChannelPut()
{
    // TODO
}


CAChannelPut::CAChannelPut(CAChannel::shared_pointer const & _channel,
                           ChannelPutRequester::shared_pointer const & _channelPutRequester,
                           epics::pvData::PVStructure::shared_pointer const & pvRequest) :
    channel(_channel),
    channelPutRequester(_channelPutRequester),
    getType(getDBRType(pvRequest, _channel->getNativeType())),
    pvStructure(createPVStructure(_channel, getType)),
    bitSet(new BitSet(pvStructure->getStructure()->getNumberFields()))
{
    // NOTE: we require value type, we can only put value field
    bitSet->set(pvStructure->getSubField("value")->getFieldOffset());
}

void CAChannelPut::activate()
{
    EXCEPTION_GUARD(channelPutRequester->channelPutConnect(Status::Ok, shared_from_this(),
                                                           pvStructure, bitSet));
}


/* --------------- epics::pvAccess::ChannelPut --------------- */


static void ca_put_handler(struct event_handler_args args)
{
    CAChannelPut *channelPut = static_cast<CAChannelPut*>(args.usr);
    channelPut->putDone(args);
}


static void ca_put_get_handler(struct event_handler_args args)
{
    CAChannelPut *channelPut = static_cast<CAChannelPut*>(args.usr);
    channelPut->getDone(args);
}

typedef int (*doPut)(CAChannel::shared_pointer const & channel, void *usrArg, PVStructure::shared_pointer const & from);


// template<primitive type, ScalarType, scalar Field, array Field>
template<typename pT, epics::pvData::ScalarType sT, typename sF, typename aF>
int doPut_pvStructure(CAChannel::shared_pointer const & channel, void *usrArg, PVStructure::shared_pointer const & pvStructure)
{
    bool isScalarValue = pvStructure->getStructure()->getField("value")->getType() == scalar;

    if (isScalarValue)
    {
        std::tr1::shared_ptr<sF> value = std::tr1::static_pointer_cast<sF>(pvStructure->getSubField("value"));

        pT val = value->get();
        int result = ca_array_put_callback(channel->getNativeType(), 1,
                                           channel->getChannelID(), &val,
                                           ca_put_handler, usrArg);

        if (result == ECA_NORMAL)
        {
            ca_flush_io();
        }

        return result;
    }
    else
    {
        std::tr1::shared_ptr<aF> value =
                std::tr1::static_pointer_cast<aF>(pvStructure->getScalarArrayField("value", sT));

        const pT* val = value->get();
        int result = ca_array_put_callback(channel->getNativeType(), value->getLength(),
                                           channel->getChannelID(), val,
                                           ca_put_handler, usrArg);

        if (result == ECA_NORMAL)
        {
            ca_flush_io();
        }

        return result;
    }
}

static doPut doPutFuncTable[] =
{
    0, //doPut_pvStructure<String, pvString, PVString, PVStringArray>,          // DBR_STRING
    doPut_pvStructure<dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_INT, DBR_SHORT
    doPut_pvStructure<dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_FLOAT
    0, //doPut_pvStructure<dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_ENUM
    doPut_pvStructure<int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_CHAR
    #ifdef vxWorks
    doPut_pvStructure<int32, pvInt, PVInt, PVIntArray>,          // DBR_LONG
    #else
    doPut_pvStructure<dbr_long_t, pvInt, PVInt, PVIntArray>,          // DBR_LONG
    #endif
    doPut_pvStructure<dbr_double_t, pvDouble, PVDouble, PVDoubleArray>,          // DBR_DOUBLE
};

void CAChannelPut::putDone(struct event_handler_args &args)
{
    if (args.status == ECA_NORMAL)
    {
        EXCEPTION_GUARD(channelPutRequester->putDone(Status::Ok));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(args.status)));
        EXCEPTION_GUARD(channelPutRequester->putDone(errorStatus));
    }
}


void CAChannelPut::put(bool lastRequest)
{
    doPut putFunc = doPutFuncTable[channel->getNativeType()];
    if (putFunc)
    {
        int result = putFunc(channel, this, pvStructure);
        if (result != ECA_NORMAL)
        {
            Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(result)));
            EXCEPTION_GUARD(channelPutRequester->getDone(errorStatus));
        }
    }
    else
    {
        // TODO remove
        std::cout << "no put func implemented" << std::endl;
    }

    // TODO here???!!!
    if (lastRequest)
        destroy();
}


void CAChannelPut::getDone(struct event_handler_args &args)
{
    if (args.status == ECA_NORMAL)
    {
        copyDBRtoPVStructure copyFunc = copyFuncTable[getType];
        if (copyFunc)
            copyFunc(args.dbr, args.count, pvStructure);
        else
        {
            // TODO remove
            std::cout << "no copy func implemented" << std::endl;
        }

        EXCEPTION_GUARD(channelPutRequester->getDone(Status::Ok));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(args.status)));
        EXCEPTION_GUARD(channelPutRequester->getDone(errorStatus));
    }
}


void CAChannelPut::get()
{
    int result = ca_array_get_callback(getType, channel->getElementCount(),
                                       channel->getChannelID(), ca_put_get_handler, this);
    if (result == ECA_NORMAL)
    {
        ca_flush_io();
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, String(ca_message(result)));
        EXCEPTION_GUARD(channelPutRequester->getDone(errorStatus));
    }
}


/* --------------- epics::pvData::Destroyable --------------- */


void CAChannelPut::destroy()
{
    // TODO
}


/* --------------- epics::pvData::Lockable --------------- */


void CAChannelPut::lock()
{
    // TODO
}


void CAChannelPut::unlock()
{
    // TODO
}
