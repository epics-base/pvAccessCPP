/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#include <epicsMutex.h>
#include <epicsGuard.h>     // Needed for 3.15 builds
#include <pv/standardField.h>
#include <pv/pvAccess.h>

#define epicsExportSharedSymbols
#include "caChannel.h"

using namespace epics::pvData;
using std::string;
using std::cout;
using std::cerr;
using std::endl;

namespace epics {
namespace pvAccess {
namespace ca {


CAChannel::shared_pointer CAChannel::create(CAChannelProvider::shared_pointer const & channelProvider,
        std::string const & channelName,
        short priority,
        ChannelRequester::shared_pointer const & channelRequester)
{
    CAChannelPtr caChannel(
        new CAChannel(channelName, channelProvider, channelRequester));
    caChannel->activate(priority);
    return caChannel;
}

extern "C" {
static void ca_connection_handler(struct connection_handler_args args)
{
    CAChannel *channel = static_cast<CAChannel*>(ca_puser(args.chid));

    channel->connect(args.op == CA_OP_CONN_UP);
}
}

void CAChannel::connect(bool isConnected)
{
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        channelConnected = isConnected;
    }
    CAChannelProviderPtr provider(channelProvider.lock());
    if (!provider) return;
    provider->notifyConnection(connectNotification);
}

void CAChannel::notifyResult(NotificationPtr const &notificationPtr) {
    CAChannelProviderPtr provider(channelProvider.lock());
    if (!provider) return;
    provider->notifyResult(notificationPtr);
}

void CAChannel::notifyClient()
{
    CAChannelProviderPtr provider(channelProvider.lock());
    if (!provider) return;
    bool isConnected = false;
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        isConnected = channelConnected;
    }
    if (!isConnected) {
        ChannelRequester::shared_pointer req(channelRequester.lock());
        if (req) {
            EXCEPTION_GUARD(req->channelStateChange(shared_from_this(),
                Channel::DISCONNECTED));
        }
        return;
    }
    while(!getFieldQueue.empty()) {
        getFieldQueue.front()->activate();
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
        CAChannelMonitorPtr monitor(monitorQueue.front());
        monitor->activate();
        addMonitor(monitor);
        monitorQueue.pop();
    }
    ChannelRequester::shared_pointer req(channelRequester.lock());
    if (req) {
        EXCEPTION_GUARD(req->channelStateChange(shared_from_this(),
            Channel::CONNECTED));
    }
}


CAChannel::CAChannel(std::string const & channelName,
                     CAChannelProvider::shared_pointer const & channelProvider,
                     ChannelRequester::shared_pointer const & channelRequester) :
    channelName(channelName),
    channelProvider(channelProvider),
    channelRequester(channelRequester),
    channelID(0),
    channelCreated(false),
    channelConnected(false),
    connectNotification(new Notification()),
    ca_context(channelProvider->caContext())
{
    if (channelName.empty())
        throw std::invalid_argument("Channel name cannot be empty");
}

void CAChannel::activate(short priority)
{
    ChannelRequester::shared_pointer req(channelRequester.lock());
    if (!req) return;
    connectNotification->setClient(shared_from_this());
    Attach to(ca_context);
    int result = ca_create_channel(channelName.c_str(),
        ca_connection_handler, this,
        priority, // TODO mapping
        &channelID);
    Status errorStatus;
    if (result == ECA_NORMAL) {
        epicsGuard<epicsMutex> G(requestsMutex);
        channelCreated = true;      // Set before addChannel()
        CAChannelProviderPtr provider(channelProvider.lock());
        if (provider)
            provider->addChannel(*this);
    }
    else {
        errorStatus = Status::error(ca_message(result));
    }
    EXCEPTION_GUARD(req->channelCreated(errorStatus, shared_from_this()));
}

CAChannel::~CAChannel()
{
    disconnectChannel();
}

void CAChannel::disconnectChannel()
{
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        if (!channelCreated) return;
        CAChannelProviderPtr provider(channelProvider.lock());
        if (provider)
            provider->delChannel(*this);
        channelCreated = false;     // Clear only after delChannel()
    }
    std::vector<CAChannelMonitorWPtr>::iterator it;
    for (it = monitorlist.begin(); it!=monitorlist.end(); ++it) {
        CAChannelMonitorPtr mon = it->lock();
        if (!mon) continue;
        mon->stop();
    }
    monitorlist.resize(0);
    /* Clear CA Channel */
    Attach to(ca_context);
    int result = ca_clear_channel(channelID);
    if (result != ECA_NORMAL) {
        string mess("CAChannel::disconnectChannel() ");
        mess += ca_message(result);
        cerr << mess << endl;
    }
}

