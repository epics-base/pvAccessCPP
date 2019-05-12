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
#include <pv/createRequest.h>

#define epicsExportSharedSymbols
#include "sharedstateimpl.h"

namespace {
struct PutOP : public pvas::Operation::Impl
{
    const std::tr1::shared_ptr<pvas::detail::SharedPut> op;

    PutOP(const std::tr1::shared_ptr<pvas::detail::SharedPut>& op,
          const pvd::PVStructure::const_shared_pointer& pvRequest,
          const pvd::PVStructure::const_shared_pointer& value,
          const pvd::BitSet& changed)
        :Impl(pvRequest, value, changed)
        ,op(op)
    {
        pva::ChannelRequester::shared_pointer req(op->channel->getChannelRequester());
        if(req)
            info = req->getPeerInfo();
    }
    virtual ~PutOP() {}

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
        if(value)
            throw std::logic_error("Put can't complete() with data");

        {
            Guard G(mutex);
            if(done)
                throw std::logic_error("Operation already complete");
            done = true;
        }

        pva::ChannelPutRequester::shared_pointer req(op->requester.lock());
        if(req)
            req->putDone(sts, op);
    }
};
}


namespace pvas {
namespace detail {

size_t SharedPut::num_instances;

SharedPut::SharedPut(const std::tr1::shared_ptr<SharedChannel>& channel,
          const requester_type::shared_pointer& requester,
          const pvd::PVStructure::const_shared_pointer &pvRequest)
    :channel(channel)
    ,requester(requester)
    ,pvRequest(pvRequest)
{
    REFTRACE_INCREMENT(num_instances);
}

SharedPut::~SharedPut()
{
    Guard G(channel->owner->mutex);
    channel->owner->puts.remove(this);
    REFTRACE_DECREMENT(num_instances);
}

void SharedPut::destroy() {}

std::tr1::shared_ptr<pva::Channel> SharedPut::getChannel()
{
    return channel;
}

void SharedPut::cancel() {}

void SharedPut::lastRequest() {}

void SharedPut::put(
        pvd::PVStructure::shared_pointer const & pvPutStructure,
        pvd::BitSet::shared_pointer const & putBitSet)
{
    std::tr1::shared_ptr<SharedPV::Handler> handler;
    pvd::PVStructure::shared_pointer realval;
    pvd::BitSet changed;
    pvd::Status sts;
    {
        Guard G(channel->owner->mutex);

        if(channel->dead) {
            sts = pvd::Status::error("Dead Channel");

        } else if(pvPutStructure->getStructure()!=mapper.requested()) {
            requester_type::shared_pointer req(requester.lock());
            sts = pvd::Status::error("Type changed");

        } else {

            handler = channel->owner->handler;

            realval = mapper.buildBase();

            mapper.copyBaseFromRequested(*realval, changed, *pvPutStructure, *putBitSet);
        }
    }

    if(!sts.isOK()) {
        requester_type::shared_pointer req(requester.lock());
        if(req)
            req->putDone(sts, pva::ChannelPut::shared_pointer());

    } else {
        std::tr1::shared_ptr<PutOP> impl(new PutOP(shared_from_this(), pvRequest, realval, changed),
                                         Operation::Impl::Cleanup());

        if(handler) {
            Operation op(impl);
            handler->onPut(channel->owner, op);
        }
    }
}

void SharedPut::get()
{
    pvd::Status sts;
    pvd::PVStructurePtr current;
    pvd::BitSetPtr changed;
    {
        Guard G(channel->owner->mutex);

        if(channel->dead) {
            sts = pvd::Status::error("Dead Channel");

        } else if(channel->owner->current) {
            assert(!!mapper.requested());

            current = mapper.buildRequested();
            changed.reset(new pvd::BitSet);

            mapper.copyBaseToRequested(*channel->owner->current, channel->owner->valid,
                                       *current, *changed);
        }
    }

    requester_type::shared_pointer req(requester.lock());
    if(!req) return;

    if(!sts.isOK()) {
        // no-op
    } else if(!current) {
        sts = pvd::Status::error("Get not possible, cache disabled");
    }

    req->getDone(sts, shared_from_this(), current, changed);
}

}} // namespace pvas::detail
