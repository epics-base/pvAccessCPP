/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsVersion.h>

#include <pv/logger.h>
#include <pv/standardField.h>

#include <pv/caChannel.h>
#include <pv/caStatus.h>

using namespace epics::pvData;
using namespace epics::pvAccess;
using namespace epics::pvAccess::ca;

using std::string;

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }

#define PVACCESS_REFCOUNT_MONITOR_DEFINE(name)
#define PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(name)
#define PVACCESS_REFCOUNT_MONITOR_DESTRUCT(name)

PVACCESS_REFCOUNT_MONITOR_DEFINE(caChannel);

CAChannel::shared_pointer CAChannel::create(CAChannelProvider::shared_pointer const & channelProvider,
        std::string const & channelName,
        short priority,
        ChannelRequester::shared_pointer const & channelRequester)
{
    CAChannel::shared_pointer thisPtr(new CAChannel(channelName, channelProvider, channelRequester));
    thisPtr->activate(priority);
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

static Structure::const_shared_pointer createStructure(CAChannel::shared_pointer const & channel, string const & properties)
{
    StandardFieldPtr standardField = getStandardField();
    Structure::const_shared_pointer structure;

    chtype channelType = channel->getNativeType();
    if (channelType != DBR_ENUM)
    {
        ScalarType st = dbr2ST[channelType];
        structure = (channel->getElementCount() > 1) ?
                    standardField->scalarArray(st, properties) :
                    standardField->scalar(st, properties);
    }
    else
    {
        // NOTE: enum arrays not supported
        structure = standardField->enumerated(properties);
    }

    return structure;
}

static void ca_get_labels_handler(struct event_handler_args args)
{

    if (args.status == ECA_NORMAL)
    {
        const dbr_gr_enum* dbr_enum_p = static_cast<const dbr_gr_enum*>(args.dbr);

        PVStringArray* labelsArray = static_cast<PVStringArray*>(args.usr);
        PVStringArray::svector labels(labelsArray->reuse());
        labels.resize(dbr_enum_p->no_str);
        std::copy(dbr_enum_p->strs, dbr_enum_p->strs + dbr_enum_p->no_str, labels.begin());
        labelsArray->replace(freeze(labels));
    }
    else
    {
        // TODO better error handling
        std::cerr << "failed to get labels for enum : " << ca_message(args.status) << std::endl;
    }
}

static PVStructure::shared_pointer createPVStructure(CAChannel::shared_pointer const & channel, string const & properties)
{
    PVStructure::shared_pointer pvStructure = getPVDataCreate()->createPVStructure(createStructure(channel, properties));
    if (channel->getNativeType() == DBR_ENUM)
    {
        PVScalarArrayPtr pvScalarArray = pvStructure->getSubField<PVStringArray>("value.choices");

        // TODO avoid getting labels if DBR_GR_ENUM or DBR_CTRL_ENUM is used in subsequent get
        int result = ca_array_get_callback(DBR_GR_ENUM, 1, channel->getChannelID(), ca_get_labels_handler, pvScalarArray.get());
        if (result == ECA_NORMAL)
        {
            ca_flush_io();

            // NOTE: we do not wait here, since all subsequent request (over TCP) is serialized
            // and will guarantee that ca_get_labels_handler is called first
        }
        else
        {
            // TODO better error handling
            std::cerr << "failed to get labels for enum " << channel->getChannelName() << ": " << ca_message(result) << std::endl;
        }
    }

    return pvStructure;
}

static PVStructure::shared_pointer createPVStructure(CAChannel::shared_pointer const & channel, chtype dbrType)
{
    // NOTE: value is always there
    string properties;
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
        properties = "value,alarm,timeStamp";
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
    string allProperties =
        (channelType != DBR_STRING && channelType != DBR_ENUM) ?
        "value,timeStamp,alarm,display,valueAlarm,control" :
        "value,timeStamp,alarm";
    Structure::const_shared_pointer structure = createStructure(shared_from_this(), allProperties);

    // TODO thread sync
    // TODO we need only Structure here
    this->structure = structure;

    // TODO call channelCreated if structure has changed
    EXCEPTION_GUARD(channelRequester->channelStateChange(shared_from_this(), Channel::CONNECTED));
}

void CAChannel::disconnected()
{
    EXCEPTION_GUARD(channelRequester->channelStateChange(shared_from_this(), Channel::DISCONNECTED));
}

CAChannel::CAChannel(std::string const & _channelName,
                     CAChannelProvider::shared_pointer const & _channelProvider,
                     ChannelRequester::shared_pointer const & _channelRequester) :
    channelName(_channelName),
    channelProvider(_channelProvider),
    channelRequester(_channelRequester),
    channelID(0),
    channelType(0),
    elementCount(0)
{
    PVACCESS_REFCOUNT_MONITOR_CONSTRUCT(caChannel);
}

void CAChannel::activate(short priority)
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
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(result)));
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