chid CAChannel::getChannelID()
{
    return channelID;
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
    CAChannelGetFieldPtr getField(
        new CAChannelGetField(shared_from_this(),requester,subField));
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        if (getConnectionState()!=Channel::CONNECTED) {
            getFieldQueue.push(getField);
            return;
        }
     }
     getField->callRequester(shared_from_this());
}


AccessRights CAChannel::getAccessRights(PVField::shared_pointer const & /*pvField*/)
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
    PVStructure::shared_pointer const & pvRequest)
{
    CAChannelGetPtr channelGet =
        CAChannelGet::create(shared_from_this(), channelGetRequester, pvRequest);
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        if (getConnectionState()!=Channel::CONNECTED) {
            getQueue.push(channelGet);
            return channelGet;
        }
    }
    channelGet->activate();
    return channelGet;
}


ChannelPut::shared_pointer CAChannel::createChannelPut(
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    PVStructure::shared_pointer const & pvRequest)
{
    CAChannelPutPtr channelPut =
        CAChannelPut::create(shared_from_this(), channelPutRequester, pvRequest);
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        if (getConnectionState()!=Channel::CONNECTED) {
            putQueue.push(channelPut);
            return channelPut;
        }
    }
    channelPut->activate();
    return channelPut;
}


Monitor::shared_pointer CAChannel::createMonitor(
    MonitorRequester::shared_pointer const & monitorRequester,
    PVStructure::shared_pointer const & pvRequest)
{
    CAChannelMonitorPtr channelMonitor =
        CAChannelMonitor::create(shared_from_this(), monitorRequester, pvRequest);
    {
        epicsGuard<epicsMutex> G(requestsMutex);
        if (getConnectionState()!=Channel::CONNECTED) {
            monitorQueue.push(channelMonitor);
            return channelMonitor;
        }
    }
    channelMonitor->activate();
    addMonitor(channelMonitor);
    return channelMonitor;
}

void CAChannel::addMonitor(CAChannelMonitorPtr const & monitor)
{
    std::vector<CAChannelMonitorWPtr>::iterator it;
    for (it = monitorlist.begin(); it!=monitorlist.end(); ++it) {
        if (it->expired()) {
            *it = monitor;
            return;
        }
    }
    monitorlist.push_back(monitor);
}

void CAChannel::printInfo(std::ostream& out)
{
    out << "CHANNEL  : " << getChannelName() << std::endl;

    ConnectionState state = getConnectionState();
    out << "STATE    : " << ConnectionStateNames[state] << std::endl;
    if (state == CONNECTED) {
        out << "ADDRESS  : " << getRemoteAddress() << std::endl;
        //out << "RIGHTS   : " << getAccessRights() << std::endl;
    }
}


CAChannelGetField::CAChannelGetField(
    CAChannelPtr const &channel,
    GetFieldRequester::shared_pointer const & requester,
    std::string const & subField)
  : channel(channel),
    getFieldRequester(requester),
    subField(subField)
{
}

void CAChannelGetField::activate()
{
    CAChannelPtr chan(channel.lock());
    if (chan) callRequester(chan);
}

CAChannelGetField::~CAChannelGetField()
{
}

void CAChannelGetField::callRequester(CAChannelPtr const & caChannel)
{
    GetFieldRequester::shared_pointer requester(getFieldRequester.lock());
    if (!requester) return;
    PVStructurePtr pvRequest(createRequest(""));
    DbdToPvPtr dbdToPv = DbdToPv::create(caChannel,pvRequest,getIO);
    Structure::const_shared_pointer structure(dbdToPv->getStructure());
    Field::const_shared_pointer field =
        subField.empty() ?
        std::tr1::static_pointer_cast<const Field>(structure) :
        structure->getField(subField);

    if (field) {
        EXCEPTION_GUARD(requester->getDone(Status::Ok, field));
    }
    else {
        Status errorStatus(Status::STATUSTYPE_ERROR, "field '" + subField + "' not found");
        EXCEPTION_GUARD(requester->getDone(errorStatus, FieldConstPtr()));
    }
}

