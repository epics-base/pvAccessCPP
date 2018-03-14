/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#include <epicsVersion.h>

#include <pv/standardField.h>
#include <pv/logger.h>
#include <pv/pvAccess.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "caChannel.h"
#include <pv/caStatus.h>

using namespace epics::pvData;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

namespace epics {
namespace pvAccess {
namespace ca {

#define EXCEPTION_GUARD(code) try { code; } \
        catch (std::exception &e) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d: %s", __FILE__, __LINE__, e.what()); } \
                catch (...) { LOG(logLevelError, "Unhandled exception caught from client code at %s:%d.", __FILE__, __LINE__); }


CAChannel::shared_pointer CAChannel::create(CAChannelProvider::shared_pointer const & channelProvider,
        std::string const & channelName,
        short priority,
        ChannelRequester::shared_pointer const & channelRequester)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::create " << channelName << endl;
    }
    CAChannelPtr thisPtr(
        new CAChannel(channelName, channelProvider, channelRequester));
    thisPtr->activate(priority);
    return thisPtr;
}

static void ca_connection_handler(struct connection_handler_args args)
{
    CAChannel *channel = static_cast<CAChannel*>(ca_puser(args.chid));

    if (args.op == CA_OP_CONN_UP) {
        channel->connected();
    } else if (args.op == CA_OP_CONN_DOWN) {
        channel->disconnected();
    }
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
        if (labelsArray)
        {
            PVStringArray::svector labels(labelsArray->reuse());
            labels.resize(dbr_enum_p->no_str);
            std::copy(dbr_enum_p->strs, dbr_enum_p->strs + dbr_enum_p->no_str, labels.begin());
            labelsArray->replace(freeze(labels));
        }
    }
    else
    {
        string mess("ca_get_labels_handler ");
        mess += ca_message(args.status);
        throw  std::runtime_error(mess);
    }
}

// Filter out unrequested fields from a source structure according to a
// structure conforming to the format of the "field" field of a pvRequest,
// preserving type ids of unchanged structures. If ntTop is true also preserve
// type id if none of the deleted top-level subfields are the value field.
static StructureConstPtr refineStructure(StructureConstPtr const & source,
           StructureConstPtr const & requestedFields, bool ntTop)
{
    if (requestedFields.get() == NULL || requestedFields->getNumberFields() == 0)
        return source;

    FieldBuilderPtr builder = getFieldCreate()->createFieldBuilder();
    bool addId = true;

    FieldConstPtrArray fields = source->getFields();
    StringArray names = source->getFieldNames();
    size_t i = 0;
    for (FieldConstPtrArray::const_iterator it = fields.begin(); it != fields.end(); ++it)
    {
        FieldConstPtr field = *it;
        const std::string & name = names[i++];
        FieldConstPtr reqField = requestedFields->getField(name);
        if (reqField.get())
        {
            if (field->getType() != structure || (reqField->getType() != structure))
                builder->add(name,field);
            else
            {
                StructureConstPtr substruct =
                    std::tr1::dynamic_pointer_cast<const Structure>(field);

                StructureConstPtr reqSubstruct =
                    std::tr1::dynamic_pointer_cast<const Structure>(reqField);

                StructureConstPtr nested = refineStructure(substruct, reqSubstruct, false);
                builder->add(name,nested);
                if (nested->getID() != substruct->getID())
                    addId = false;
            }
        }
        else if (!ntTop || name == "value")
            addId =  false;
    }
    if (addId)
        builder->setId(source->getID());
    return  builder->createStructure();
}

static PVStructure::shared_pointer createPVStructure(CAChannel::shared_pointer const & channel, string const & properties, PVStructurePtr pvRequest)
{   
    StructureConstPtr unrefinedStructure = createStructure(channel, properties);

    PVStructurePtr fieldPVStructure = pvRequest->getSubField<PVStructure>("field");
    StructureConstPtr finalStructure = fieldPVStructure.get() ?
        refineStructure(unrefinedStructure, fieldPVStructure->getStructure(),true) :
        unrefinedStructure;

    PVStructure::shared_pointer pvStructure = getPVDataCreate()->createPVStructure(finalStructure);
    if (channel->getNativeType() == DBR_ENUM)
    {
        PVScalarArrayPtr pvScalarArray = pvStructure->getSubField<PVStringArray>("value.choices");

        // TODO avoid getting labels if DBR_GR_ENUM or DBR_CTRL_ENUM is used in subsequent get
        int result = ca_array_get_callback(
            DBR_GR_ENUM, 1, channel->getChannelID(), ca_get_labels_handler, pvScalarArray.get());
        if (result == ECA_NORMAL)
        {
            result = ca_flush_io();
            // NOTE: we do not wait here, since all subsequent request (over TCP) is serialized
            // and will guarantee that ca_get_labels_handler is called first
        }
        if (result != ECA_NORMAL){
            string mess(channel->getChannelName() + "PVStructure::shared_pointer createPVStructure ");
            mess += "failed to get labels for enum ";
            throw  std::runtime_error(mess);
        }
    }
    return pvStructure;
}

static PVStructure::shared_pointer createPVStructure(CAChannel::shared_pointer const & channel, chtype dbrType, PVStructurePtr pvRequest)
{
    // Match to closest DBR type
    // NOTE: value is always there
    string properties;
    bool isArray = channel->getElementCount() > 1;
    if (dbrType >= DBR_CTRL_STRING)      // 28
    {
        if (dbrType != DBR_CTRL_STRING && dbrType != DBR_CTRL_ENUM)
        {
            if (isArray)
                properties = "value,alarm,display";
            else
                properties = "value,alarm,display,valueAlarm,control";
        }
        else
            properties = "value,alarm";
    }
    else if (dbrType >= DBR_GR_STRING)   // 21
    {
        if (dbrType != DBR_GR_STRING && dbrType != DBR_GR_ENUM)
        {
            if (isArray)
                properties = "value,alarm,display";
            else
                properties = "value,alarm,display,valueAlarm";
        }
        else
            properties = "value,alarm";
    }
    else if (dbrType >= DBR_TIME_STRING) // 14
        properties = "value,alarm,timeStamp";
    else if (dbrType >= DBR_STS_STRING)  // 7
        properties = "value,alarm";
    else
        properties = "value";

    return createPVStructure(channel, properties, pvRequest);
}