std::tr1::shared_ptr<ChannelProvider> CAChannel::getProvider()
{
    return channelProvider;
}


std::string CAChannel::getRemoteAddress()
{
    return std::string(ca_host_name(channelID));
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


std::string CAChannel::getChannelName()
{
    return channelName;
}


std::tr1::shared_ptr<ChannelRequester> CAChannel::getChannelRequester()
{
    return channelRequester;
}

void CAChannel::getField(GetFieldRequester::shared_pointer const & requester,
                         std::string const & subField)
{
    Field::const_shared_pointer field =
        subField.empty() ?
        std::tr1::static_pointer_cast<const Field>(structure) :
        structure->getField(subField);

    if (field)
    {
        EXCEPTION_GUARD(requester->getDone(Status::Ok, field));
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
                    Structure::const_shared_pointer(), Structure::const_shared_pointer()));
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


Monitor::shared_pointer CAChannel::createMonitor(
    MonitorRequester::shared_pointer const & monitorRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelMonitor::create(shared_from_this(), monitorRequester, pvRequest);
}


ChannelArray::shared_pointer CAChannel::createChannelArray(
    ChannelArrayRequester::shared_pointer const & channelArrayRequester,
    epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not supported");
    ChannelArray::shared_pointer nullChannelArray;
    EXCEPTION_GUARD(channelArrayRequester->channelArrayConnect(errorStatus, nullChannelArray,
                    Array::const_shared_pointer()));
    return nullChannelArray;
}


void CAChannel::printInfo(std::ostream& out)
{
    out << "CHANNEL  : " << getChannelName() << std::endl;

    ConnectionState state = getConnectionState();
    out << "STATE    : " << ConnectionStateNames[state] << std::endl;
    if (state == CONNECTED)
    {
        out << "ADDRESS  : " << getRemoteAddress() << std::endl;
        //out << "RIGHTS   : " << getAccessRights() << std::endl;
    }
}


/* --------------- epics::pvData::Destroyable --------------- */


void CAChannel::destroy()
{
    threadAttach();

    Lock lock(requestsMutex);
    {
        while (!requests.empty())
        {
            ChannelRequest::shared_pointer request = requests.rbegin()->second.lock();
            if (request)
                request->destroy();
        }
    }

    /* Clear CA Channel */
    ca_clear_channel(channelID);
}

/* ---------------------------------------------------------- */

void CAChannel::threadAttach()
{
    std::tr1::static_pointer_cast<CAChannelProvider>(channelProvider)->threadAttach();
}

void CAChannel::registerRequest(ChannelRequest::shared_pointer const & request)
{
    Lock lock(requestsMutex);
    requests[request.get()] = ChannelRequest::weak_pointer(request);
}

void CAChannel::unregisterRequest(ChannelRequest::shared_pointer const & request)
{
    Lock lock(requestsMutex);
    requests.erase(request.get());
}














ChannelGet::shared_pointer CAChannelGet::create(
    CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    // TODO use std::make_shared
    std::tr1::shared_ptr<CAChannelGet> tp(
        new CAChannelGet(channel, channelGetRequester, pvRequest)
    );
    ChannelGet::shared_pointer thisPtr = tp;
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

    // timeStamp -> DBR_TIME_<type>
    // NOTE: that only DBR_TIME_<type> type holds timestamp, therefore if you request for
    // the fields above, you will never get timestamp
    if (fieldStructure->getField("timeStamp"))
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_TIME_STRING);

    // alarm -> DBR_STS_<type>
    if (fieldStructure->getField("alarm"))
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_STS_STRING);

    return nativeType;
}


CAChannelGet::CAChannelGet(CAChannel::shared_pointer const & _channel,
                           ChannelGetRequester::shared_pointer const & _channelGetRequester,
                           epics::pvData::PVStructure::shared_pointer const & pvRequest) :
    channel(_channel),
    channelGetRequester(_channelGetRequester),
    getType(getDBRType(pvRequest, _channel->getNativeType())),
    pvStructure(createPVStructure(_channel, getType)),
    bitSet(new BitSet(static_cast<uint32>(pvStructure->getStructure()->getNumberFields()))),
    lastRequestFlag(false)
{
    // TODO
    bitSet->set(0);
}