/* ---------------------------------------------------------- */

CAChannelGetPtr CAChannelGet::create(
    CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelGetPtr(new CAChannelGet(channel, channelGetRequester, pvRequest));
}

CAChannelGet::CAChannelGet(CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    PVStructure::shared_pointer const & pvRequest)
  :
    channel(channel),
    channelGetRequester(channelGetRequester),
    pvRequest(pvRequest),
    getStatus(Status::Ok),
    getNotification(new Notification()),
    ca_context(channel->caContext())
{}

CAChannelGet::~CAChannelGet()
{
}

void CAChannelGet::activate()
{
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if (!getRequester) return;
    dbdToPv = DbdToPv::create(channel,pvRequest,getIO);
    dbdToPv->getChoices(channel);
    pvStructure = dbdToPv->createPVStructure();
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    getNotification->setClient(shared_from_this());
    EXCEPTION_GUARD(getRequester->channelGetConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}



std::string CAChannelGet::getRequesterName()
{
    return "CAChannelGet";
}

extern "C" {

static void ca_get_handler(struct event_handler_args args)
{
    CAChannelGet *channelGet = static_cast<CAChannelGet*>(args.usr);
    channelGet->getDone(args);
}

} // extern "C"

void CAChannelGet::getDone(struct event_handler_args &args)
{
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if (!getRequester) return;
    getStatus = dbdToPv->getFromDBD(pvStructure,bitSet,args);
    channel->notifyResult(getNotification);
}

void CAChannelGet::notifyClient()
{
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if (!getRequester) return;
    EXCEPTION_GUARD(getRequester->getDone(getStatus, shared_from_this(), pvStructure, bitSet));
}

void CAChannelGet::get()
{
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if (!getRequester) return;
    bitSet->clear();
    Attach to(ca_context);
    int result = ca_array_get_callback(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), ca_get_handler, this);
    if (result == ECA_NORMAL)
        result = ca_flush_io();
    if (result != ECA_NORMAL) {
        string mess("CAChannelGet::get ");
        mess += channel->getChannelName() + " message " + ca_message(result);
        getStatus = Status(Status::STATUSTYPE_ERROR,mess);
        notifyClient();
    }
}

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
    PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelPutPtr(new CAChannelPut(channel, channelPutRequester, pvRequest));
}

CAChannelPut::CAChannelPut(CAChannel::shared_pointer const & channel,
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    PVStructure::shared_pointer const & pvRequest)
:
    channel(channel),
    channelPutRequester(channelPutRequester),
    pvRequest(pvRequest),
    block(false),
    isPut(false),
    getStatus(Status::Ok),
    putStatus(Status::Ok),
    putNotification(new Notification()),
    ca_context(channel->caContext())
{}

CAChannelPut::~CAChannelPut()
{
}


void CAChannelPut::activate()
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if (!putRequester) return;
    dbdToPv = DbdToPv::create(channel,pvRequest,putIO);
    dbdToPv->getChoices(channel);
    pvStructure = dbdToPv->createPVStructure();
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    PVStringPtr pvString = pvRequest->getSubField<PVString>("record._options.block");
    if (pvString) {
        std::string val = pvString->get();
        if (val.compare("true")==0)
            block = true;
    }
    putNotification->setClient(shared_from_this());
    EXCEPTION_GUARD(putRequester->channelPutConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

std::string CAChannelPut::getRequesterName()
{
    return "CAChannelPut";
}


/* --------------- epics::pvAccess::ChannelPut --------------- */

extern "C" {

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

} // extern "C"


void CAChannelPut::put(PVStructure::shared_pointer const & pvPutStructure,
                       BitSet::shared_pointer const & /*putBitSet*/)
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if (!putRequester) return;
    {
       epicsGuard<epicsMutex> G(mutex);
       isPut = true;
    }
    putStatus = dbdToPv->putToDBD(channel,pvPutStructure,block,&ca_put_handler,this);
    if (!block || !putStatus.isOK()) {
        EXCEPTION_GUARD(putRequester->putDone(putStatus, shared_from_this()));
    }
}


