/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * pvAccessCPP is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include <pv/pvAccess.h>

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

ChannelProcess::shared_pointer Channel::createChannelProcess(
        ChannelProcessRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelProcess::shared_pointer ret;
    requester->channelProcessConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"), ret);
    return ret;
}

ChannelGet::shared_pointer Channel::createChannelGet(
        ChannelGetRequester::shared_pointer const & requester,
        epics::pvData::PVStructure::shared_pointer const & pvRequest)
{
    ChannelGet::shared_pointer ret;
    requester->channelGetConnect(pvd::Status(pvd::Status::STATUSTYPE_FATAL, "Not Implemented"),
                                 ret, pvd::StructureConstPtr());
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
