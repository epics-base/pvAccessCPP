/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <list>

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <errlog.h>

#include <shareLib.h>
#include <pv/sharedPtr.h>
#include <pv/noDefaultMethods.h>
#include <pv/sharedVector.h>
#include <pv/bitSet.h>
#include <pv/pvData.h>
#include <pv/createRequest.h>
#include <pv/status.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "sharedstateimpl.h"

namespace {
struct RPCOP : public pvas::Operation::Impl
{
    const std::tr1::shared_ptr<pvas::detail::SharedRPC> op;

    RPCOP(const std::tr1::shared_ptr<pvas::detail::SharedRPC>& op,
          const pvd::PVStructure::const_shared_pointer& pvRequest,
          const pvd::PVStructure::const_shared_pointer& value)
        :Impl(pvRequest, value, pvd::BitSet().set(0))
        ,op(op)
    {
        pva::ChannelRequester::shared_pointer req(op->channel->getChannelRequester());
        if(req)
            info = req->getPeerInfo();
    }
    virtual ~RPCOP() {}

    virtual pva::Channel::shared_pointer getChannel() OVERRIDE FINAL
    {
        return op->channel;
    }

    virtual pva::ChannelBaseRequester::shared_pointer getRequester() OVERRIDE FINAL
    {
        return op->requester.lock();
    }

    virtual void complete(const pvd::Status& sts,
                          const epics::pvData::PVStructure* value) OVERRIDE FINAL
    {
        {
            Guard G(mutex);
            if(done)
                throw std::logic_error("Operation already complete");
            done = true;
        }
        epics::pvData::PVStructurePtr tosend;
        if(!sts.isSuccess()) {
            // no data for error
        } else if(value) {
            tosend = pvd::getPVDataCreate()->createPVStructure(value->getStructure());
            tosend->copyUnchecked(*value);
        } else {
            // RPC with null result.  Make empty structure
            tosend = pvd::getPVDataCreate()->createPVStructure(
                        pvd::getFieldCreate()
                        ->createFieldBuilder()
                        ->createStructure());
        }
        pva::ChannelRPCRequester::shared_pointer req(op->requester.lock());
        if(req)
            req->requestDone(sts, op, tosend);
    }
};
}

namespace pvas {
namespace detail {

size_t SharedRPC::num_instances;

SharedRPC::SharedRPC(const std::tr1::shared_ptr<SharedChannel>& channel,
          const requester_type::shared_pointer& requester,
          const pvd::PVStructure::const_shared_pointer &pvRequest)
    :channel(channel)
    ,requester(requester)
    ,pvRequest(pvRequest)
    ,connected(false)
{
    REFTRACE_INCREMENT(num_instances);
}

SharedRPC::~SharedRPC() {
    Guard G(channel->owner->mutex);
    channel->owner->rpcs.remove(this);
    REFTRACE_DECREMENT(num_instances);
}

void SharedRPC::destroy() {}

std::tr1::shared_ptr<pva::Channel> SharedRPC::getChannel()
{
    return channel;
}

void SharedRPC::cancel() {}

void SharedRPC::lastRequest() {}

void SharedRPC::request(epics::pvData::PVStructure::shared_pointer const & pvArgument)
{
    std::tr1::shared_ptr<SharedPV::Handler> handler;
    pvd::Status sts;
    {
        Guard G(channel->owner->mutex);
        if(channel->dead) {
            sts = pvd::Status::error("Dead Channel");

        } else {
            handler = channel->owner->handler;
        }
    }

    if(!sts.isOK()) {
        requester_type::shared_pointer req(requester.lock());
        if(req)
            req->requestDone(sts, shared_from_this(), pvd::PVStructurePtr());

    } else {
        std::tr1::shared_ptr<RPCOP> impl(new RPCOP(shared_from_this(), pvRequest, pvArgument),
                                         Operation::Impl::Cleanup());

        if(handler) {
            Operation op(impl);
            handler->onRPC(channel->owner, op);
        }
    }
}


}} // namespace pvas::detail
