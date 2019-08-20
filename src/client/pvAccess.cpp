/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <pv/reftrack.h>
#include <pv/valueBuilder.h>
#include <pv/epicsException.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>
#include <pv/security.h>

namespace pvd = epics::pvData;

namespace epics {
namespace pvAccess {

size_t Channel::num_instances;

const char* Channel::ConnectionStateNames[] = { "NEVER_CONNECTED", "CONNECTED", "DISCONNECTED", "DESTROYED" };

Channel::Channel() {REFTRACE_INCREMENT(num_instances);}
Channel::~Channel() {REFTRACE_DECREMENT(num_instances);}

std::string Channel::getRequesterName()
{
    std::tr1::shared_ptr<ChannelRequester> req(getChannelRequester());
    return req ? req->getRequesterName() : std::string("<Destroy'd Channel>");
}

void Channel::message(std::string const & message, epics::pvData::MessageType messageType)
{
    std::tr1::shared_ptr<ChannelRequester> req(getChannelRequester());
    if(req) {
        req->message(message, messageType);
    } else {
        std::cerr<<epics::pvData::getMessageTypeName(messageType)
                 <<": on Destroy'd Channel \""<<getChannelName()
                 <<"\" : "<<message;
    }
}

Channel::ConnectionState Channel::getConnectionState() { return CONNECTED; }

bool Channel::isConnected() { return getConnectionState()==CONNECTED; }

void Channel::getField(GetFieldRequester::shared_pointer const & requester,std::string const & subField)
{
    requester->getDone(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented")
                       ,pvd::FieldConstPtr());
}

AccessRights Channel::getAccessRights(epics::pvData::PVField::shared_pointer const & pvField)
{
    return readWrite;
}

namespace {
/* allow createChannelProcess() to use a ChannelPut w/o data */
struct Process2PutProxy : public ChannelProcess
{
    struct Req : public ChannelPutRequester
    {
        const ChannelProcessRequester::weak_pointer requester; // was passed to createChannelProcess()
        const std::tr1::weak_ptr<Process2PutProxy> operation; // enclosing Process2PutProxy

        epicsMutex mutex;
        epics::pvData::PVStructurePtr dummy;

        Req(const ChannelProcessRequester::weak_pointer& req,
            const std::tr1::weak_ptr<Process2PutProxy>& op)
            :requester(req), operation(op)
        {}
        virtual ~Req() {}

        virtual std::string getRequesterName() OVERRIDE FINAL {
            ChannelProcessRequester::shared_pointer req(requester.lock());
            return req ? req->getRequesterName() : "";
        }

        virtual void channelDisconnect(bool destroy) OVERRIDE FINAL {
            epics::pvData::PVStructurePtr dummy;
            {
                epicsGuard<epicsMutex> G(mutex);
                this->dummy.swap(dummy);
            }
            ChannelProcessRequester::shared_pointer req(requester.lock());
            if(req)
                req->channelDisconnect(destroy);
        }

        virtual void channelPutConnect(
            const epics::pvData::Status& status,
            ChannelPut::shared_pointer const & channelPut,
            epics::pvData::Structure::const_shared_pointer const & structure) OVERRIDE FINAL
        {
            epics::pvData::PVStructurePtr dummy(epics::pvData::getPVDataCreate()->createPVStructure(structure));
            ChannelProcessRequester::shared_pointer req(requester.lock());
            std::tr1::shared_ptr<Process2PutProxy> op(operation.lock());
            if(!op) return;
            {
                epicsGuard<epicsMutex> G(mutex);
                this->dummy = dummy;
                op->op = channelPut;
            }
            if(req)
                req->channelProcessConnect(status, op);
        }

        virtual void putDone(
            const epics::pvData::Status& status,
            ChannelPut::shared_pointer const & channelPut) OVERRIDE FINAL
        {
            ChannelProcessRequester::shared_pointer req(requester.lock());
            std::tr1::shared_ptr<Process2PutProxy> op(operation.lock());
            if(req && op)
                req->processDone(status, op);
        }

        virtual void getDone(
            const epics::pvData::Status& status,
            ChannelPut::shared_pointer const & channelPut,
            epics::pvData::PVStructure::shared_pointer const & pvStructure,
            epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL
        { /* never called */ }
    };

    ChannelPut::shared_pointer op; // the op we wrap
    std::tr1::shared_ptr<Req> op_request; // keep our Req alive

    epics::pvData::BitSetPtr empty;

    Process2PutProxy() :empty(new epics::pvData::BitSet) {}
    virtual ~Process2PutProxy() {}

