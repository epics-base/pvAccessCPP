/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */


#include <epicsVersion.h>

#include <pv/standardField.h>
#include <pv/logger.h>
#include <pv/pvAccess.h>
#include "channelConnectThread.h"
#include "monitorEventThread.h"
#include "getDoneThread.h"
#include "putDoneThread.h"

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
        channel->connect(true);
    } else if (args.op == CA_OP_CONN_DOWN) {
        channel->connect(false);
    }
}

void CAChannel::connect(bool isConnected)
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::connect " << channelName << endl;
    }
    {
         Lock lock(requestsMutex);
         channelConnected = isConnected;
    }
    channelConnectThread->channelConnected(notifyChannelRequester);
}

void CAChannel::notifyClient()
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::notifyClient " << channelName << endl;
    }
    CAChannelProviderPtr provider(channelProvider.lock());
    if(!provider) return;
    bool isConnected = false;
    {
         Lock lock(requestsMutex);
         isConnected = channelConnected;
    }
    if(!isConnected) {
        ChannelRequester::shared_pointer req(channelRequester.lock());
        if(req) {
            EXCEPTION_GUARD(req->channelStateChange(
                shared_from_this(), Channel::DISCONNECTED));
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
    if(req) {
        EXCEPTION_GUARD(req->channelStateChange(
             shared_from_this(), Channel::CONNECTED));
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
    channelConnectThread(ChannelConnectThread::get())
{
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::CAChannel " << channelName << endl;
    }
}

void CAChannel::activate(short priority)
{
    ChannelRequester::shared_pointer req(channelRequester.lock());
    if(!req) return;
    if(DEBUG_LEVEL>0) {
          cout<< "CAChannel::activate " << channelName << endl;
    }
    notifyChannelRequester = NotifyChannelRequesterPtr(new NotifyChannelRequester());
    notifyChannelRequester->setChannel(shared_from_this());
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
    std::vector<CAChannelMonitorWPtr>::iterator it;
    for(it = monitorlist.begin(); it!=monitorlist.end(); ++it)
    {
         CAChannelMonitorPtr mon = (*it).lock();
         if(!mon) continue;
         mon->stop();
    }
    monitorlist.resize(0);
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
    CAChannelGetFieldPtr getField(
         new CAChannelGetField(shared_from_this(),requester,subField));
    {
         Lock lock(requestsMutex);
         if(getConnectionState()!=Channel::CONNECTED) {
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
    PVStructure::shared_pointer const & pvRequest)
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
    PVStructure::shared_pointer const & pvRequest)
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
    addMonitor(channelMonitor);
    return channelMonitor;
}

void CAChannel::addMonitor(CAChannelMonitorPtr const & monitor)
{
    std::vector<CAChannelMonitorWPtr>::iterator it;
    for(it = monitorlist.begin(); it!=monitorlist.end(); ++it)
    {
         CAChannelMonitorWPtr mon = *it;
         if(mon.lock()) continue;
         mon = monitor;
         return;
    }
    monitorlist.push_back(monitor);
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
    CAChannelPtr const &channel,
    GetFieldRequester::shared_pointer const & requester,std::string const & subField)
  : channel(channel),
    getFieldRequester(requester),
    subField(subField)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGetField::CAChannelGetField()\n";
    }
}

void CAChannelGetField::activate()
{
    CAChannelPtr chan(channel.lock());
    if(chan) callRequester(chan);
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
    Structure::const_shared_pointer structure(dbdToPv->getStructure());
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

CAChannelGetPtr CAChannelGet::create(
    CAChannel::shared_pointer const & channel,
    ChannelGetRequester::shared_pointer const & channelGetRequester,
    PVStructure::shared_pointer const & pvRequest)
{
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelGet::create " << channel->getChannelName() << endl;
    }
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
    getDoneThread(GetDoneThread::get())
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
    dbdToPv->getChoices(channel);
    pvStructure = dbdToPv->createPVStructure();
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    notifyGetRequester = NotifyGetRequesterPtr(new NotifyGetRequester());
    notifyGetRequester->setChannelGet(shared_from_this());
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
    getStatus = dbdToPv->getFromDBD(pvStructure,bitSet,args);
    getDoneThread->getDone(notifyGetRequester);
}

void CAChannelGet::notifyClient()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelGet::notifyClient " <<  channel->getChannelName() << endl;
    }
    ChannelGetRequester::shared_pointer getRequester(channelGetRequester.lock());
    if(!getRequester) return;
    EXCEPTION_GUARD(getRequester->getDone(getStatus, shared_from_this(), pvStructure, bitSet));
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
    if(DEBUG_LEVEL>0) {
        cout << "CAChannelPut::create " << channel->getChannelName() << endl;
    }
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
    putDoneThread(PutDoneThread::get())
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
    dbdToPv->getChoices(channel);
    pvStructure = dbdToPv->createPVStructure();
    bitSet = BitSetPtr(new BitSet(pvStructure->getStructure()->getNumberFields()));
    PVStringPtr pvString = pvRequest->getSubField<PVString>("record._options.block");
    if(pvString) {
        std::string val = pvString->get();
        if(val.compare("true")==0) block = true;
    }
    notifyPutRequester = NotifyPutRequesterPtr(new NotifyPutRequester());
    notifyPutRequester->setChannelPut(shared_from_this());
    EXCEPTION_GUARD(putRequester->channelPutConnect(Status::Ok, shared_from_this(),
                    pvStructure->getStructure()));
}

std::string CAChannelPut::getRequesterName() { return "CAChannelPut";}


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

} // namespace