void CAChannelGet::activate()
{
    EXCEPTION_GUARD(channelGetRequester->channelGetConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}


/* --------------- epics::pvAccess::ChannelGet --------------- */


static void ca_get_handler(struct event_handler_args args)
{
    CAChannelGet *channelGet = static_cast<CAChannelGet*>(args.usr);
    channelGet->getDone(args);
}

typedef void (*copyDBRtoPVStructure)(const void * from, unsigned count, PVStructure::shared_pointer const & to);


// template<primitive type, scalar Field, array Field>
template<typename pT, typename sF, typename aF>
void copy_DBR(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<sF> value = std::tr1::static_pointer_cast<sF>(pvStructure->getSubFieldT("value"));
        value->put(static_cast<const pT*>(dbr)[0]);
    }
    else
    {
        std::tr1::shared_ptr<aF> value = pvStructure->getSubFieldT<aF>("value");
        typename aF::svector temp(value->reuse());
        temp.resize(count);
        std::copy(static_cast<const pT*>(dbr), static_cast<const pT*>(dbr) + count, temp.begin());
        value->replace(freeze(temp));
    }
}

#if defined(__vxworks) || defined(__rtems__)
// dbr_long_t is defined as "int", pvData uses int32 which can be defined as "long int" (32-bit)
// template<primitive type, scalar Field, array Field>
template<>
void copy_DBR<dbr_long_t, PVInt, PVIntArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVInt> value = std::tr1::static_pointer_cast<PVInt>(pvStructure->getSubFieldT("value"));
        value->put(static_cast<const int32*>(dbr)[0]);
    }
    else
    {
        std::tr1::shared_ptr<PVIntArray> value = pvStructure->getSubFieldT<PVIntArray>("value");
        PVIntArray::svector temp(value->reuse());
        temp.resize(count);
        std::copy(static_cast<const int32*>(dbr), static_cast<const int32*>(dbr) + count, temp.begin());
        value->replace(freeze(temp));
    }
}
#endif

// string specialization
template<>
void copy_DBR<string, PVString, PVStringArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVString> value = std::tr1::static_pointer_cast<PVString>(pvStructure->getSubField("value"));
        value->put(std::string(static_cast<const char*>(dbr)));
    }
    else
    {
        std::tr1::shared_ptr<PVStringArray> value = pvStructure->getSubFieldT<PVStringArray>("value");
        const dbr_string_t* dbrStrings = static_cast<const dbr_string_t*>(dbr);
        PVStringArray::svector sA(value->reuse());
        sA.resize(count);
        std::copy(dbrStrings, dbrStrings + count, sA.begin());
        value->replace(freeze(sA));
    }
}

// enum specialization
template<>
void copy_DBR<dbr_enum_t,  PVString, PVStringArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVInt> value = std::tr1::static_pointer_cast<PVInt>(pvStructure->getSubFieldT("value.index"));
        value->put(static_cast<const dbr_enum_t*>(dbr)[0]);
    }
    else
    {
        // not supported
        std::cerr << "caChannel: array of enums not supported" << std::endl;
    }
}

// template<DBR type, primitive type, scalar Field, array Field>
template<typename T, typename pT, typename sF, typename aF>
void copy_DBR_STS(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer alarm = pvStructure->getSubFieldT<PVStructure>("alarm");
    alarm->getSubFieldT<PVInt>("status")->put(dbrStatus2alarmStatus[data->status]);
    alarm->getSubFieldT<PVInt>("severity")->put(data->severity);
    alarm->getSubFieldT<PVString>("message")->put(dbrStatus2alarmMessage[data->status]);

    copy_DBR<pT, sF, aF>(&data->value, count, pvStructure);
}

// template<DBR type, primitive type, scalar Field, array Field>
template<typename T, typename pT, typename sF, typename aF>
void copy_DBR_TIME(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer ts = pvStructure->getSubFieldT<PVStructure>("timeStamp");
    epics::pvData::int64 spe = data->stamp.secPastEpoch;
    spe += 7305*86400;
    ts->getSubFieldT<PVLong>("secondsPastEpoch")->put(spe);
    ts->getSubFieldT<PVInt>("nanoseconds")->put(data->stamp.nsec);

    copy_DBR_STS<T, pT, sF, aF>(dbr, count, pvStructure);
}


template <typename T>
void copy_format(const void * /*dbr*/, PVStructure::shared_pointer const & pvDisplayStructure)
{
    pvDisplayStructure->getSubFieldT<PVString>("format")->put("%d");
}