    virtual void destroy() OVERRIDE FINAL
    { op->destroy(); }
    virtual std::tr1::shared_ptr<Channel> getChannel() OVERRIDE FINAL
    { return op->getChannel(); }
    virtual void cancel() OVERRIDE FINAL
    { op->cancel(); }
    virtual void lastRequest() OVERRIDE FINAL
    { op->lastRequest(); }
    virtual void process() OVERRIDE FINAL
    {
        epics::pvData::PVStructurePtr blob;
        {
            epicsGuard<epicsMutex> G(op_request->mutex);
            blob = op_request->dummy;
        }
        if(!blob) {
            ChannelProcessRequester::shared_pointer req(op_request->requester.lock());
            ChannelProcess::shared_pointer op(op_request->operation.lock());
            req->processDone(epics::pvData::Status::error("Not connected"), op);
        } else {
            empty->clear();
            op->put(blob, empty);
        }
    }
};
}//namespace

ChannelProcess::shared_pointer Channel::createChannelProcess(
        ChannelProcessRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequestx)
{
    pvd::PVStructure::shared_pointer pvRequest(pvRequestx);
    std::tr1::shared_ptr<Process2PutProxy> ret(new Process2PutProxy);
    ret->op_request.reset(new Process2PutProxy::Req(requester, ret));

    // inject record._options.process=true if client hasn't provided
    if(!pvRequest->getSubField("record._options.process"))
    {
        pvRequest = pvd::ValueBuilder(*pvRequest)
                     .addNested("record")
                         .addNested("_options")
                            .add<pvd::pvString>("process", "true")
                         .endNested()
                     .endNested()
                     .buildPVStructure();
    }

    ChannelPut::shared_pointer op(createChannelPut(ret->op_request, pvRequest));
    if(!op) {
        ret.reset();
    } else {
        epicsGuard<epicsMutex> G(ret->op_request->mutex);
        ret->op = op;
    }

    return ret;
}

namespace {
/** Allow createChannelGet() to use createChannelPut()
 */
struct Get2PutProxy : public ChannelGet
{
    struct Req : public ChannelPutRequester
    {
        const ChannelGetRequester::weak_pointer requester; // was passed to createChannelGet()
        const std::tr1::weak_ptr<Get2PutProxy> operation; // enclosing Get2PutProxy

        epicsMutex mutex;

        Req(const ChannelGetRequester::weak_pointer& req,
            const std::tr1::weak_ptr<Get2PutProxy>& op)
            :requester(req), operation(op)
        {}
        virtual ~Req() {}

        virtual std::string getRequesterName() OVERRIDE FINAL {
            ChannelGetRequester::shared_pointer req(requester.lock());
            return req ? req->getRequesterName() : "";
        }

        virtual void message(const std::string &message, MessageType messageType) OVERRIDE FINAL {
            ChannelGetRequester::shared_pointer req(requester.lock());
            if(req)
                req->message(message, messageType);
            else
                ChannelPutRequester::message(message, messageType);
        }

        virtual void channelDisconnect(bool destroy) OVERRIDE FINAL {
            ChannelGetRequester::shared_pointer req(requester.lock());
            if(req)
                req->channelDisconnect(destroy);
        }

        virtual void channelPutConnect(
            const epics::pvData::Status& status,
            ChannelPut::shared_pointer const & channelPut,
            epics::pvData::Structure::const_shared_pointer const & structure) OVERRIDE FINAL
        {
            ChannelGetRequester::shared_pointer req(requester.lock());
            std::tr1::shared_ptr<Get2PutProxy> op(operation.lock());
            if(!op) return;
            {
                epicsGuard<epicsMutex> G(mutex);
                op->op = channelPut;
            }
            if(req)
                req->channelGetConnect(status, op, structure);
        }

        virtual void putDone(
            const epics::pvData::Status& status,
            ChannelPut::shared_pointer const & channelPut) OVERRIDE FINAL
        { /* never called */ }

        virtual void getDone(
            const epics::pvData::Status& status,
            ChannelPut::shared_pointer const & channelPut,
            epics::pvData::PVStructure::shared_pointer const & pvStructure,
            epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL
        {
            ChannelGetRequester::shared_pointer req(requester.lock());
            std::tr1::shared_ptr<Get2PutProxy> op(operation.lock());
            if(req && op)
                req->getDone(status, op, pvStructure, bitSet);
        }
    };

    ChannelPut::shared_pointer op; // the put we wrap
    std::tr1::shared_ptr<Get2PutProxy::Req> op_request; // keep our Req alive

    ChannelPut::shared_pointer OP() {
        epicsGuard<epicsMutex> G(op_request->mutex);
        return op;
    }

    Get2PutProxy() {}
    virtual ~Get2PutProxy() {}