void CAChannelPut::put(PVStructure::shared_pointer const & pvPutStructure,
                       BitSet::shared_pointer const & /*putBitSet*/)
{
    if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::put " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    {
       Lock lock(mutex);    
       isPut = true;
    }
    putStatus = dbdToPv->putToDBD(channel,pvPutStructure,block,&ca_put_handler,this);
    if(!block || !putStatus.isOK()) {
        EXCEPTION_GUARD(putRequester->putDone(putStatus, shared_from_this()));
    }
}


void CAChannelPut::putDone(struct event_handler_args &args)
{
     if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::putDone " << channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    if(args.status!=ECA_NORMAL)
    {
        putStatus = Status(Status::STATUSTYPE_ERROR, string(ca_message(args.status)));
    } else {
        putStatus = Status::Ok;
    }
    putDoneThread->putDone(notifyPutRequester);
}

void CAChannelPut::getDone(struct event_handler_args &args)
{
     if(DEBUG_LEVEL>1) {
        cout << "CAChannelPut::getDone " << channel->getChannelName() << endl;
    }
    
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    getStatus = dbdToPv->getFromDBD(pvStructure,bitSet,args);
    putDoneThread->putDone(notifyPutRequester);
}

void CAChannelPut::notifyClient()
{
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    if(isPut) {
        EXCEPTION_GUARD(putRequester->putDone(putStatus, shared_from_this()));
    } else {
        EXCEPTION_GUARD(putRequester->getDone(getStatus, shared_from_this(), pvStructure, bitSet));
    }
}


void CAChannelPut::get()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelPut::get " <<  channel->getChannelName() << endl;
    }
    ChannelPutRequester::shared_pointer putRequester(channelPutRequester.lock());
    if(!putRequester) return;
    {
       Lock lock(mutex);    
       isPut = false;
    }

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

CAChannelMonitorPtr CAChannelMonitor::create(
    CAChannel::shared_pointer const & channel,
    MonitorRequester::shared_pointer const & monitorRequester,
    PVStructure::shared_pointer const & pvRequest)
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
    monitorEventThread(MonitorEventThread::get()),
    pevid(NULL),
    eventMask(DBE_VALUE | DBE_ALARM)
{}

CAChannelMonitor::~CAChannelMonitor()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::~CAChannelMonitor() "
            << channel->getChannelName()
            << " isStarted " << (isStarted ? "true" : "false")
            << endl;
    }
    stop();
}

void CAChannelMonitor::activate()
{
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::activate " << channel->getChannelName() << endl;
    }
    dbdToPv = DbdToPv::create(channel,pvRequest,monitorIO);
    dbdToPv->getChoices(channel);
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
        pvString = pvOptions->getSubField<PVString>("DBE");
        if(pvString) {
            std::string value(pvString->get());
            eventMask = 0;
            if(value.find("VALUE")!=std::string::npos) eventMask|=DBE_VALUE;
            if(value.find("ARCHIVE")!=std::string::npos) eventMask|=DBE_ARCHIVE;
            if(value.find("ALARM")!=std::string::npos) eventMask|=DBE_ALARM;
            if(value.find("PROPERTY")!=std::string::npos) eventMask|=DBE_PROPERTY;
        }
    }
    notifyMonitorRequester = NotifyMonitorRequesterPtr(new NotifyMonitorRequester());
    notifyMonitorRequester->setChannelMonitor(shared_from_this());
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
    {
       Lock lock(mutex);    
       if(!isStarted) return;
    }
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
        monitorEventThread->event(notifyMonitorRequester);
    }
    else
    {
        string mess("CAChannelMonitor::subscriptionEvent ");
        mess += channel->getChannelName();
        mess += ca_message(args.status);
        throw  std::runtime_error(mess);
    }
}


void CAChannelMonitor::notifyClient()
{
    {
         Lock lock(mutex);
         if(!isStarted) return;
    }
    MonitorRequester::shared_pointer requester(monitorRequester.lock());
    if(!requester) return;
    requester->monitorEvent(shared_from_this());
}

Status CAChannelMonitor::start()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::start " << channel->getChannelName() << endl;
    }
    Status status = Status::Ok;
    {
        Lock lock(mutex);
        if(isStarted) {
            status = Status(Status::STATUSTYPE_WARNING,"already started");
            return status;
        }
        isStarted = true;
        monitorQueue->start();
    }
    channel->attachContext();
    int result = ca_create_subscription(dbdToPv->getRequestType(),
         0,
         channel->getChannelID(), eventMask,
         ca_subscription_handler, this,
         &pevid);
    if (result == ECA_NORMAL)
    {
        result = ca_flush_io();
    }
    if (result == ECA_NORMAL) return status;
    isStarted = false;
    string message(ca_message(result));
    return Status(Status::STATUSTYPE_ERROR,message);
}

Status CAChannelMonitor::stop()
{
    if(DEBUG_LEVEL>0) {
        std::cout << "CAChannelMonitor::stop "
            << channel->getChannelName()
            << " isStarted " << (isStarted ? "true" : "false")
            << endl;
    }
    {
         Lock lock(mutex);
         if(!isStarted) return Status(Status::STATUSTYPE_WARNING,"already stopped");
         isStarted = false;     
    }
    monitorQueue->stop();
    int result = ca_clear_subscription(pevid);
    if(result==ECA_NORMAL) return Status::Ok;
    return Status(Status::STATUSTYPE_ERROR,string(ca_message(result)));
}


MonitorElementPtr CAChannelMonitor::poll()
{
    if(DEBUG_LEVEL>1) {
        std::cout << "CAChannelMonitor::poll " << channel->getChannelName() << endl;
    }
    {
         Lock lock(mutex);
         if(!isStarted) return MonitorElementPtr();
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

/* --------------- ChannelRequest --------------- */

void CAChannelMonitor::cancel()
{
    // noop
}


}}}