template <>
void copy_format<dbr_time_string>(const void * /*dbr*/, PVStructure::shared_pointer const & pvDisplayStructure)
{
    pvDisplayStructure->getSubFieldT<PVString>("format")->put("%s");
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
        pvDisplayStructure->getSubFieldT<PVString>("format")->put(std::string(fmt)); \
    } \
    else \
    { \
        pvDisplayStructure->getSubFieldT<PVString>("format")->put("%f"); \
    } \
}

COPY_FORMAT_FOR(dbr_gr_float)
COPY_FORMAT_FOR(dbr_ctrl_float)
COPY_FORMAT_FOR(dbr_gr_double)
COPY_FORMAT_FOR(dbr_ctrl_double)

#undef COPY_FORMAT_FOR

// template<DBR type, primitive type, scalar Field, array Field>
template<typename T, typename pT, typename sF, typename aF>
void copy_DBR_GR(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer alarm = pvStructure->getSubFieldT<PVStructure>("alarm");
    alarm->getSubFieldT<PVInt>("status")->put(0);
    alarm->getSubFieldT<PVInt>("severity")->put(data->severity);
    alarm->getSubFieldT<PVString>("message")->put(dbrStatus2alarmMessage[data->status]);

    PVStructure::shared_pointer disp = pvStructure->getSubFieldT<PVStructure>("display");
    disp->getSubFieldT<PVString>("units")->put(std::string(data->units));
    disp->getSubFieldT<PVDouble>("limitHigh")->put(data->upper_disp_limit);
    disp->getSubFieldT<PVDouble>("limitLow")->put(data->lower_disp_limit);

    copy_format<T>(dbr, disp);

    PVStructure::shared_pointer va = pvStructure->getSubFieldT<PVStructure>("valueAlarm");
    va->getSubFieldT<PVDouble>("highAlarmLimit")->put(data->upper_alarm_limit);
    va->getSubFieldT<PVDouble>("highWarningLimit")->put(data->upper_warning_limit);
    va->getSubFieldT<PVDouble>("lowWarningLimit")->put(data->lower_warning_limit);
    va->getSubFieldT<PVDouble>("lowAlarmLimit")->put(data->lower_alarm_limit);

    copy_DBR<pT, sF, aF>(&data->value, count, pvStructure);
}

// enum specialization
template<>
void copy_DBR_GR<dbr_gr_enum, dbr_enum_t, PVString, PVStringArray>
(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const dbr_gr_enum* data = static_cast<const dbr_gr_enum*>(dbr);

    copy_DBR_STS<dbr_gr_enum, dbr_enum_t, PVString, PVStringArray>(data, count, pvStructure);
}


// template<DBR type, primitive type, scalar Field, array Field>
template<typename T, typename pT, typename sF, typename aF>
void copy_DBR_CTRL(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer alarm = pvStructure->getSubFieldT<PVStructure>("alarm");
    alarm->getSubFieldT<PVInt>("status")->put(0);
    alarm->getSubFieldT<PVInt>("severity")->put(data->severity);
    alarm->getSubFieldT<PVString>("message")->put(dbrStatus2alarmMessage[data->status]);

    PVStructure::shared_pointer disp = pvStructure->getSubFieldT<PVStructure>("display");
    disp->getSubFieldT<PVString>("units")->put(std::string(data->units));
    disp->getSubFieldT<PVDouble>("limitHigh")->put(data->upper_disp_limit);
    disp->getSubFieldT<PVDouble>("limitLow")->put(data->lower_disp_limit);

    copy_format<T>(dbr, disp);

    PVStructure::shared_pointer va = pvStructure->getSubFieldT<PVStructure>("valueAlarm");
    std::tr1::static_pointer_cast<sF>(va->getSubFieldT("highAlarmLimit"))->put(data->upper_alarm_limit);
    std::tr1::static_pointer_cast<sF>(va->getSubFieldT("highWarningLimit"))->put(data->upper_warning_limit);
    std::tr1::static_pointer_cast<sF>(va->getSubFieldT("lowWarningLimit"))->put(data->lower_warning_limit);
    std::tr1::static_pointer_cast<sF>(va->getSubFieldT("lowAlarmLimit"))->put(data->lower_alarm_limit);

    PVStructure::shared_pointer ctrl = pvStructure->getSubFieldT<PVStructure>("control");
    ctrl->getSubFieldT<PVDouble>("limitHigh")->put(data->upper_ctrl_limit);
    ctrl->getSubFieldT<PVDouble>("limitLow")->put(data->lower_ctrl_limit);

    copy_DBR<pT, sF, aF>(&data->value, count, pvStructure);
}