void CAChannel::connected()
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::connected " << channelName << endl;
    }
    std::queue<CAChannelPutPtr> putQ;
    std::queue<CAChannelGetPtr> getQ;
    std::queue<CAChannelMonitorPtr> monitorQ;
    {
        Lock lock(requestsMutex);
        // we assume array if element count > 1
        elementCount = ca_element_count(channelID);
        channelType = ca_field_type(channelID);
        bool isArray = elementCount > 1;

        // no valueAlarm and control,display for non-numeric type
        // no control,display for numeric arrays
        string allProperties =
            (channelType != DBR_STRING && channelType != DBR_ENUM) ?
            isArray ?
            "value,timeStamp,alarm,display" :
            "value,timeStamp,alarm,display,valueAlarm,control" :
            "value,timeStamp,alarm";
        Structure::const_shared_pointer structure = createStructure(
            shared_from_this(), allProperties);

        // TODO we need only Structure here
        this->structure = structure;
        
        std::vector<CAChannelGetWPtr>::const_iterator getiter;
        for (getiter = getList.begin(); getiter != getList.end(); ++getiter) {
            CAChannelGetPtr temp = (*getiter).lock();
            if(!temp) continue;
            getQ.push(temp);
        }
        std::vector<CAChannelPutWPtr>::const_iterator putiter;
        for (putiter = putList.begin(); putiter != putList.end(); ++putiter) {
            CAChannelPutPtr temp = (*putiter).lock();
            if(!temp) continue;
            putQ.push(temp);
        }
        std::vector<CAChannelMonitorWPtr>::const_iterator monitoriter;
        for (monitoriter = monitorList.begin(); monitoriter != monitorList.end(); ++monitoriter) {
            CAChannelMonitorPtr temp = (*monitoriter).lock();
            if(!temp) continue;
            monitorQ.push(temp);
        }
    }
    while(!putQ.empty()) {
        putQ.front()->channelCreated(Status::Ok,shared_from_this());
        putQ.pop();
    }
    while(!getQ.empty()) {
         getQ.front()->channelCreated(Status::Ok,shared_from_this());
         getQ.pop();
    }
    while(!monitorQ.empty()) {
         monitorQ.front()->channelCreated(Status::Ok,shared_from_this());
         monitorQ.pop();
    }
    while(!getFieldQueue.empty()) {
        getFieldQueue.front()->callRequester(shared_from_this());
        getFieldQueue.pop();
    }
    while(!putQueue.empty()) {
        putQueue.front()->activate();
        putQueue.pop();
    }
    while(!getQueue.empty()) {
        getQueue.front()->activate();
        getQueue.pop();
    }
    while(!monitorQueue.empty()) {
        monitorQueue.front()->activate();
        monitorQueue.pop();
    }
    ChannelRequester::shared_pointer req(channelRequester.lock());
    if(req) {
        EXCEPTION_GUARD(req->channelStateChange(
             shared_from_this(), Channel::CONNECTED));
    }
}

void CAChannel::disconnected()
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::disconnected " << channelName << endl;
    }
    std::queue<CAChannelPutPtr> putQ;
    std::queue<CAChannelGetPtr> getQ;
    std::queue<CAChannelMonitorPtr> monitorQ;
    {
        Lock lock(requestsMutex);
        std::vector<CAChannelGetWPtr>::const_iterator getiter;
        for (getiter = getList.begin(); getiter != getList.end(); ++getiter) {
            CAChannelGetPtr temp = (*getiter).lock();
            if(!temp) continue;
            getQ.push(temp);
        }
        std::vector<CAChannelPutWPtr>::const_iterator putiter;
        for (putiter = putList.begin(); putiter != putList.end(); ++putiter) {
            CAChannelPutPtr temp = (*putiter).lock();
            if(!temp) continue;
            putQ.push(temp);
        }
        std::vector<CAChannelMonitorWPtr>::const_iterator monitoriter;
        for (monitoriter = monitorList.begin(); monitoriter != monitorList.end(); ++monitoriter) {
            CAChannelMonitorPtr temp = (*monitoriter).lock();
            if(!temp) continue;
            monitorQ.push(temp);
        }
    }
    while(!putQ.empty()) {
         putQ.front()->channelDisconnect(false);
         putQ.pop();
    }
    while(!getQ.empty()) {
         getQ.front()->channelDisconnect(false);
         getQ.pop();
    }
    while(!monitorQ.empty()) {
         monitorQ.front()->channelDisconnect(false);
         monitorQ.pop();
    }
    ChannelRequester::shared_pointer req(channelRequester.lock());
    if(req) {
        EXCEPTION_GUARD(req->channelStateChange(
             shared_from_this(), Channel::DISCONNECTED));
    }
}

size_t CAChannel::num_instances;

CAChannel::CAChannel(std::string const & _channelName,
                     CAChannelProvider::shared_pointer const & _channelProvider,
                     ChannelRequester::shared_pointer const & _channelRequester) :
    channelName(_channelName),
    channelProvider(_channelProvider),
    channelRequester(_channelRequester),
    channelID(0),
    channelType(0),
    elementCount(0),
    channelCreated(false)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::CAChannel " << channelName << endl;
    }
}

void CAChannel::activate(short priority)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::activate " << channelName << endl;
    }
    ChannelRequester::shared_pointer req(channelRequester.lock());
    if(!req) return;
    attachContext();
    int result = ca_create_channel(channelName.c_str(),
         ca_connection_handler,
         this,
         priority, // TODO mapping
         &channelID);
    if (result == ECA_NORMAL)
    {
       channelCreated = true;
       CAChannelProviderPtr provider(channelProvider.lock());
       if(provider) provider->addChannel(shared_from_this());
       EXCEPTION_GUARD(req->channelCreated(Status::Ok, shared_from_this()));
    } else {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(result)));
        EXCEPTION_GUARD(req->channelCreated(errorStatus, shared_from_this()));
    }
}

