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
#include "stopMonitorThread.h"

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
    CAChannelPtr caChannel(
        new CAChannel(channelName, channelProvider, channelRequester));
    caChannel->activate(priority);
    return caChannel;
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

void CAChannel::connected()
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::connected " << channelName << endl;
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

    ChannelRequester::shared_pointer req(channelRequester.lock());
    if(req) {
        EXCEPTION_GUARD(req->channelStateChange(
             shared_from_this(), Channel::DISCONNECTED));
    }
}

size_t CAChannel::num_instances;

CAChannel::CAChannel(std::string const & channelName,
                     CAChannelProvider::shared_pointer const & channelProvider,
                     ChannelRequester::shared_pointer const & channelRequester) :
    channelName(channelName),
    channelProvider(channelProvider),
    channelRequester(channelRequester),
    channelID(0),
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
        cout << "CAChannel::~CAChannel() " << channelName 
             << " channelCreated " << (channelCreated ? "true" : "false")
             << endl;
    }
    {
        Lock lock(requestsMutex);
        if(!channelCreated) return;
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
    CAChannelProviderPtr provider(channelProvider.lock());
    if(provider) {
        std::tr1::static_pointer_cast<CAChannelProvider>(provider)->attachContext();
    }
    int result = ca_clear_channel(channelID);
    if (result == ECA_NORMAL) return;
    string mess("CAChannel::disconnectChannel() ");
    mess += ca_message(result);
    cerr << mess << endl;
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
    PVStructurePtr pvRequest(createRequest(""));
    DbdToPvPtr dbdToPv = DbdToPv::create(caChannel,pvRequest,getIO);
    PVStructurePtr pvStructure = dbdToPv->createPVStructure();
    epics::pvData::Structure::const_shared_pointer structure(pvStructure->getStructure());
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
        return;
    }
    string mess("CAChannel::attachContext provider does not exist ");
    mess += getChannelName();
    throw  std::runtime_error(mess);
}


size_t CAChannelGet::num_instances;

CAChannelGetPtr CAChannelGet::create(
    CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGet::create " << channel->getChannelName() << endl;
    }
    return CAChannelGetPtr(new CAChannelGet(channel, channelGetRequester, pvRequest));
}

CAChannelGet::CAChannelGet(CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
  :
    channel(channel),
    channelGetRequester(channelGetRequester),
    pvRequest(pvRequest)
{}

CAChannelGet::~CAChannelGet()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::~CAChannelGet() " <<  channel->getChannelName() << endl;
    }
}

void CAChannelGet::activate()
{
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelGet::activate " <<  channel->getChannelName() << endl;
    }
    dbdToPv = DbdToPv::create(channel,pvRequest,getIO);
    pvStructure = dbdToPv->createPVStructure();
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    EXCEPTION_GUARD(getRequester->channelGetConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

std::string CAChannelGet::getRequesterName() { return "CAChannelGet";}

namespace {

static void ca_get_handler(struct event_handler_args args)
{
    CAChannelGet *channelGet = static_cast<CAChannelGet*>(args.usr);
    channelGet->getDone(args);
}

} // namespace

void CAChannelGet::getDone(struct event_handler_args &args)
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelGet::getDone " 
            <<  channel->getChannelName() << endl;
    }
    
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    Status status = dbdToPv->getFromDBD(pvStructure,bitSet,args);
    EXCEPTION_GUARD(getRequester->getDone(status, shared_from_this(), pvStructure, bitSet));
}

void CAChannelGet::get()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelGet::get " <<  channel->getChannelName() << endl;
    }
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    channel->attachContext();
    bitSet->clear();
    int result = ca_array_get_callback(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), ca_get_handler, this);
    if (result == ECA_NORMAL)
    {
        result = ca_flush_io();
    }
    if (result != ECA_NORMAL)
    {
        string mess("CAChannelGet::get ");
        mess += channel->getChannelName() + " message " + ca_message(result);
        throw  std::runtime_error(mess);
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

size_t CAChannelPut::num_instances;

CAChannelPutPtr CAChannelPut::create(
    CAChannel::shared_pointer const & channel,
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::create " << channel->getChannelName() << endl;
    }
    return CAChannelPutPtr(new CAChannelPut(channel, channelPutRequester, pvRequest));
}

CAChannelPut::CAChannelPut(CAChannel::shared_pointer const & channel,
    ChannelPutRequester::shared_pointer const & channelPutRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
:
    channel(channel),
    channelPutRequester(channelPutRequester),
    pvRequest(pvRequest),
    block(false)
{}

CAChannelPut::~CAChannelPut()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelPut::~CAChannelPut() " << channel->getChannelName() << endl;
    }
}