// enum specialization
template<>
void copy_DBR_CTRL<dbr_ctrl_enum, dbr_enum_t, PVString, PVStringArray>
(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const dbr_ctrl_enum* data = static_cast<const dbr_ctrl_enum*>(dbr);

    copy_DBR_STS<dbr_ctrl_enum, dbr_enum_t, PVString, PVStringArray>(data, count, pvStructure);
}


static copyDBRtoPVStructure copyFuncTable[] =
{
    copy_DBR<string, PVString, PVStringArray>,          // DBR_STRING
    copy_DBR<dbr_short_t, PVShort, PVShortArray>,          // DBR_INT, DBR_SHORT
    copy_DBR<dbr_float_t, PVFloat, PVFloatArray>,          // DBR_FLOAT
    copy_DBR<dbr_enum_t, PVString, PVStringArray>,          // DBR_ENUM
    copy_DBR<int8 /*dbr_char_t*/, PVByte, PVByteArray>,          // DBR_CHAR
    copy_DBR<dbr_long_t, PVInt, PVIntArray>,          // DBR_LONG
    copy_DBR<dbr_double_t, PVDouble, PVDoubleArray>,          // DBR_DOUBLE

    copy_DBR_STS<dbr_sts_string, string, PVString, PVStringArray>,          // DBR_STS_STRING
    copy_DBR_STS<dbr_sts_short, dbr_short_t, PVShort, PVShortArray>,          // DBR_STS_INT, DBR_STS_SHORT
    copy_DBR_STS<dbr_sts_float, dbr_float_t, PVFloat, PVFloatArray>,          // DBR_STS_FLOAT
    copy_DBR_STS<dbr_sts_enum, dbr_enum_t, PVString, PVStringArray>,          // DBR_STS_ENUM
    copy_DBR_STS<dbr_sts_char, int8 /*dbr_char_t*/, PVByte, PVByteArray>,          // DBR_STS_CHAR
    copy_DBR_STS<dbr_sts_long, dbr_long_t, PVInt, PVIntArray>,          // DBR_STS_LONG
    copy_DBR_STS<dbr_sts_double, dbr_double_t, PVDouble, PVDoubleArray>,          // DBR_STS_DOUBLE

    copy_DBR_TIME<dbr_time_string, string, PVString, PVStringArray>,          // DBR_TIME_STRING
    copy_DBR_TIME<dbr_time_short, dbr_short_t, PVShort, PVShortArray>,          // DBR_TIME_INT, DBR_TIME_SHORT
    copy_DBR_TIME<dbr_time_float, dbr_float_t, PVFloat, PVFloatArray>,          // DBR_TIME_FLOAT
    copy_DBR_TIME<dbr_time_enum, dbr_enum_t, PVString, PVStringArray>,          // DBR_TIME_ENUM
    copy_DBR_TIME<dbr_time_char, int8 /*dbr_char_t*/, PVByte, PVByteArray>,          // DBR_TIME_CHAR
    copy_DBR_TIME<dbr_time_long, dbr_long_t, PVInt, PVIntArray>,          // DBR_TIME_LONG
    copy_DBR_TIME<dbr_time_double, dbr_double_t, PVDouble, PVDoubleArray>,          // DBR_TIME_DOUBLE

    copy_DBR_STS<dbr_sts_string, string, PVString, PVStringArray>,          // DBR_GR_STRING -> DBR_STS_STRING
    copy_DBR_GR<dbr_gr_short, dbr_short_t, PVShort, PVShortArray>,          // DBR_GR_INT, DBR_GR_SHORT
    copy_DBR_GR<dbr_gr_float, dbr_float_t, PVFloat, PVFloatArray>,          // DBR_GR_FLOAT
    copy_DBR_GR<dbr_gr_enum, dbr_enum_t, PVString, PVStringArray>,          // DBR_GR_ENUM
    copy_DBR_GR<dbr_gr_char, int8 /*dbr_char_t*/, PVByte, PVByteArray>,          // DBR_GR_CHAR
    copy_DBR_GR<dbr_gr_long, dbr_long_t, PVInt, PVIntArray>,          // DBR_GR_LONG
    copy_DBR_GR<dbr_gr_double, dbr_double_t, PVDouble, PVDoubleArray>,          // DBR_GR_DOUBLE

    copy_DBR_STS<dbr_sts_string, string, PVString, PVStringArray>,          // DBR_CTRL_STRING -> DBR_STS_STRING
    copy_DBR_CTRL<dbr_ctrl_short, dbr_short_t, PVShort, PVShortArray>,          // DBR_CTRL_INT, DBR_CTRL_SHORT
    copy_DBR_CTRL<dbr_ctrl_float, dbr_float_t, PVFloat, PVFloatArray>,          // DBR_CTRL_FLOAT
    copy_DBR_CTRL<dbr_ctrl_enum, dbr_enum_t, PVString, PVStringArray>,          // DBR_CTRL_ENUM
    copy_DBR_CTRL<dbr_ctrl_char, int8 /*dbr_char_t*/, PVByte, PVByteArray>,          // DBR_CTRL_CHAR
    copy_DBR_CTRL<dbr_ctrl_long, dbr_long_t, PVInt, PVIntArray>,          // DBR_CTRL_LONG
    copy_DBR_CTRL<dbr_ctrl_double, dbr_double_t, PVDouble, PVDoubleArray>          // DBR_CTRL_DOUBLE
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

        EXCEPTION_GUARD(channelGetRequester->getDone(Status::Ok, shared_from_this(), pvStructure, bitSet));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        EXCEPTION_GUARD(channelGetRequester->getDone(errorStatus, shared_from_this(), PVStructure::shared_pointer(), BitSet::shared_pointer()));
    }
}