CAChannel::~CAChannel()
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannel::~CAChannel() " << channelName << endl;
    }
    disconnectChannel();
}

void CAChannel::disconnectChannel()
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannel::disconnectChannel() "
             << channelName 
             << " channelCreated " << (channelCreated ? "true" : "false")
             << endl;
    }
    {
        Lock lock(requestsMutex);
        if(!channelCreated) return;
        channelCreated = false;
    }
    /* Clear CA Channel */
    attachContext();
    int result = ca_clear_channel(channelID);
    if (result == ECA_NORMAL) return;
    string mess("CAChannel::disconnectChannel() ");
    mess += ca_message(result);
    cerr << mess << endl;
}



void CAChannel::addChannelGet(const CAChannelGetPtr & get)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::addChannelGet " << channelName << endl;
    }
    Lock lock(requestsMutex);
    for(size_t i=0; i< getList.size(); ++i) {
         if(!(getList[i].lock())) {
             getList[i] = get;
             return;
         }
    }
    getList.push_back(get);
}

void CAChannel::addChannelPut(const CAChannelPutPtr & put)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::addChannelPut " << channelName << endl;
    }
    Lock lock(requestsMutex);
    for(size_t i=0; i< putList.size(); ++i) {
         if(!(putList[i].lock())) {
             putList[i] = put;
             return;
         }
    }
    putList.push_back(put);
}


void CAChannel::addChannelMonitor(const CAChannelMonitorPtr & monitor)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::addChannelMonitor " << channelName << endl;
    }
    Lock lock(requestsMutex);
    for(size_t i=0; i< monitorList.size(); ++i) {
         if(!(monitorList[i].lock())) {
             monitorList[i] = monitor;
             return;
         }
    }
    monitorList.push_back(monitor);
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

Structure::const_shared_pointer CAChannel::getStructure()
{
    return structure;
}


std::tr1::shared_ptr<ChannelProvider> CAChannel::getProvider()
{
    return channelProvider.lock();
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
    return channelRequester.lock();
}

void CAChannel::getField(GetFieldRequester::shared_pointer const & requester,
                         std::string const & subField)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannel::getField " << channelName << endl;
    }
    CAChannelGetFieldPtr getField(new CAChannelGetField(requester,subField));
    {
         Lock lock(requestsMutex);
         if(getConnectionState()!=Channel::CONNECTED) {
             getFieldQueue.push(getField);
             return;
         }
     }
     getField->callRequester(shared_from_this());
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


ChannelGet::shared_pointer CAChannel::createChannelGet(
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannel::createChannelGet " << channelName << endl;
    }
    CAChannelGetPtr channelGet = 
        CAChannelGet::create(shared_from_this(), channelGetRequester, pvRequest);
    {
         Lock lock(requestsMutex);
         if(getConnectionState()!=Channel::CONNECTED) {
              getQueue.push(channelGet);
              return channelGet;
         }
    }
    channelGet->activate();
    return channelGet;
}


ChannelPut::shared_pointer CAChannel::createChannelPut(
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannel::createChannelPut " << channelName << endl;
    }
    CAChannelPutPtr channelPut = 
        CAChannelPut::create(shared_from_this(), channelPutRequester, pvRequest);
    {
         Lock lock(requestsMutex);
         if(getConnectionState()!=Channel::CONNECTED) {
              putQueue.push(channelPut);
              return channelPut;
         }
    }
    channelPut->activate();
    return channelPut;
}


Monitor::shared_pointer CAChannel::createMonitor(
    MonitorRequester::shared_pointer const & monitorRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannel::createMonitor " << channelName << endl;
    }
    CAChannelMonitorPtr channelMonitor = 
        CAChannelMonitor::create(shared_from_this(), monitorRequester, pvRequest);
    {
         Lock lock(requestsMutex);
         if(getConnectionState()!=Channel::CONNECTED) {
              monitorQueue.push(channelMonitor);
              return channelMonitor;
         }
    }
    channelMonitor->activate();
    return channelMonitor;
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


CAChannelGetField::CAChannelGetField(
    GetFieldRequester::shared_pointer const & requester,std::string const & subField)
  : getFieldRequester(requester),
    subField(subField)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGetField::CAChannelGetField()\n";
    }
}

CAChannelGetField::~CAChannelGetField()
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGetField::~CAChannelGetField()\n";
    }
}

void CAChannelGetField::callRequester(CAChannelPtr const & caChannel)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGetField::callRequester\n";
    }
    GetFieldRequester::shared_pointer requester(getFieldRequester.lock());
    if(!requester) return;
    epics::pvData::Structure::const_shared_pointer structure(caChannel->getStructure());
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

/* ---------------------------------------------------------- */

void CAChannel::attachContext()
{
    CAChannelProviderPtr provider(channelProvider.lock());
    if(provider) {
        std::tr1::static_pointer_cast<CAChannelProvider>(provider)->attachContext();
    }
}

static chtype getDBRType(PVStructure::shared_pointer const & pvRequest, chtype nativeType)
{
    // get "field" sub-structure
    PVStructure::shared_pointer fieldSubField =
        std::tr1::dynamic_pointer_cast<PVStructure>(pvRequest->getSubField("field"));
    if (!fieldSubField)
        fieldSubField = pvRequest;
    Structure::const_shared_pointer fieldStructure = fieldSubField->getStructure();

    // no fields
    if (fieldStructure->getNumberFields() == 0)
    {
        return static_cast<chtype>(static_cast<int>(nativeType) + DBR_TIME_STRING);
    }
    // control -> DBR_CTRL_<type>
    if (fieldStructure->getField("control"))
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

size_t CAChannelGet::num_instances;

CAChannelGetPtr CAChannelGet::create(
    CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelGetPtr(new CAChannelGet(channel, channelGetRequester, pvRequest));
}


CAChannelGet::CAChannelGet(CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
  :
    channel(channel),
    channelGetRequester(channelGetRequester),
    pvRequest(pvRequest),
    firstTime(true)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGet::CAChannelGet() " << channel->getChannelName() << endl;
    }
}

