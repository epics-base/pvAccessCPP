/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <stdexcept>
#include <vector>
#include <queue>
#include <utility>

#define epicsExportSharedSymbols
#include <pv/pipelineServer.h>
#include <pv/wildcard.h>

using namespace epics::pvData;
using namespace std;

namespace epics {
namespace pvAccess {

class ChannelPipelineMonitorImpl :
    public PipelineMonitor,
    public PipelineControl,
    public std::tr1::enable_shared_from_this<ChannelPipelineMonitorImpl>
{
private:

    typedef vector<MonitorElement::shared_pointer> FreeElementQueue;
    typedef queue<MonitorElement::shared_pointer> MonitorElementQueue;

    Channel::shared_pointer m_channel;
    MonitorRequester::shared_pointer m_monitorRequester;
    PipelineSession::shared_pointer m_pipelineSession;

    size_t m_queueSize;

    FreeElementQueue m_freeQueue;
    MonitorElementQueue m_monitorQueue;

    Mutex m_freeQueueLock;
    Mutex m_monitorQueueLock;

    bool m_active;
    MonitorElement::shared_pointer m_nullMonitorElement;

    size_t m_requestedCount;

    bool m_pipeline;

    bool m_done;

    bool m_unlistenReported;

public:
    ChannelPipelineMonitorImpl(
        Channel::shared_pointer const & channel,
        MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest,
        PipelineService::shared_pointer const & pipelineService) :
        m_channel(channel),
        m_monitorRequester(monitorRequester),
        m_queueSize(2),
        m_freeQueueLock(),
        m_monitorQueueLock(),
        m_active(false),
        m_requestedCount(0),
        m_pipeline(false),
        m_done(false),
        m_unlistenReported(false)
    {

        m_pipelineSession = pipelineService->createPipeline(pvRequest);

        // extract queueSize and pipeline parameter
        PVStructurePtr pvOptions = pvRequest->getSubField<PVStructure>("record._options");
        if (pvOptions) {
            PVStringPtr pvString = pvOptions->getSubField<PVString>("queueSize");
            if (pvString) {
                int32 size;
                std::stringstream ss;
                ss << pvString->get();
                ss >> size;
                if (size > 1)
                    m_queueSize = static_cast<size_t>(size);
            }
            pvString = pvOptions->getSubField<PVString>("pipeline");
            if (pvString)
                m_pipeline = (pvString->get() == "true");
        }

        // server queue size must be >= client queue size
        size_t minQueueSize = m_pipelineSession->getMinQueueSize();
        if (m_queueSize < minQueueSize)
            m_queueSize = minQueueSize;

        Structure::const_shared_pointer structure = m_pipelineSession->getStructure();

        // create free elements
        {
            Lock guard(m_freeQueueLock);
            m_freeQueue.reserve(m_queueSize);
            for (size_t i = 0; i < m_queueSize; i++)
            {
                PVStructure::shared_pointer pvStructure = getPVDataCreate()->createPVStructure(structure);
                MonitorElement::shared_pointer monitorElement(new MonitorElement(pvStructure));
                // we always send all
                monitorElement->changedBitSet->set(0);
                m_freeQueue.push_back(monitorElement);
            }
        }
    }

    PipelineSession::shared_pointer getPipelineSession() const {
        return m_pipelineSession;
    }

    bool isPipelineEnabled() const {
        return m_pipeline;
    }

    virtual ~ChannelPipelineMonitorImpl()
    {
        destroy();
    }

    virtual Status start()
    {
        bool notify = false;
        {
            Lock guard(m_monitorQueueLock);

            // already started
            if (m_active)
                return Status::Ok;
            m_active = true;

            notify = (m_monitorQueue.size() != 0);
        }

        if (notify)
        {
            Monitor::shared_pointer thisPtr = shared_from_this();
            m_monitorRequester->monitorEvent(thisPtr);
        }

        return Status::Ok;
    }

    virtual Status stop()
    {
        Lock guard(m_monitorQueueLock);
        m_active = false;
        return Status::Ok;
    }

    // get next free element
    virtual MonitorElement::shared_pointer poll()
    {
        Lock guard(m_monitorQueueLock);

        // do not give send more elements than m_requestedCount
        // even if m_monitorQueue is not empty
        bool emptyQueue = m_monitorQueue.empty();
        if (emptyQueue || m_requestedCount == 0 || !m_active)
        {
            // report "unlisten" event if queue empty and done, release lock first
            if (!m_unlistenReported && m_done && emptyQueue)
            {
                m_unlistenReported = true;
                guard.unlock();
                m_monitorRequester->unlisten(shared_from_this());
            }

            return m_nullMonitorElement;
        }

        MonitorElement::shared_pointer element = m_monitorQueue.front();
        m_monitorQueue.pop();

        m_requestedCount--;

        return element;
    }

    virtual void release(MonitorElement::shared_pointer const & monitorElement)
    {
        Lock guard(m_freeQueueLock);
        m_freeQueue.push_back(monitorElement);
    }

    virtual void reportRemoteQueueStatus(int32 freeElements)
    {
        // TODO check
        size_t count = static_cast<size_t>(freeElements);

        //std::cout << "reportRemoteQueueStatus(" << count << ')' << std::endl;

        bool notify = false;
        {
            Lock guard(m_monitorQueueLock);
            m_requestedCount += count;
            notify = m_active && (m_monitorQueue.size() != 0);
        }

        // notify
        // TODO too many notify calls?
        if (notify)
        {
            Monitor::shared_pointer thisPtr = shared_from_this();
            m_monitorRequester->monitorEvent(thisPtr);
        }

        m_pipelineSession->request(shared_from_this(), count);
    }

    virtual void destroy()
    {
        bool notifyCancel = false;

        {
            Lock guard(m_monitorQueueLock);
            m_active = false;
            notifyCancel = !m_done;
            m_done = true;
        }

        if (notifyCancel)
            m_pipelineSession->cancel();
    }

    virtual void lock()
    {
        // noop
    }

    virtual void unlock()
    {
        // noop
    }

    virtual size_t getFreeElementCount() {
        Lock guard(m_freeQueueLock);
        return m_freeQueue.size();
    }

    virtual size_t getRequestedCount() {
        // TODO consider using atomic ops
        Lock guard(m_monitorQueueLock);
        return m_requestedCount;
    }

    virtual MonitorElement::shared_pointer getFreeElement() {
        Lock guard(m_freeQueueLock);
        if (m_freeQueue.empty())
            return m_nullMonitorElement;

        MonitorElement::shared_pointer freeElement = m_freeQueue.back();
        m_freeQueue.pop_back();

        return freeElement;
    }

    virtual void putElement(MonitorElement::shared_pointer const & element) {

        bool notify = false;
        {
            Lock guard(m_monitorQueueLock);
            if (m_done)
                return;
            // throw std::logic_error("putElement called after done");

            m_monitorQueue.push(element);
            // TODO there is way to much of notification, per each putElement
            notify = (m_requestedCount != 0);
        }

        // notify
        if (notify)
        {
            Monitor::shared_pointer thisPtr = shared_from_this();
            m_monitorRequester->monitorEvent(thisPtr);
        }
    }

    virtual void done() {
        Lock guard(m_monitorQueueLock);
        m_done = true;

        bool report = !m_unlistenReported && m_monitorQueue.empty();
        if (report)
            m_unlistenReported = true;

        guard.unlock();

        if (report)
            m_monitorRequester->unlisten(shared_from_this());
    }

};


class PipelineChannel :
    public Channel,
    public std::tr1::enable_shared_from_this<PipelineChannel>
{
private:

    static Status notSupportedStatus;
    static Status destroyedStatus;

    AtomicBoolean m_destroyed;

    ChannelProvider::shared_pointer m_provider;
    string m_channelName;
    ChannelRequester::shared_pointer m_channelRequester;

    PipelineService::shared_pointer m_pipelineService;

public:
    POINTER_DEFINITIONS(PipelineChannel);

    PipelineChannel(
        ChannelProvider::shared_pointer const & provider,
        string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        PipelineService::shared_pointer const & pipelineService) :
        m_provider(provider),
        m_channelName(channelName),
        m_channelRequester(channelRequester),
        m_pipelineService(pipelineService)
    {
    }

    virtual ~PipelineChannel()
    {
        destroy();
    }

    virtual std::tr1::shared_ptr<ChannelProvider> getProvider()
    {
        return m_provider;
    }

    virtual std::string getRemoteAddress()
    {
        // local
        return getChannelName();
    }

    virtual ConnectionState getConnectionState()
    {
        return isConnected() ?
               Channel::CONNECTED :
               Channel::DESTROYED;
    }

    virtual std::string getChannelName()
    {
        return m_channelName;
    }

    virtual std::tr1::shared_ptr<ChannelRequester> getChannelRequester()
    {
        return m_channelRequester;
    }

    virtual bool isConnected()
    {
        return !m_destroyed.get();
    }


    virtual AccessRights getAccessRights(epics::pvData::PVField::shared_pointer const & /*pvField*/)
    {
        return none;
    }

    virtual void getField(GetFieldRequester::shared_pointer const & requester,std::string const & /*subField*/)
    {
        requester->getDone(notSupportedStatus, epics::pvData::Field::shared_pointer());
    }

    virtual ChannelProcess::shared_pointer createChannelProcess(
        ChannelProcessRequester::shared_pointer const & channelProcessRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelProcess::shared_pointer nullPtr;
        channelProcessRequester->channelProcessConnect(notSupportedStatus, nullPtr);
        return nullPtr;
    }

    virtual ChannelGet::shared_pointer createChannelGet(
        ChannelGetRequester::shared_pointer const & channelGetRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelGet::shared_pointer nullPtr;
        channelGetRequester->channelGetConnect(notSupportedStatus, nullPtr,
                                               epics::pvData::Structure::const_shared_pointer());
        return nullPtr;
    }

    virtual ChannelPut::shared_pointer createChannelPut(
        ChannelPutRequester::shared_pointer const & channelPutRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelPut::shared_pointer nullPtr;
        channelPutRequester->channelPutConnect(notSupportedStatus, nullPtr,
                                               epics::pvData::Structure::const_shared_pointer());
        return nullPtr;
    }


    virtual ChannelPutGet::shared_pointer createChannelPutGet(
        ChannelPutGetRequester::shared_pointer const & channelPutGetRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelPutGet::shared_pointer nullPtr;
        epics::pvData::Structure::const_shared_pointer nullStructure;
        channelPutGetRequester->channelPutGetConnect(notSupportedStatus, nullPtr, nullStructure, nullStructure);
        return nullPtr;
    }

    virtual ChannelRPC::shared_pointer createChannelRPC(
        ChannelRPCRequester::shared_pointer const & channelRPCRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelRPC::shared_pointer nullPtr;
        channelRPCRequester->channelRPCConnect(notSupportedStatus, nullPtr);
        return nullPtr;
    }

    virtual Monitor::shared_pointer createMonitor(
        MonitorRequester::shared_pointer const & monitorRequester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
    {
        if (!pvRequest)
            throw std::invalid_argument("pvRequest == null");

        if (m_destroyed.get())
        {
            Monitor::shared_pointer nullPtr;
            epics::pvData::Structure::const_shared_pointer nullStructure;
            monitorRequester->monitorConnect(destroyedStatus, nullPtr, nullStructure);
            return nullPtr;
        }

        // TODO use std::make_shared
        std::tr1::shared_ptr<ChannelPipelineMonitorImpl> tp(
            new ChannelPipelineMonitorImpl(shared_from_this(), monitorRequester, pvRequest, m_pipelineService)
        );
        Monitor::shared_pointer channelPipelineMonitorImpl = tp;

        if (tp->isPipelineEnabled())
        {
            monitorRequester->monitorConnect(Status::Ok, channelPipelineMonitorImpl, tp->getPipelineSession()->getStructure());
            return channelPipelineMonitorImpl;
        }
        else
        {
            Monitor::shared_pointer nullPtr;
            epics::pvData::Structure::const_shared_pointer nullStructure;
            Status noPipelineEnabledStatus(Status::STATUSTYPE_ERROR, "pipeline option not enabled, use e.g. 'record[queueSize=16,pipeline=true]field(value)' pvRequest to enable pipelining");
            monitorRequester->monitorConnect(noPipelineEnabledStatus, nullPtr, nullStructure);
            return nullPtr;
        }
    }

    virtual ChannelArray::shared_pointer createChannelArray(
        ChannelArrayRequester::shared_pointer const & channelArrayRequester,
        epics::pvData::PVStructure::shared_pointer const & /*pvRequest*/)
    {
        ChannelArray::shared_pointer nullPtr;
        channelArrayRequester->channelArrayConnect(notSupportedStatus, nullPtr, epics::pvData::Array::const_shared_pointer());
        return nullPtr;
    }


    virtual void printInfo()
    {
        printInfo(std::cout);
    }

    virtual void printInfo(std::ostream& out)
    {
        out << "PipelineChannel: ";
        out << getChannelName();
        out << " [";
        out << Channel::ConnectionStateNames[getConnectionState()];
        out << "]";
    }

    virtual string getRequesterName()
    {
        return getChannelName();
    }

    virtual void message(std::string const & message,MessageType messageType)
    {
        // just delegate
        m_channelRequester->message(message, messageType);
    }

    virtual void destroy()
    {
        m_destroyed.set();
    }
};

Status PipelineChannel::notSupportedStatus(Status::STATUSTYPE_ERROR, "only monitor (aka pipeline) requests are supported by this channel");
Status PipelineChannel::destroyedStatus(Status::STATUSTYPE_ERROR, "channel destroyed");

Channel::shared_pointer createPipelineChannel(ChannelProvider::shared_pointer const & provider,
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        PipelineService::shared_pointer const & pipelineService)
{
    // TODO use std::make_shared
    std::tr1::shared_ptr<PipelineChannel> tp(
        new PipelineChannel(provider, channelName, channelRequester, pipelineService)
    );
    Channel::shared_pointer channel = tp;
    return channel;
}


class PipelineChannelProvider :
    public virtual ChannelProvider,
    public virtual ChannelFind,
    public std::tr1::enable_shared_from_this<PipelineChannelProvider> {

public:
    POINTER_DEFINITIONS(PipelineChannelProvider);

    static string PROVIDER_NAME;

    static Status noSuchChannelStatus;

    // TODO thread pool support

    PipelineChannelProvider() {
    }

    virtual string getProviderName() {
        return PROVIDER_NAME;
    }

    virtual std::tr1::shared_ptr<ChannelProvider> getChannelProvider()
    {
        return shared_from_this();
    }

    virtual void cancel() {}

    virtual void destroy() {}

    virtual ChannelFind::shared_pointer channelFind(std::string const & channelName,
            ChannelFindRequester::shared_pointer const & channelFindRequester)
    {
        bool found;
        {
            Lock guard(m_mutex);
            found = (m_services.find(channelName) != m_services.end()) ||
                    findWildService(channelName);
        }
        ChannelFind::shared_pointer thisPtr(shared_from_this());
        channelFindRequester->channelFindResult(Status::Ok, thisPtr, found);
        return thisPtr;
    }


    virtual ChannelFind::shared_pointer channelList(
        ChannelListRequester::shared_pointer const & channelListRequester)
    {
        if (!channelListRequester.get())
            throw std::runtime_error("null requester");

        PVStringArray::svector channelNames;
        {
            Lock guard(m_mutex);
            channelNames.reserve(m_services.size());
            for (PipelineServiceMap::const_iterator iter = m_services.begin();
                    iter != m_services.end();
                    iter++)
                channelNames.push_back(iter->first);
        }

        ChannelFind::shared_pointer thisPtr(shared_from_this());
        channelListRequester->channelListResult(Status::Ok, thisPtr, freeze(channelNames), false);
        return thisPtr;
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & channelName,
        ChannelRequester::shared_pointer const & channelRequester,
        short /*priority*/)
    {
        PipelineService::shared_pointer service;

        PipelineServiceMap::const_iterator iter;
        {
            Lock guard(m_mutex);
            iter = m_services.find(channelName);
        }
        if (iter != m_services.end())
            service = iter->second;

        // check for wild services
        if (!service)
            service = findWildService(channelName);

        if (!service)
        {
            Channel::shared_pointer nullChannel;
            channelRequester->channelCreated(noSuchChannelStatus, nullChannel);
            return nullChannel;
        }

        // TODO use std::make_shared
        std::tr1::shared_ptr<PipelineChannel> tp(
            new PipelineChannel(
                shared_from_this(),
                channelName,
                channelRequester,
                service));
        Channel::shared_pointer pipelineChannel = tp;
        channelRequester->channelCreated(Status::Ok, pipelineChannel);
        return pipelineChannel;
    }

    virtual Channel::shared_pointer createChannel(
        std::string const & /*channelName*/,
        ChannelRequester::shared_pointer const & /*channelRequester*/,
        short /*priority*/,
        std::string const & /*address*/)
    {
        // this will never get called by the pvAccess server
        throw std::runtime_error("not supported");
    }

    void registerService(std::string const & serviceName, PipelineService::shared_pointer const & service)
    {
        Lock guard(m_mutex);
        m_services[serviceName] = service;

        if (isWildcardPattern(serviceName))
            m_wildServices.push_back(std::make_pair(serviceName, service));
    }

    void unregisterService(std::string const & serviceName)
    {
        Lock guard(m_mutex);
        m_services.erase(serviceName);

        if (isWildcardPattern(serviceName))
        {
            for (PipelineWildServiceList::iterator iter = m_wildServices.begin();
                    iter != m_wildServices.end();
                    iter++)
                if (iter->first == serviceName)
                {
                    m_wildServices.erase(iter);
                    break;
                }
        }
    }

private:
    // assumes sync on services
    PipelineService::shared_pointer findWildService(string const & wildcard)
    {
        if (!m_wildServices.empty())
            for (PipelineWildServiceList::iterator iter = m_wildServices.begin();
                    iter != m_wildServices.end();
                    iter++)
                if (Wildcard::wildcardfit(iter->first.c_str(), wildcard.c_str()))
                    return iter->second;

        return PipelineService::shared_pointer();
    }

    // (too) simple check
    bool isWildcardPattern(string const & pattern)
    {
        return
            (pattern.find('*') != string::npos ||
             pattern.find('?') != string::npos ||
             (pattern.find('[') != string::npos && pattern.find(']') != string::npos));
    }

    typedef std::map<string, PipelineService::shared_pointer> PipelineServiceMap;
    PipelineServiceMap m_services;

    typedef std::vector<std::pair<string, PipelineService::shared_pointer> > PipelineWildServiceList;
    PipelineWildServiceList m_wildServices;

    epics::pvData::Mutex m_mutex;
};

string PipelineChannelProvider::PROVIDER_NAME("PipelineService");
Status PipelineChannelProvider::noSuchChannelStatus(Status::STATUSTYPE_ERROR, "no such channel");



class PipelineChannelProviderFactory : public ChannelProviderFactory
{
public:
    POINTER_DEFINITIONS(PipelineChannelProviderFactory);

    PipelineChannelProviderFactory() :
        m_channelProviderImpl(new PipelineChannelProvider())
    {
    }

    virtual std::string getFactoryName()
    {
        return PipelineChannelProvider::PROVIDER_NAME;
    }

    virtual ChannelProvider::shared_pointer sharedInstance()
    {
        return m_channelProviderImpl;
    }

    virtual ChannelProvider::shared_pointer newInstance()
    {
        // TODO use std::make_shared
        std::tr1::shared_ptr<PipelineChannelProvider> tp(new PipelineChannelProvider());
        ChannelProvider::shared_pointer channelProvider = tp;
        return channelProvider;
    }

private:
    PipelineChannelProvider::shared_pointer m_channelProviderImpl;
};


PipelineServer::PipelineServer()
{
    // TODO factory is never deregistered, multiple PipelineServer instances create multiple factories, etc.
    m_channelProviderFactory.reset(new PipelineChannelProviderFactory());
    registerChannelProviderFactory(m_channelProviderFactory);

    m_channelProviderImpl = m_channelProviderFactory->sharedInstance();

    m_serverContext = ServerContextImpl::create();
    m_serverContext->setChannelProviderName(m_channelProviderImpl->getProviderName());

    m_serverContext->initialize(getChannelProviderRegistry());
}

PipelineServer::~PipelineServer()
{
    // multiple destroy call is OK
    destroy();
}

void PipelineServer::printInfo()
{
    std::cout << m_serverContext->getVersion().getVersionString() << std::endl;
    m_serverContext->printInfo();
}

void PipelineServer::run(int seconds)
{
    m_serverContext->run(seconds);
}

struct ThreadRunnerParam {
    PipelineServer::shared_pointer server;
    int timeToRun;
};

static void threadRunner(void* usr)
{
    ThreadRunnerParam* pusr = static_cast<ThreadRunnerParam*>(usr);
    ThreadRunnerParam param = *pusr;
    delete pusr;

    param.server->run(param.timeToRun);
}

/// Method requires usage of std::tr1::shared_ptr<PipelineServer>. This instance must be
/// owned by a shared_ptr instance.
void PipelineServer::runInNewThread(int seconds)
{
    std::auto_ptr<ThreadRunnerParam> param(new ThreadRunnerParam());
    param->server = shared_from_this();
    param->timeToRun = seconds;

    epicsThreadCreate("PipelineServer thread",
                      epicsThreadPriorityMedium,
                      epicsThreadGetStackSize(epicsThreadStackSmall),
                      threadRunner, param.get());

    // let the thread delete 'param'
    param.release();
}

void PipelineServer::destroy()
{
    m_serverContext->destroy();
}

void PipelineServer::registerService(std::string const & serviceName, PipelineService::shared_pointer const & service)
{
    std::tr1::dynamic_pointer_cast<PipelineChannelProvider>(m_channelProviderImpl)->registerService(serviceName, service);
}

void PipelineServer::unregisterService(std::string const & serviceName)
{
    std::tr1::dynamic_pointer_cast<PipelineChannelProvider>(m_channelProviderImpl)->unregisterService(serviceName);
}

}
}