void CAChannelPut::putDone(struct event_handler_args &args)
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if (!putRequester) return;
    if (args.status!=ECA_NORMAL) {
        putStatus = Status(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
    }
    else {
        putStatus = Status::Ok;
    }
    channel->notifyResult(putNotification);
}

void CAChannelPut::getDone(struct event_handler_args &args)
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if (!putRequester) return;
    getStatus = dbdToPv->getFromDBD(pvStructure,bitSet,args);
    channel->notifyResult(putNotification);
}

void CAChannelPut::notifyClient()
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if (!putRequester) return;
    if (isPut) {
        EXCEPTION_GUARD(putRequester->putDone(putStatus, shared_from_this()));
    }
    else {
        EXCEPTION_GUARD(putRequester->getDone(getStatus, shared_from_this(), pvStructure, bitSet));
    }
}


void CAChannelPut::get()
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if (!putRequester) return;
    {
       epicsGuard<epicsMutex> G(mutex);
       isPut = false;
    }

    bitSet->clear();
    Attach to(ca_context);
    int result = ca_array_get_callback(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), ca_put_get_handler, this);
    if (result == ECA_NORMAL)
        result = ca_flush_io();
    if (result != ECA_NORMAL) {
        string mess("CAChannelPut::get ");
        mess += channel->getChannelName() + " message " +ca_message(result);
        Status status(Status::STATUSTYPE_ERROR,mess);
        EXCEPTION_GUARD(putRequester->getDone(status, shared_from_this(), pvStructure, bitSet));
    }
}


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
    epicsMutex mutex;

    std::queue<MonitorElementPtr> monitorElementQueue;
public:
    explicit CACMonitorQueue(int32 queueSize)
        : queueSize(queueSize),
          isStarted(false)
    {
    }
    ~CACMonitorQueue()
    {
    }
    void start()
    {
        epicsGuard<epicsMutex> G(mutex);
        while (!monitorElementQueue.empty())
            monitorElementQueue.pop();
        isStarted = true;
    }
    void stop()
    {
        epicsGuard<epicsMutex> G(mutex);
        while (!monitorElementQueue.empty())
            monitorElementQueue.pop();
        isStarted = false;
    }

    bool event(
        const PVStructurePtr &pvStructure,
        const MonitorElementPtr &activeElement)
    {
        epicsGuard<epicsMutex> G(mutex);
        if (!isStarted)
            return false;
        if (monitorElementQueue.size() == queueSize)
            return false;
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
        epicsGuard<epicsMutex> G(mutex);
        if (!isStarted)
            return MonitorElementPtr();
        if (monitorElementQueue.empty())
            return MonitorElementPtr();
        MonitorElementPtr retval = monitorElementQueue.front();
        return retval;
    }
    void release(MonitorElementPtr const &monitorElement)
    {
        epicsGuard<epicsMutex> G(mutex);
        if (!isStarted)
            return;
        if (monitorElementQueue.empty())
        {
            string mess("CAChannelMonitor::release client error calling release ");
            throw std::runtime_error(mess);
        }
        monitorElementQueue.pop();
    }
};

CAChannelMonitorPtr CAChannelMonitor::create(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    PVStructure::shared_pointer const & pvRequest)
{
    return CAChannelMonitorPtr(new CAChannelMonitor(channel, monitorRequester, pvRequest));
}

CAChannelMonitor::CAChannelMonitor(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    PVStructurePtr const & pvRequest)
:
    channel(channel),
    monitorRequester(monitorRequester),
    pvRequest(pvRequest),
    isStarted(false),
    pevid(NULL),
    eventMask(DBE_VALUE | DBE_ALARM),
    eventNotification(new Notification()),
    ca_context(channel->caContext())
{}

CAChannelMonitor::~CAChannelMonitor()
{
    stop();
}