CAChannelGet::~CAChannelGet()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::~CAChannelGet() " <<  channel->getChannelName() << endl;
    }
}

void CAChannelGet::activate()
{
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::activate " <<  channel->getChannelName() << " requester "<<getRequester<<"\n";
    }
    if(!getRequester) return;
    if(pvStructure) throw  std::runtime_error("CAChannelGet::activate() was called twice");
    getType = getDBRType(pvRequest, channel->getNativeType());
    pvStructure = createPVStructure(channel, getType, pvRequest);
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    pvCopy = PVCopy::create(
          createPVStructure(channel, getType, pvRequest),
          CreateRequest::create()->createRequest("field()"),
          "");
    channel->addChannelGet(shared_from_this());
    if(channel->getConnectionState()==Channel::CONNECTED) {
         EXCEPTION_GUARD(getRequester->channelGetConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
    }
}

void CAChannelGet::channelCreated(const Status& status,Channel::shared_pointer const & cl)
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::channelCreated " <<  channel->getChannelName() << endl;
    }
    firstTime = true;
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    chtype newType = getDBRType(pvRequest, channel->getNativeType());
    if(newType!=getType) {
        getType = getDBRType(pvRequest, channel->getNativeType());
        pvStructure = createPVStructure(channel, getType, pvRequest);
        bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
        pvCopy = PVCopy::create(
          createPVStructure(channel, getType, pvRequest),
          CreateRequest::create()->createRequest("field()"),
          "");
    }
    EXCEPTION_GUARD(getRequester->channelGetConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

void CAChannelGet::channelStateChange(
     Channel::shared_pointer const & channel,
     Channel::ConnectionState connectionState)
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::channelStateChange " <<  channel->getChannelName() << endl;
    }
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    if(connectionState==Channel::DISCONNECTED || connectionState==Channel::DESTROYED) {
        EXCEPTION_GUARD(getRequester->channelDisconnect(connectionState==Channel::DESTROYED);)
    }
}

void CAChannelGet::channelDisconnect(bool destroy)
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::channelDisconnect " <<  channel->getChannelName() << endl;
    }
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    EXCEPTION_GUARD(getRequester->channelDisconnect(destroy);)
}

/* --------------- epics::pvAccess::ChannelGet --------------- */

namespace {

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
        std::tr1::shared_ptr<sF> value = pvStructure->getSubField<sF>("value");
        if (value.get()) value->put(static_cast<const pT*>(dbr)[0]);
    }
    else
    {
        std::tr1::shared_ptr<aF> value = pvStructure->getSubField<aF>("value");
        if (value.get())
        {
            std::tr1::shared_ptr<aF> value = pvStructure->getSubField<aF>("value");
            typename aF::svector temp(value->reuse());
            temp.resize(count);
            std::copy(static_cast<const pT*>(dbr), static_cast<const pT*>(dbr) + count, temp.begin());
            value->replace(freeze(temp));
        }
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
        std::tr1::shared_ptr<PVInt> value = pvStructure->getSubField<PVInt>("value");
        if (value.get()) value->put(static_cast<const int32*>(dbr)[0]);
    }
    else
    {
        std::tr1::shared_ptr<PVIntArray> value = pvStructure->getSubField<PVIntArray>("value");
        if (value.get())
        {
            PVIntArray::svector temp(value->reuse());
            temp.resize(count);
            std::copy(static_cast<const int32*>(dbr), static_cast<const int32*>(dbr) + count, temp.begin());
            value->replace(freeze(temp));
        }
    }
}
#endif

// string specialization
template<>
void copy_DBR<string, PVString, PVStringArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        std::tr1::shared_ptr<PVString> value = pvStructure->getSubField<PVString>("value");
        if (value.get()) value->put(std::string(static_cast<const char*>(dbr)));
    }
    else
    {
        std::tr1::shared_ptr<PVStringArray> value = pvStructure->getSubField<PVStringArray>("value");
        if (value.get())
        {
            const dbr_string_t* dbrStrings = static_cast<const dbr_string_t*>(dbr);
            PVStringArray::svector sA(value->reuse());
            sA.resize(count);
            std::copy(dbrStrings, dbrStrings + count, sA.begin());
            value->replace(freeze(sA));
        }
    }
}