    virtual void destroy() OVERRIDE FINAL
    {
        ChannelPut::shared_pointer O(OP());
        if(O) O->destroy();
    }
    virtual std::tr1::shared_ptr<Channel> getChannel() OVERRIDE FINAL
    {
        ChannelPut::shared_pointer O(OP());
        return O ? O->getChannel() : std::tr1::shared_ptr<Channel>();
    }
    virtual void cancel() OVERRIDE FINAL
    {
        ChannelPut::shared_pointer O(OP());
        if(O) O->cancel();
    }
    virtual void lastRequest() OVERRIDE FINAL
    {
        ChannelPut::shared_pointer O(OP());
        if(O) O->lastRequest();
    }
    virtual void get() OVERRIDE FINAL
    {
        ChannelPut::shared_pointer O(OP());
        if(O) O->get();
    }
};
}// namespace

ChannelGet::shared_pointer Channel::createChannelGet(
        ChannelGetRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    std::tr1::shared_ptr<Get2PutProxy> ret(new Get2PutProxy);
    ret->op_request.reset(new Get2PutProxy::Req(requester, ret));

    ChannelPut::shared_pointer op(createChannelPut(ret->op_request, pvRequest));
    if(!op) {
        ret.reset();
    } else {
        epicsGuard<epicsMutex> G(ret->op_request->mutex);
        ret->op = op;
    }

    return ret;
}

ChannelPut::shared_pointer Channel::createChannelPut(
        ChannelPutRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelPut::shared_pointer ret;
    requester->channelPutConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"),
                                 ret, pvd::StructureConstPtr());
    return ret;
}

ChannelPutGet::shared_pointer Channel::createChannelPutGet(
        ChannelPutGetRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelPutGet::shared_pointer ret;
    requester->channelPutGetConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"),
                                    ret, pvd::StructureConstPtr(), pvd::StructureConstPtr());
    return ret;
}

ChannelRPC::shared_pointer Channel::createChannelRPC(
        ChannelRPCRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelRPC::shared_pointer ret;
    requester->channelRPCConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"), ret);
    return ret;
}

pvd::Monitor::shared_pointer Channel::createMonitor(
        epics::pvAccess::MonitorRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    pvd::Monitor::shared_pointer ret;
    requester->monitorConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"),
                              ret, pvd::StructureConstPtr());
    return ret;
}

ChannelArray::shared_pointer Channel::createChannelArray(
        ChannelArrayRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelArray::shared_pointer ret;
    requester->channelArrayConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"),
                                   ret, pvd::Array::const_shared_pointer());
    return ret;
}

size_t ChannelProvider::num_instances;

ChannelProvider::ChannelProvider()
{
    REFTRACE_INCREMENT(num_instances);
}

ChannelProvider::~ChannelProvider()
{
    REFTRACE_DECREMENT(num_instances);
}

NetStats::~NetStats() {}

size_t ChannelBaseRequester::num_instances;

ChannelBaseRequester::ChannelBaseRequester()
{
    REFTRACE_INCREMENT(num_instances);
}

ChannelBaseRequester::~ChannelBaseRequester()
{
    REFTRACE_DECREMENT(num_instances);
}

size_t ChannelRequest::num_instances;

ChannelRequest::ChannelRequest()
{
    REFTRACE_INCREMENT(num_instances);
}

ChannelRequest::~ChannelRequest()
{
    REFTRACE_DECREMENT(num_instances);
}

size_t ChannelRequester::num_instances;

ChannelRequester::ChannelRequester()
{
    REFTRACE_INCREMENT(num_instances);
}

ChannelRequester::~ChannelRequester()
{
    REFTRACE_DECREMENT(num_instances);
}

PeerInfo::const_shared_pointer ChannelRequester::getPeerInfo()
{
    return PeerInfo::const_shared_pointer();
}

std::string DefaultChannelRequester::getRequesterName() { return "DefaultChannelRequester"; }

void DefaultChannelRequester::channelCreated(const epics::pvData::Status& status, Channel::shared_pointer const & channel)
{
    if(!status.isSuccess()) {
        std::ostringstream strm;
        status.dump(strm);
        throw std::runtime_error(strm.str());
    }
}

void DefaultChannelRequester::channelStateChange(Channel::shared_pointer const & channel, Channel::ConnectionState connectionState)
{ /* no-op */ }

ChannelRequester::shared_pointer DefaultChannelRequester::build()
{
    ChannelRequester::shared_pointer ret(new DefaultChannelRequester);
    return ret;
}

MonitorElement::MonitorElement(epics::pvData::PVStructurePtr const & pvStructurePtr)
    : pvStructurePtr(pvStructurePtr)
    ,changedBitSet(epics::pvData::BitSet::create(static_cast<epics::pvData::uint32>(pvStructurePtr->getNumberFields())))
    ,overrunBitSet(epics::pvData::BitSet::create(static_cast<epics::pvData::uint32>(pvStructurePtr->getNumberFields())))
{}

}} // namespace epics::pvAccess

namespace {

struct DummyChannelFind : public epics::pvAccess::ChannelFind {
    epics::pvAccess::ChannelProvider::weak_pointer provider;
    DummyChannelFind(const epics::pvAccess::ChannelProvider::shared_pointer& provider) : provider(provider) {}
    virtual ~DummyChannelFind() {}
    virtual void destroy() OVERRIDE FINAL {}
    virtual epics::pvAccess::ChannelProvider::shared_pointer getChannelProvider() OVERRIDE FINAL { return provider.lock(); }
    virtual void cancel() OVERRIDE FINAL {}
};

}

namespace epics {namespace pvAccess {

ChannelFind::shared_pointer ChannelFind::buildDummy(const ChannelProvider::shared_pointer& provider)
{
    std::tr1::shared_ptr<DummyChannelFind> ret(new DummyChannelFind(provider));
    return ret;
}

}} // namespace epics::pvAccess
