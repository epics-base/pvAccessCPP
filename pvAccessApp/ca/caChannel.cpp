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

void CAChannel::connected()
{
    StandardPVFieldPtr standardPVField = getStandardPVField();
    PVStructure::shared_pointer pvStructure;

    // TODO
    String properties("value,timeStamp");
    // TODO arrays
    unsigned elementCount = ca_element_count(channelID);

    chtype type = ca_field_type(channelID);
    switch (type)
    {
        case DBR_CHAR:
            // byte
            break;
        case DBR_SHORT:
            // short
            break;
        case DBR_ENUM:
            // enum
            break;
        case DBR_LONG:
            // int
            break;
        case DBR_FLOAT:
            // float
            break;
        case DBR_DOUBLE:
            // double
            pvStructure = (elementCount > 1) ?
                           standardPVField->scalarArray(pvDouble, properties) :
                           standardPVField->scalar(pvDouble, properties);
            break;
        case DBR_STRING:
            // string
            break;
        default:
            // TODO !!!
            break;
    }

    // TODO thread sync
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
    channelRequester(_channelRequester)
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
    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelProcess::shared_pointer nullChannelProcess;
    EXCEPTION_GUARD(channelProcessRequester->channelProcessConnect(errorStatus, nullChannelProcess));
    return nullChannelProcess;
}

ChannelGet::shared_pointer CAChannel::createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelGet::shared_pointer nullChannelGet;
    EXCEPTION_GUARD(channelGetRequester->channelGetConnect(errorStatus, nullChannelGet,
                                                           PVStructure::shared_pointer(), BitSet::shared_pointer()));
    return nullChannelGet;
}


ChannelPut::shared_pointer CAChannel::createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
{
    Status errorStatus(Status::STATUSTYPE_ERROR, "not implemented");
    ChannelPut::shared_pointer nullChannelPut;
    EXCEPTION_GUARD(channelPutRequester->channelPutConnect(errorStatus, nullChannelPut,
                                                           PVStructure::shared_pointer(), BitSet::shared_pointer()));
    return nullChannelPut;
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