void CAChannelPut::activate()
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::activate " << channel->getChannelName() << endl;
    }
    dbdToPv = DbdToPv::create(channel,pvRequest,putIO);
    pvStructure = dbdToPv->createPVStructure();
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    PVStringPtr pvString = pvRequest->getSubField<PVString>("record._options.block");
    if(pvString) {
        std::string val = pvString->get();
        if(val.compare("true")==0) block = true;
    }
    EXCEPTION_GUARD(putRequester->channelPutConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

std::string CAChannelPut::getRequesterName() { return "CAChannelPut";}


/* --------------- epics::pvAccess::ChannelPut --------------- */

namespace {

static void ca_put_get_handler(struct event_handler_args args)
{
    CAChannelPut *channelPut = static_cast<CAChannelPut*>(args.usr);
    channelPut->getDone(args);
}

} // namespace


void CAChannelPut::put(PVStructure::shared_pointer const & pvPutStructure,
                       BitSet::shared_pointer const & /*putBitSet*/)
{
    if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::put " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    Status status = dbdToPv->putToDBD(channel,pvPutStructure,block);
    EXCEPTION_GUARD(putRequester->putDone(status, shared_from_this()));
}


void CAChannelPut::getDone(struct event_handler_args &args)
{
     if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::getDone " << channel->getChannelName() << endl;
    }
    
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    Status status = dbdToPv->getFromDBD(pvStructure,bitSet,args);
    EXCEPTION_GUARD(putRequester->getDone(status, shared_from_this(), pvStructure, bitSet));
}


void CAChannelPut::get()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelPut::get " <<  channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    channel->attachContext();
    bitSet->clear();
    int result = ca_array_get_callback(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), ca_put_get_handler, this);
    if (result == ECA_NORMAL)
    {
        result = ca_flush_io();
    }
    if (result != ECA_NORMAL)
    {
        string mess("CAChannelPut::get ");
        mess += channel->getChannelName() + " message " +ca_message(result);
        throw  std::runtime_error(mess);
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
         if(!isStarted) return;
         if(monitorElementQueue.empty()) {
              string mess("CAChannelMonitor::release client error calling release ");
              throw  std::runtime_error(mess);
         }
         monitorElementQueue.pop();
     }
};

size_t CAChannelMonitor::num_instances;

CAChannelMonitorPtr CAChannelMonitor::create(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelMonitor::create " << channel->getChannelName() << endl;
    }
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
    stopMonitorThread(StopMonitorThread::get())
{}

CAChannelMonitor::~CAChannelMonitor()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::~CAChannelMonitor() "
            << channel->getChannelName()
            << " isStarted " << (isStarted ? "true" : "false")
            << endl;
    }
    if(isStarted)  stop();
    stopMonitorThread->addNoEventsCallback(&waitForNoEvents);
    waitForNoEvents.wait();
}

void CAChannelMonitor::activate()
{
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::activate " << channel->getChannelName() << endl;
    }
    dbdToPv = DbdToPv::create(channel,pvRequest,monitorIO);
    pvStructure = dbdToPv->createPVStructure();
    activeElement = MonitorElementPtr(new MonitorElement(pvStructure));
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
    EXCEPTION_GUARD(requester->monitorConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

std::string CAChannelMonitor::getRequesterName() { return "CAChannelMonitor";}

void CAChannelMonitor::subscriptionEvent(struct event_handler_args &args)
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelMonitor::subscriptionEvent "
             << channel->getChannelName() << endl;
    }
    if(!isStarted) return;
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    Status status = dbdToPv->getFromDBD(pvStructure,activeElement->changedBitSet,args);
    if(status.isOK())
    {
        if(monitorQueue->event(pvStructure,activeElement)) {
             activeElement->changedBitSet->clear();
             activeElement->overrunBitSet->clear();
        } else {
            *(activeElement->overrunBitSet) |= *(activeElement->changedBitSet);
        }
            
        // call monitorRequester even if queue is full
        requester->monitorEvent(shared_from_this());
    }
    else
    {
        string mess("CAChannelMonitor::subscriptionEvent ");
        mess += channel->getChannelName();
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
    monitorQueue->start();
    isStarted = true;
    int result = ca_create_subscription(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), DBE_VALUE,
         ca_subscription_handler, this,
         &eventID);
    if (result == ECA_NORMAL)
    {
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
        std::cout << "CAChannelMonitor::stop "
            << channel->getChannelName()
            << " isStarted " << (isStarted ? "true" : "false")
            << endl;
    }
    if(!isStarted) return Status(Status::STATUSTYPE_WARNING,"already stopped");
    isStarted = false;
    monitorQueue->stop();
    stopMonitorThread->callStop(eventID);
    eventID = NULL;
    return Status::Ok;
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