void CAChannelGet::get()
{
    channel->threadAttach();

    /*
    From R3.14.12 onwards ca_array_get_callback() replies will give a CA client application the current number
    of elements in an array field, provided they specified an element count of zero in their original request.
    The element count is passed in the callback argument structure.
    Prior to R3.14.12 requesting zero elements in a ca_array_get_callback() call was illegal and would fail
    immediately.
    */

    int result = ca_array_get_callback(getType,
#if (((EPICS_VERSION * 256 + EPICS_REVISION) * 256 + EPICS_MODIFICATION) >= ((3*256+14)*256+12))
                                       0,
#else
                                       channel->getElementCount(),
#endif
                                       channel->getChannelID(), ca_get_handler, this);
    if (result == ECA_NORMAL)
    {
        ca_flush_io();
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(result)));
        EXCEPTION_GUARD(channelGetRequester->getDone(errorStatus, shared_from_this(), PVStructure::shared_pointer(), BitSet::shared_pointer()));
    }

    if (lastRequestFlag)
        destroy();
}


/* --------------- epics::pvData::ChannelRequest --------------- */

Channel::shared_pointer CAChannelGet::getChannel()
{
    return channel;
}

void CAChannelGet::cancel()
{
    // noop
}

void CAChannelGet::lastRequest()
{
    // TODO sync !!!
    lastRequestFlag = true;
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
    // TODO use std::make_shared
    std::tr1::shared_ptr<CAChannelPut> tp(
        new CAChannelPut(channel, channelPutRequester, pvRequest)
    );
    ChannelPut::shared_pointer thisPtr = tp;
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
    bitSet(new BitSet(static_cast<uint32>(pvStructure->getStructure()->getNumberFields()))),
    lastRequestFlag(false)
{
    // NOTE: we require value type, we can only put value field
    bitSet->set(pvStructure->getSubFieldT("value")->getFieldOffset());
}

void CAChannelPut::activate()
{
    EXCEPTION_GUARD(channelPutRequester->channelPutConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
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
        std::tr1::shared_ptr<sF> value = std::tr1::static_pointer_cast<sF>(pvStructure->getSubFieldT("value"));

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
        std::tr1::shared_ptr<aF> value = pvStructure->getSubFieldT<aF>("value");

        const pT* val = value->view().data();
        int result = ca_array_put_callback(channel->getNativeType(), static_cast<unsigned long>(value->getLength()),
                                           channel->getChannelID(), val,
                                           ca_put_handler, usrArg);

        if (result == ECA_NORMAL)
        {
            ca_flush_io();
        }

        return result;
    }
}

// string specialization
template<>
int doPut_pvStructure<string, pvString, PVString, PVStringArray>(CAChannel::shared_pointer const & channel, void *usrArg, PVStructure::shared_pointer const & pvStructure)
{
    bool isScalarValue = pvStructure->getStructure()->getField("value")->getType() == scalar;

    if (isScalarValue)
    {
        std::tr1::shared_ptr<PVString> value = std::tr1::static_pointer_cast<PVString>(pvStructure->getSubFieldT("value"));

        string val = value->get();
        int result = ca_array_put_callback(channel->getNativeType(), 1,
                                           channel->getChannelID(), val.c_str(),
                                           ca_put_handler, usrArg);

        if (result == ECA_NORMAL)
        {
            ca_flush_io();
        }

        return result;
    }
    else
    {
        std::tr1::shared_ptr<PVStringArray> value = pvStructure->getSubFieldT<PVStringArray>("value");

        PVStringArray::const_svector stringArray(value->view());

        size_t arraySize = stringArray.size();
        size_t ca_stringBufferSize = arraySize * MAX_STRING_SIZE;
        char* ca_stringBuffer = new char[ca_stringBufferSize];
        memset(ca_stringBuffer, 0, ca_stringBufferSize);

        char *p = ca_stringBuffer;
        for(size_t i = 0; i < arraySize; i++)
        {
            string value = stringArray[i];
            size_t len = value.length();
            if (len >= MAX_STRING_SIZE)
                len = MAX_STRING_SIZE - 1;
            memcpy(p, value.c_str(), len);
            p += MAX_STRING_SIZE;
        }


        int result = ca_array_put_callback(channel->getNativeType(), arraySize,
                                           channel->getChannelID(), ca_stringBuffer,
                                           ca_put_handler, usrArg);
        delete[] ca_stringBuffer;

        if (result == ECA_NORMAL)
        {
            ca_flush_io();
        }

        return result;
    }
}

// enum specialization
template<>
int doPut_pvStructure<dbr_enum_t, pvString, PVString, PVStringArray>(CAChannel::shared_pointer const & channel, void *usrArg, PVStructure::shared_pointer const & pvStructure)
{
    bool isScalarValue = pvStructure->getStructure()->getField("value")->getType() == structure;

    if (isScalarValue)
    {
        std::tr1::shared_ptr<PVInt> value = std::tr1::static_pointer_cast<PVInt>(pvStructure->getSubFieldT("value.index"));

        dbr_enum_t val = value->get();
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
        // no enum arrays in V3
        return ECA_NOSUPPORT;
    }
}

static doPut doPutFuncTable[] =
{
    doPut_pvStructure<string, pvString, PVString, PVStringArray>,          // DBR_STRING
    doPut_pvStructure<dbr_short_t, pvShort, PVShort, PVShortArray>,          // DBR_INT, DBR_SHORT
    doPut_pvStructure<dbr_float_t, pvFloat, PVFloat, PVFloatArray>,          // DBR_FLOAT
    doPut_pvStructure<dbr_enum_t, pvString, PVString, PVStringArray>,          // DBR_ENUM
    doPut_pvStructure<int8 /*dbr_char_t*/, pvByte, PVByte, PVByteArray>,          // DBR_CHAR
#if defined(__vxworks) || defined(__rtems__)
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
        EXCEPTION_GUARD(channelPutRequester->putDone(Status::Ok, shared_from_this()));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        EXCEPTION_GUARD(channelPutRequester->putDone(errorStatus, shared_from_this()));
    }
}