// enum specialization
template<>
void copy_DBR<dbr_enum_t,  PVString, PVStringArray>(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    if (count == 1)
    {
        PVIntPtr value = pvStructure->getSubField<PVInt>("value.index");
        if (value.get()) value->put(static_cast<const dbr_enum_t*>(dbr)[0]);
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

    PVStructure::shared_pointer alarm = pvStructure->getSubField<PVStructure>("alarm");
    if (alarm.get())
    {
        PVIntPtr status = alarm->getSubField<PVInt>("status");
        if (status.get()) status->put(dbrStatus2alarmStatus[data->status]);

        PVIntPtr severity = alarm->getSubField<PVInt>("severity");
        if (severity.get()) severity->put(data->severity);

        PVStringPtr message = alarm->getSubField<PVString>("message");
        if (message.get()) message->put(dbrStatus2alarmMessage[data->status]);
    }

    copy_DBR<pT, sF, aF>(&data->value, count, pvStructure);
}

// template<DBR type, primitive type, scalar Field, array Field>
template<typename T, typename pT, typename sF, typename aF>
void copy_DBR_TIME(const void * dbr, unsigned count, PVStructure::shared_pointer const & pvStructure)
{
    const T* data = static_cast<const T*>(dbr);

    PVStructure::shared_pointer ts = pvStructure->getSubField<PVStructure>("timeStamp");
    if (ts.get())
    {
        epics::pvData::int64 spe = data->stamp.secPastEpoch;
        spe += 7305*86400;

        PVLongPtr secondsPastEpoch = ts->getSubField<PVLong>("secondsPastEpoch");
        if (secondsPastEpoch.get()) secondsPastEpoch->put(spe);

        PVIntPtr nanoseconds = ts->getSubField<PVInt>("nanoseconds");
        if (nanoseconds.get()) nanoseconds->put(data->stamp.nsec);
    }

    copy_DBR_STS<T, pT, sF, aF>(dbr, count, pvStructure);
}


template <typename T>
void copy_format(const void * /*dbr*/, PVStructure::shared_pointer const & pvDisplayStructure)
{
    PVStringPtr format = pvDisplayStructure->getSubField<PVString>("format");
    if (format.get()) format->put("%d");
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
        PVStringPtr format = pvDisplayStructure->getSubField<PVString>("format");\
        if (format.get()) format->put(std::string(fmt));\
    } \
    else \
    { \
        PVStringPtr format = pvDisplayStructure->getSubField<PVString>("format");\
        if (format.get()) format->put("%f");\
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

    PVStructurePtr alarm = pvStructure->getSubField<PVStructure>("alarm");
    if (alarm.get())
    {
        PVIntPtr status = alarm->getSubField<PVInt>("status");
        if (status.get()) status->put(dbrStatus2alarmStatus[data->status]);

        PVIntPtr severity = alarm->getSubField<PVInt>("severity");
        if (severity.get()) severity->put(data->severity);

        PVStringPtr message = alarm->getSubField<PVString>("message");
        if (message.get()) message->put(dbrStatus2alarmMessage[data->status]);
    }

    PVStructurePtr disp = pvStructure->getSubField<PVStructure>("display");
    if (disp.get())
    {
        PVStringPtr units = disp->getSubField<PVString>("units");
        if (units.get()) units->put(std::string(data->units));

        PVDoublePtr limitHigh = disp->getSubField<PVDouble>("limitHigh");
        if (limitHigh.get()) limitHigh->put(data->upper_disp_limit);

        PVDoublePtr limitLow = disp->getSubField<PVDouble>("limitLow");
        if (limitLow.get()) limitLow->put(data->lower_disp_limit);

        copy_format<T>(dbr, disp);
    }

    PVStructurePtr va = pvStructure->getSubField<PVStructure>("valueAlarm");
    if (va.get())
    {
        std::tr1::shared_ptr<sF> highAlarmLimit = va->getSubField<sF>("highAlarmLimit");
        if (highAlarmLimit.get()) highAlarmLimit->put(data->upper_alarm_limit);

        std::tr1::shared_ptr<sF> highWarningLimit = va->getSubField<sF>("highWarningLimit");
        if (highWarningLimit.get()) highWarningLimit->put(data->upper_warning_limit);

        std::tr1::shared_ptr<sF> lowWarningLimit = va->getSubField<sF>("lowWarningLimit");
        if (lowWarningLimit.get()) lowWarningLimit->put(data->lower_warning_limit);

        std::tr1::shared_ptr<sF> lowAlarmLimit = va->getSubField<sF>("lowAlarmLimit");
        if (lowAlarmLimit.get()) lowAlarmLimit->put(data->lower_alarm_limit);
    }
    
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

    PVStructure::shared_pointer alarm = pvStructure->getSubField<PVStructure>("alarm");
    if (alarm.get())
    {
        PVIntPtr status = alarm->getSubField<PVInt>("status");
        if (status.get()) status->put(dbrStatus2alarmStatus[data->status]);

        PVIntPtr severity = alarm->getSubField<PVInt>("severity");
        if (severity.get()) severity->put(data->severity);

        PVStringPtr message = alarm->getSubField<PVString>("message");
        if (message.get()) message->put(dbrStatus2alarmMessage[data->status]);
    }

    PVStructurePtr disp = pvStructure->getSubField<PVStructure>("display");
    if (disp.get())
    {
        PVStringPtr units = disp->getSubField<PVString>("units");
        if (units.get()) units->put(std::string(data->units));

        PVDoublePtr limitHigh = disp->getSubField<PVDouble>("limitHigh");
        if (limitHigh.get()) limitHigh->put(data->upper_disp_limit);

        PVDoublePtr limitLow = disp->getSubField<PVDouble>("limitLow");
        if (limitLow.get()) limitLow->put(data->lower_disp_limit);

        copy_format<T>(dbr, disp);
    }

    PVStructurePtr va = pvStructure->getSubField<PVStructure>("valueAlarm");
    if (va.get())
    {
        std::tr1::shared_ptr<sF> highAlarmLimit = va->getSubField<sF>("highAlarmLimit");
        if (highAlarmLimit.get()) highAlarmLimit->put(data->upper_alarm_limit);

        std::tr1::shared_ptr<sF> highWarningLimit = va->getSubField<sF>("highWarningLimit");
        if (highWarningLimit.get()) highWarningLimit->put(data->upper_warning_limit);

        std::tr1::shared_ptr<sF> lowWarningLimit = va->getSubField<sF>("lowWarningLimit");
        if (lowWarningLimit.get()) lowWarningLimit->put(data->lower_warning_limit);

        std::tr1::shared_ptr<sF> lowAlarmLimit = va->getSubField<sF>("lowAlarmLimit");
        if (lowAlarmLimit.get()) lowAlarmLimit->put(data->lower_alarm_limit);
    }

    PVStructurePtr ctrl = pvStructure->getSubField<PVStructure>("control");
    if (ctrl.get())
    {
        PVDoublePtr limitHigh = ctrl->getSubField<PVDouble>("limitHigh");
        if (limitHigh.get()) limitHigh->put(data->upper_ctrl_limit);

        PVDoublePtr limitLow = ctrl->getSubField<PVDouble>("limitLow");
        if (limitLow.get()) limitLow->put(data->lower_ctrl_limit);
    }

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

} // namespace

void CAChannelGet::getDone(struct event_handler_args &args)
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelGet::getDone " 
            <<  channel->getChannelName()
            << " firstTime " << (firstTime ? "true" : "false") 
            << endl;
    }
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    if (args.status == ECA_NORMAL)
    {
        copyDBRtoPVStructure copyFunc = copyFuncTable[getType];
        if (copyFunc)
            copyFunc(args.dbr, args.count, pvStructure);
        else
        {
            throw  std::runtime_error("CAChannelGet::getDone no copy func implemented");
        }
        pvCopy->updateMasterSetBitSet(pvStructure,bitSet);
        if(firstTime) {
            bitSet->clear();
            bitSet->set(0);
            firstTime = false;
        }
        EXCEPTION_GUARD(getRequester->getDone(Status::Ok, shared_from_this(), pvStructure, bitSet));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        EXCEPTION_GUARD(getRequester->getDone(errorStatus, shared_from_this(),
           PVStructure::shared_pointer(), BitSet::shared_pointer()));
    }
}