void CAChannelMonitor::activate()
{
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if (!requester) return;
    dbdToPv = DbdToPv::create(channel,pvRequest,monitorIO);
    dbdToPv->getChoices(channel);
    pvStructure = dbdToPv->createPVStructure();
    activeElement = MonitorElementPtr(new MonitorElement(pvStructure));
    int32 queueSize = 2;
    PVStructurePtr pvOptions = pvRequest->getSubField<PVStructure>("record._options");
    if (pvOptions) {
        PVStringPtr pvString = pvOptions->getSubField<PVString>("queueSize");
        if (pvString) {
            int size=0;
            std::stringstream ss;
            ss << pvString->get();
            ss >> size;
            if (size > 1) queueSize = size;
        }
        pvString = pvOptions->getSubField<PVString>("DBE");
        if (pvString) {
            std::string value(pvString->get());
            eventMask = 0;
            if (value.find("VALUE")!=std::string::npos) eventMask|=DBE_VALUE;
            if (value.find("ARCHIVE")!=std::string::npos) eventMask|=DBE_ARCHIVE;
            if (value.find("ALARM")!=std::string::npos) eventMask|=DBE_ALARM;
            if (value.find("PROPERTY")!=std::string::npos) eventMask|=DBE_PROPERTY;
        }
    }
    eventNotification->setClient(shared_from_this());
    monitorQueue = CACMonitorQueuePtr(new CACMonitorQueue(queueSize));
    EXCEPTION_GUARD(requester->monitorConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

std::string CAChannelMonitor::getRequesterName()
{
    return "CAChannelMonitor";
}

void CAChannelMonitor::subscriptionEvent(struct event_handler_args &args)
{
    {
       epicsGuard<epicsMutex> G(mutex);
       if (!isStarted) return;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if (!requester) return;
    Status status = dbdToPv->getFromDBD(pvStructure,activeElement->changedBitSet,args);
    if (status.isOK()) {
        if (monitorQueue->event(pvStructure,activeElement)) {
             activeElement->changedBitSet->clear();
             activeElement->overrunBitSet->clear();
        }
        else {
            *(activeElement->overrunBitSet) |= *(activeElement->changedBitSet);
        }
        channel->notifyResult(eventNotification);
    }
    else {
        string mess("CAChannelMonitor::subscriptionEvent ");
        mess += channel->getChannelName();
        mess += ca_message(args.status);
        throw std::runtime_error(mess);
    }
}


void CAChannelMonitor::notifyClient()
{
    {
         epicsGuard<epicsMutex> G(mutex);
         if(!isStarted) return;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if (!requester) return;
    requester->monitorEvent(shared_from_this());
}

Status CAChannelMonitor::start()
{
    {
        epicsGuard<epicsMutex> G(mutex);
        if (isStarted)
            return Status(Status::STATUSTYPE_WARNING, "already started");
        isStarted = true;
        monitorQueue->start();
    }
    Attach to(ca_context);
    int result = ca_create_subscription(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), eventMask,
         ca_subscription_handler, this,
         &pevid);
    if (result == ECA_NORMAL)
        result = ca_flush_io();
    if (result == ECA_NORMAL)
        return Status::Ok;
    {
        epicsGuard<epicsMutex> G(mutex);
        isStarted = false;
    }
    return Status(Status::STATUSTYPE_ERROR, string(ca_message(result)));
}

Status CAChannelMonitor::stop()
{
    {
        epicsGuard<epicsMutex> G(mutex);
        if (!isStarted)
            return Status(Status::STATUSTYPE_WARNING, "already stopped");
        isStarted = false;
    }
    monitorQueue->stop();
    // Attach to(ca_context); -- Not required!
    int result = ca_clear_subscription(pevid);
    if (result==ECA_NORMAL)
        return Status::Ok;
    return Status(Status::STATUSTYPE_ERROR, string(ca_message(result)));
}

MonitorElementPtr CAChannelMonitor::poll()
{
    {
        epicsGuard<epicsMutex> G(mutex);
        if (!isStarted) return MonitorElementPtr();
    }
    return monitorQueue->poll();
}

void CAChannelMonitor::release(MonitorElementPtr const & monitorElement)
{
    monitorQueue->release(monitorElement);
}

void CAChannelMonitor::cancel()
{
}


}}}