void CAChannelPut::put(PVStructure::shared_pointer const & pvPutStructure,
                       BitSet::shared_pointer const & /*putBitSet*/)
{
    channel->threadAttach();

    doPut putFunc = doPutFuncTable[channel->getNativeType()];
    if (putFunc)
    {
        // TODO now we always put all
        int result = putFunc(channel, this, pvPutStructure);
        if (result != ECA_NORMAL)
        {
            Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(result)));
            EXCEPTION_GUARD(channelPutRequester->putDone(errorStatus, shared_from_this()));
        }
    }
    else
    {
        // TODO remove
        std::cout << "no put func implemented" << std::endl;
    }

    // TODO here???!!!
    if (lastRequestFlag)
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

        EXCEPTION_GUARD(channelPutRequester->getDone(Status::Ok, shared_from_this(), pvStructure, bitSet));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        EXCEPTION_GUARD(channelPutRequester->getDone(errorStatus, shared_from_this(),
                        PVStructure::shared_pointer(), BitSet::shared_pointer()));
    }

    // TODO here???!!!
    if (lastRequestFlag)
        destroy();
}


void CAChannelPut::get()
{
    channel->threadAttach();

    int result = ca_array_get_callback(getType, channel->getElementCount(),
                                       channel->getChannelID(), ca_put_get_handler, this);
    if (result == ECA_NORMAL)
    {
        ca_flush_io();
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(result)));
        EXCEPTION_GUARD(channelPutRequester->getDone(errorStatus, shared_from_this(),
                        PVStructure::shared_pointer(), BitSet::shared_pointer()));
    }
}



/* --------------- epics::pvData::ChannelRequest --------------- */

Channel::shared_pointer CAChannelPut::getChannel()
{
    return channel;
}

void CAChannelPut::cancel()
{
    // noop
}