void CAChannelGet::get()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelGet::get " <<  channel->getChannelName() << endl;
    }
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    channel->attachContext();

    /*
    From R3.14.12 onwards ca_array_get_callback() replies will give a CA client application the current number
    of elements in an array field, provided they specified an element count of zero in their original request.
    The element count is passed in the callback argument structure.
    Prior to R3.14.12 requesting zero elements in a ca_array_get_callback() call was illegal and would fail
    immediately.
    */
    bitSet->clear();
    int result = ca_array_get_callback(getType,
         0,
         channel->getChannelID(), ca_get_handler, this);
    if (result == ECA_NORMAL)
    {
        result = ca_flush_io();
    }
    if (result == ECA_NORMAL) return;
    string mess("CAChannelGet::get ");
    mess += ca_message(result);
    throw  std::runtime_error(mess);
}


/* --------------- epics::pvData::ChannelRequest --------------- */

Channel::shared_pointer CAChannelGet::getChannel()
{
    return channel;
}

void CAChannelGet::cancel()
{
}

void CAChannelGet::lastRequest()
{
}



CAChannelPutPtr CAChannelPut::create(
    CAChannel::shared_pointer const & channel,
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelPutPtr(new CAChannelPut(channel, channelPutRequester, pvRequest));
}


CAChannelPut::~CAChannelPut()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelPut::~CAChannelPut() " << channel->getChannelName() << endl;
    }
}

size_t CAChannelPut::num_instances;

CAChannelPut::CAChannelPut(CAChannel::shared_pointer const & channel,
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
:
    channel(channel),
    channelPutRequester(channelPutRequester),
    pvRequest(pvRequest),
    block(false)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::CAChannePut() " << channel->getChannelName() << endl;
    }
}

void CAChannelPut::activate()
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::activate " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    if(pvStructure) throw  std::runtime_error("CAChannelPut::activate() was called twice");
    getType = getDBRType(pvRequest,channel->getNativeType());
    pvStructure = createPVStructure(channel, getType, pvRequest);
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    PVStringPtr pvString = pvRequest->getSubField<PVString>("record._options.block");
    if(pvString) {
        std::string val = pvString->get();
        if(val.compare("true")==0) block = true;
    }
    bitSet->set(pvStructure->getSubFieldT("value")->getFieldOffset());
    channel->addChannelPut(shared_from_this());
    if(channel->getConnectionState()==Channel::CONNECTED) {
         EXCEPTION_GUARD(putRequester->channelPutConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
    }
}


void CAChannelPut::channelCreated(const Status& status,Channel::shared_pointer const & c)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::channelCreated " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    chtype newType = getDBRType(pvRequest, channel->getNativeType());
    if(newType!=getType) {
        getType = getDBRType(pvRequest, channel->getNativeType());
        pvStructure = createPVStructure(channel, getType, pvRequest);
        bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
        PVStringPtr pvString = pvRequest->getSubField<PVString>("record._options.block");
        if(pvString) {
            std::string val = pvString->get();
            if(val.compare("true")==0) block = true;
        }
        bitSet->set(0);
    }
    EXCEPTION_GUARD(putRequester->channelPutConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

void CAChannelPut::channelStateChange(
     Channel::shared_pointer const & channel,
     Channel::ConnectionState connectionState)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::channelStateChange " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    if(connectionState==Channel::DISCONNECTED || connectionState==Channel::DESTROYED) {
        EXCEPTION_GUARD(putRequester->channelDisconnect(connectionState==Channel::DESTROYED);)
    }
}

void CAChannelPut::channelDisconnect(bool destroy)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::channelDisconnect " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    EXCEPTION_GUARD(putRequester->channelDisconnect(destroy);)
}

/* --------------- epics::pvAccess::ChannelPut --------------- */

namespace {

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
        int result = 0;
        if(usrArg!=NULL) {
            result = ca_array_put_callback(channel->getNativeType(), 1,
                channel->getChannelID(), &val,
                ca_put_handler, usrArg);
        } else {
            result = ca_array_put(channel->getNativeType(), 1,
                channel->getChannelID(), &val);
        }

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
        int result = 0;
        if(usrArg!=NULL) {
            result = ca_array_put_callback(channel->getNativeType(),
                static_cast<unsigned long>(value->getLength()),
                channel->getChannelID(), val,
                ca_put_handler, usrArg);
        } else {
            result = ca_array_put(channel->getNativeType(),
                static_cast<unsigned long>(value->getLength()),
                channel->getChannelID(), val);
        }
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
        int result = 0;
        if(usrArg!=NULL) {
            result = ca_array_put_callback(
            channel->getNativeType(), 1,
            channel->getChannelID(), val.c_str(),
            ca_put_handler, usrArg);
        } else {
            result = ca_array_put(
            channel->getNativeType(), 1,
            channel->getChannelID(), val.c_str());
        }
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

        int result = 0;
        if(usrArg!=NULL) {
            result = ca_array_put_callback(
                 channel->getNativeType(), arraySize,
                 channel->getChannelID(), ca_stringBuffer,
                 ca_put_handler, usrArg);
        } else {
            result = ca_array_put(
                 channel->getNativeType(), arraySize,
                 channel->getChannelID(), ca_stringBuffer);
        }
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
        int result = 0;
        if(usrArg!=NULL) {
            result = ca_array_put_callback(
                 channel->getNativeType(), 1,
                 channel->getChannelID(), &val,
                 ca_put_handler, usrArg);
        } else {
            result = ca_array_put(
                 channel->getNativeType(), 1,
                 channel->getChannelID(), &val);
        }
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

} // namespace

void CAChannelPut::putDone(struct event_handler_args &args)
{
    if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::putDone " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    if (args.status == ECA_NORMAL)
    {
        EXCEPTION_GUARD(putRequester->putDone(Status::Ok, shared_from_this()));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        EXCEPTION_GUARD(putRequester->putDone(errorStatus, shared_from_this()));
    }
}

void CAChannelPut::put(PVStructure::shared_pointer const & pvPutStructure,
                       BitSet::shared_pointer const & /*putBitSet*/)
{
    if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::put " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    doPut putFunc = doPutFuncTable[channel->getNativeType()];
    if (putFunc)
    {
        // TODO now we always put all 
        if(block) {
            channel->attachContext();
            int result = putFunc(channel, this, pvPutStructure);
            if (result != ECA_NORMAL)
            {
                string message(ca_message(result));
                Status errorStatus(Status::STATUSTYPE_ERROR, message);
                EXCEPTION_GUARD(putRequester->putDone(errorStatus, shared_from_this()));
            }
        } else {
            channel->attachContext();
            int result = putFunc(channel,NULL, pvPutStructure);
            if (result == ECA_NORMAL)
            {
                EXCEPTION_GUARD(putRequester->putDone(Status::Ok, shared_from_this()));
            }
            else
            {
                string message(ca_message(result));
                Status errorStatus(Status::STATUSTYPE_ERROR,message);
                EXCEPTION_GUARD(putRequester->putDone(errorStatus, shared_from_this()));
            }
        }
    }
    else
    {
        // TODO remove
        std::cout << "no put func implemented" << std::endl;
    }

}


void CAChannelPut::getDone(struct event_handler_args &args)
{
    if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::getDone " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
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

        EXCEPTION_GUARD(putRequester->getDone(Status::Ok, shared_from_this(), pvStructure, bitSet));
    }
    else
    {
        Status errorStatus(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
        EXCEPTION_GUARD(putRequester->getDone(errorStatus, shared_from_this(),
                        PVStructure::shared_pointer(), BitSet::shared_pointer()));
    }

}


void CAChannelPut::get()
{
    if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::get " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    channel->attachContext();

    int result = ca_array_get_callback(getType, channel->getElementCount(),
         channel->getChannelID(), ca_put_get_handler, this);
    
    if (result == ECA_NORMAL)
    {
        result = ca_flush_io();
    }
    if (result == ECA_NORMAL) return;
    string message(ca_message(result));
    Status errorStatus(Status::STATUSTYPE_ERROR, message);
    EXCEPTION_GUARD(putRequester->getDone(errorStatus, shared_from_this(),
                        PVStructure::shared_pointer(), BitSet::shared_pointer()));
}



/* --------------- epics::pvData::ChannelRequest --------------- */

Channel::shared_pointer CAChannelPut::getChannel()
{
    return channel;
}

void CAChannelPut::cancel()
{
}

void CAChannelPut::lastRequest()
{
}


/* --------------- Monitor --------------- */


static void ca_subscription_handler(struct event_handler_args args)
{
    CAChannelMonitor *channelMonitor = static_cast<CAChannelMonitor*>(args.usr);
    channelMonitor->subscriptionEvent(args);
}

class CACMonitorQueue :
    public std::tr1::enable_shared_from_this<CACMonitorQueue>
{
public:
    POINTER_DEFINITIONS(CACMonitorQueue);
private:
    size_t queueSize;
    bool isStarted;
    Mutex mutex;
    
    std::queue<MonitorElementPtr> monitorElementQueue;
public:
    CACMonitorQueue(
        int32 queueSize)
     : queueSize(queueSize),
       isStarted(false)
     {}
     ~CACMonitorQueue()
     {
     } 
     void start()
     {
         Lock guard(mutex);
         while(!monitorElementQueue.empty()) monitorElementQueue.pop();
         isStarted = true;
     }
     void stop()
     {
         Lock guard(mutex);
         while(!monitorElementQueue.empty()) monitorElementQueue.pop();
         isStarted = false;
     }

     bool event(
          const PVStructurePtr &pvStructure,
          const MonitorElementPtr & activeElement)
     {
         Lock guard(mutex);
         if(!isStarted) return false;
         if(monitorElementQueue.size()==queueSize) return false;
         PVStructure::shared_pointer pvs = 
              getPVDataCreate()->createPVStructure(pvStructure);
         MonitorElementPtr monitorElement(new MonitorElement(pvs));
         *(monitorElement->changedBitSet) = *(activeElement->changedBitSet);
         *(monitorElement->overrunBitSet) = *(activeElement->overrunBitSet);
         monitorElementQueue.push(monitorElement);
         return true;
     }
     MonitorElementPtr poll()
     {
          Lock guard(mutex);
          if(!isStarted) return MonitorElementPtr();
          if(monitorElementQueue.empty()) return MonitorElementPtr();
          MonitorElementPtr retval = monitorElementQueue.front();
          return retval;
     }
     void release(MonitorElementPtr const & monitorElement)
     {
         Lock guard(mutex);
         if(monitorElementQueue.empty()) {
              throw  std::runtime_error("client error calling release");
         }
         monitorElementQueue.pop();
     }
};

CAChannelMonitorPtr CAChannelMonitor::create(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelMonitorPtr(new CAChannelMonitor(channel, monitorRequester, pvRequest));
}

CAChannelMonitor::~CAChannelMonitor()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::~CAChannelMonitor() " << channel->getChannelName() << endl;
    }
    if(!isStarted) return;
    channel->attachContext();
    int result = ca_clear_subscription(eventID);
    if (result == ECA_NORMAL) return;
    string mess("CAChannelMonitor::~CAChannelMonitor() ");
    mess += ca_message(result);
    cerr << mess << endl;
}

size_t CAChannelMonitor::num_instances;

CAChannelMonitor::CAChannelMonitor(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    PVStructurePtr const & pvRequest) 
:
    channel(channel),
    monitorRequester(monitorRequester),
    pvRequest(pvRequest),
    isStarted(false),
    firstTime(true)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelMonitor::CAChannelMonitor() " << channel->getChannelName() << endl;
    }
}