void CAChannelPut::lastRequest()
{
    // TODO sync !!!
    lastRequestFlag = true;
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











Monitor::shared_pointer CAChannelMonitor::create(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    // TODO use std::make_shared
    std::tr1::shared_ptr<CAChannelMonitor> tp(
        new CAChannelMonitor(channel, monitorRequester, pvRequest)
    );
    Monitor::shared_pointer thisPtr = tp;
    static_cast<CAChannelMonitor*>(thisPtr.get())->activate();
    return thisPtr;
}


CAChannelMonitor::~CAChannelMonitor()
{
    // TODO
}


CAChannelMonitor::CAChannelMonitor(CAChannel::shared_pointer const & _channel,
                                   MonitorRequester::shared_pointer const & _monitorRequester,
                                   epics::pvData::PVStructure::shared_pointer const & pvRequest) :
    channel(_channel),
    monitorRequester(_monitorRequester),
    getType(getDBRType(pvRequest, _channel->getNativeType())),
    pvStructure(createPVStructure(_channel, getType)),
    changedBitSet(new BitSet(static_cast<uint32>(pvStructure->getStructure()->getNumberFields()))),
    overrunBitSet(new BitSet(static_cast<uint32>(pvStructure->getStructure()->getNumberFields()))),
    count(0),
    element(new MonitorElement())
{
    // TODO
    changedBitSet->set(0);

    element->pvStructurePtr = pvStructure;
    element->changedBitSet = changedBitSet;
    element->overrunBitSet = overrunBitSet;
}

void CAChannelMonitor::activate()
{
    // TODO remove
    thisPointer = shared_from_this();

    EXCEPTION_GUARD(monitorRequester->monitorConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}


/* --------------- Monitor --------------- */


static void ca_subscription_handler(struct event_handler_args args)
{
    CAChannelMonitor *channelMonitor = static_cast<CAChannelMonitor*>(args.usr);
    channelMonitor->subscriptionEvent(args);
}


void CAChannelMonitor::subscriptionEvent(struct event_handler_args &args)
{
    if (args.status == ECA_NORMAL)
    {
        // TODO override indicator

        copyDBRtoPVStructure copyFunc = copyFuncTable[getType];
        if (copyFunc)
            copyFunc(args.dbr, args.count, pvStructure);
        else
        {
            // TODO remove
            std::cout << "no copy func implemented" << std::endl;
        }

        {
            Lock lock(mutex);
            count = 1;
        }
        monitorRequester->monitorEvent(shared_from_this());
    }
    else
    {
        //Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        //EXCEPTION_GUARD(channelMonitorRequester->MonitorDone(errorStatus));
    }
}


epics::pvData::Status CAChannelMonitor::start()
{
    channel->threadAttach();

    /*
    From R3.14.12 onwards when using the IOC server and the C++ client libraries monitor callbacks
    replies will give a CA client application the current number of elements in an array field,
    provided they specified an element count of zero in their original request.
    The element count is passed in the callback argument structure.
    Prior to R3.14.12 you could request a zero-length subscription and the zero would mean
    “use the value of chid->element_count() for this particular channel”,
    but the length of the data you got in your callbacks would never change
    (the server would zero-fill everything after the current length of the field).
     */

    // TODO DBE_PROPERTY support
    int result = ca_create_subscription(getType,
                                        0 /*channel->getElementCount()*/,
                                        channel->getChannelID(), DBE_VALUE,
                                        ca_subscription_handler, this,
                                        &eventID);
    if (result == ECA_NORMAL)
    {
        ca_flush_io();
        return Status::Ok;
    }
    else
    {
        return Status(Status::STATUSTYPE_ERROR, string(ca_message(result)));
    }
}


epics::pvData::Status CAChannelMonitor::stop()
{
    channel->threadAttach();

    int result = ca_clear_subscription(eventID);

    if (result == ECA_NORMAL)
    {
        return Status::Ok;
    }
    else
    {
        return Status(Status::STATUSTYPE_ERROR, string(ca_message(result)));
    }
}


MonitorElementPtr CAChannelMonitor::poll()
{
    Lock lock(mutex);
    if (count)
    {
        count--;
        return element;
    }
    else
    {
        return nullElement;
    }
}


void CAChannelMonitor::release(MonitorElementPtr const & /*monitorElement*/)
{
    // noop
}

/* --------------- epics::pvData::ChannelRequest --------------- */

void CAChannelMonitor::cancel()
{
    // noop
}

/* --------------- epics::pvData::Destroyable --------------- */


void CAChannelMonitor::destroy()
{
    channel->threadAttach();

    ca_clear_subscription(eventID);

    // TODO
    thisPointer.reset();
}