void CAChannelMonitor::activate()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::activate " << channel->getChannelName() << endl;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    if(pvStructure) throw  std::runtime_error("CAChannelMonitor::activate() was called twice");
    getType = getDBRType(pvRequest, channel->getNativeType());
    pvStructure = createPVStructure(channel, getType, pvRequest);
    activeElement = MonitorElementPtr(new MonitorElement(pvStructure));
    pvCopy = PVCopy::create(
          createPVStructure(channel, getType, pvRequest),
          CreateRequest::create()->createRequest("field()"),
          "");
    int32 queueSize = 2;
    PVStructurePtr pvOptions = pvRequest->getSubField<PVStructure>("record._options");
    if (pvOptions) {
        PVStringPtr pvString = pvOptions->getSubField<PVString>("queueSize");
        if (pvString) {
            int size;
            std::stringstream ss;
            ss << pvString->get();
            ss >> size;
            if (size > 1) queueSize = size;
        }
    }
    monitorQueue = CACMonitorQueuePtr(new CACMonitorQueue(queueSize));
    channel->addChannelMonitor(shared_from_this());
    if(channel->getConnectionState()==Channel::CONNECTED) {
        EXCEPTION_GUARD(requester->monitorConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
    }
}

void CAChannelMonitor::channelCreated(const Status& status,Channel::shared_pointer const & c)
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::channelCreated " << channel->getChannelName() << endl;
    }
    firstTime = true;
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    chtype newType = getDBRType(pvRequest, channel->getNativeType());
    if(newType!=getType) {
        getType = getDBRType(pvRequest, channel->getNativeType());
        pvStructure = createPVStructure(channel, getType, pvRequest);
        activeElement = MonitorElementPtr(new MonitorElement(pvStructure));
        pvCopy = PVCopy::create(
          createPVStructure(channel, getType, pvRequest),
          CreateRequest::create()->createRequest("field()"),
          "");
        int32 queueSize = 2;
        PVStructurePtr pvOptions = pvRequest->getSubField<PVStructure>("record._options");
        if (pvOptions) {
            PVStringPtr pvString = pvOptions->getSubField<PVString>("queueSize");
            if (pvString) {
                int size;
                std::stringstream ss;
                ss << pvString->get();
                ss >> size;
                if (size > 1) queueSize = size;
            }
        }
        monitorQueue = CACMonitorQueuePtr(new CACMonitorQueue(queueSize));
    }
    EXCEPTION_GUARD(requester->monitorConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

void CAChannelMonitor::channelStateChange(
     Channel::shared_pointer const & channel,
     Channel::ConnectionState connectionState)
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::channelStateChange " << channel->getChannelName() << endl;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    if(connectionState==Channel::DISCONNECTED || connectionState==Channel::DESTROYED) {
        EXCEPTION_GUARD(requester->channelDisconnect(connectionState==Channel::DESTROYED);)
    }
}


void CAChannelMonitor::channelDisconnect(bool destroy)
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::channelDisconnect " << channel->getChannelName() << endl;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    EXCEPTION_GUARD(requester->channelDisconnect(destroy);)
}

void CAChannelMonitor::subscriptionEvent(struct event_handler_args &args)
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelMonitor::subscriptionEvent "
             << channel->getChannelName()
             << " firstTime " << (firstTime ? "true" : "false")
              << endl;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    if (args.status == ECA_NORMAL)
    {
        copyDBRtoPVStructure copyFunc = copyFuncTable[getType];
        if (copyFunc) {
            copyFunc(args.dbr, args.count, pvStructure);
            pvCopy->updateMasterSetBitSet(pvStructure,activeElement->changedBitSet);
            if(firstTime) {
               activeElement->changedBitSet->clear();
               activeElement->overrunBitSet->clear();
               activeElement->changedBitSet->set(0);
               firstTime = false;
            }
            if(monitorQueue->event(pvStructure,activeElement)) {
                 activeElement->changedBitSet->clear();
                 activeElement->overrunBitSet->clear();
            } else {
                *(activeElement->overrunBitSet) |= *(activeElement->changedBitSet);
            }
            
            // call monitorRequester even if queue is full
            requester->monitorEvent(shared_from_this());
        } else {
            std::cout << "no copy func implemented" << std::endl;
            
        }
    }
    else
    {
        string mess("CAChannelMonitor::subscriptionEvent ");
        mess += ca_message(args.status);
        throw  std::runtime_error(mess);
    }
}

epics::pvData::Status CAChannelMonitor::start()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::start " << channel->getChannelName() << endl;
    }
    Status status = Status::Ok;
    if(isStarted) {
        status = Status(Status::STATUSTYPE_WARNING,"already started");
        return status;
    }
    channel->attachContext();

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
    monitorQueue->start();
    int result = ca_create_subscription(getType,
         0,
         channel->getChannelID(), DBE_VALUE,
         ca_subscription_handler, this,
         &eventID);
    if (result == ECA_NORMAL)
    {
        isStarted = true;
        result = ca_flush_io();
    }
    if (result == ECA_NORMAL) return status;
    isStarted = false;
    string message(ca_message(result));
    return Status(Status::STATUSTYPE_ERROR,message);
}

epics::pvData::Status CAChannelMonitor::stop()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::stop " << channel->getChannelName() << endl;
    }
    Status status = Status::Ok;
    if(!isStarted) return Status(Status::STATUSTYPE_WARNING,"already stopped");
    channel->attachContext();
    int result = ca_clear_subscription(eventID);
    if (result == ECA_NORMAL)
    {
        isStarted = false;
        monitorQueue->stop();
        result = ca_flush_io();
    }
    if (result == ECA_NORMAL) return status;
    string mess("CAChannelMonitor::stop() ");
    mess += ca_message(result);
    return Status(Status::STATUSTYPE_ERROR,mess);
}


MonitorElementPtr CAChannelMonitor::poll()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelMonitor::poll " << channel->getChannelName() << endl;
    }
    return monitorQueue->poll();
}


void CAChannelMonitor::release(MonitorElementPtr const & monitorElement)
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelMonitor::release " << channel->getChannelName() << endl;
    }
    monitorQueue->release(monitorElement);
}

/* --------------- epics::pvData::ChannelRequest --------------- */

void CAChannelMonitor::cancel()
{
    // noop
}


}}}
