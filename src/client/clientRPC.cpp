/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>

#include <pv/pvData.h>
#include <pv/bitSet.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "pv/logger.h"
#include "clientpvt.h"
#include "pv/pvAccess.h"

namespace pvd = epics::pvData;
namespace pva = epics::pvAccess;
typedef epicsGuard<epicsMutex> Guard;
typedef epicsGuardRelease<epicsMutex> UnGuard;

namespace {

struct RPCer : public pva::ChannelRPCRequester,
               public pvac::Operation::Impl,
               public pvac::detail::wrapped_shared_from_this<RPCer>
{
    mutable epicsMutex mutex;

    bool started;
    operation_type::shared_pointer op;

    pvac::ClientChannel::GetCallback *cb;
    pvac::GetEvent event;

    pvd::PVStructure::const_shared_pointer args;

    static size_t num_instances;

    RPCer(pvac::ClientChannel::GetCallback* cb,
          const pvd::PVStructure::const_shared_pointer& args) :started(false), cb(cb), args(args)
      {REFTRACE_INCREMENT(num_instances);}
    virtual ~RPCer() {REFTRACE_DECREMENT(num_instances);}

    void callEvent(Guard& G, pvac::GetEvent::event_t evt = pvac::GetEvent::Fail)
    {
        pvac::ClientChannel::GetCallback *cb=this->cb;
        if(!cb) return;

        event.event = evt;

        this->cb = 0;

        try {
            UnGuard U(G);
            cb->getDone(event);
            return;
        }catch(std::exception& e){
            if(!this->cb || evt==pvac::GetEvent::Fail) {
                LOG(pva::logLevelError, "Unhandled exception in ClientChannel::GetCallback::getDone(): %s", e.what());
            } else {
               event.event = pvac::GetEvent::Fail;
               event.message = e.what();
            }
        }
        // continues error handling
        try {
            UnGuard U(G);
            cb->getDone(event);
            return;
        }catch(std::exception& e){
            LOG(pva::logLevelError, "Unhandled exception following exception in ClientChannel::GetCallback::monitorEvent(): %s", e.what());
        }
    }

    virtual std::string name() const OVERRIDE FINAL
    {
        Guard G(mutex);
        return op ? op->getChannel()->getChannelName() : "<dead>";
    }

    // called automatically via wrapped_shared_from_this
    virtual void cancel()
    {
        Guard G(mutex);
        if(started && op) op->cancel();
        callEvent(G, pvac::GetEvent::Cancel);
    }

    virtual std::string getRequesterName() OVERRIDE FINAL
    {
        Guard G(mutex);
        return op ? op->getChannel()->getRequesterName() : "<dead>";
    }

    virtual void channelRPCConnect(
        const epics::pvData::Status& status,
        pva::ChannelRPC::shared_pointer const & operation)
    {
        Guard G(mutex);
        if(!cb || started) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        if(!status.isSuccess()) {
            callEvent(G);

        } else {
            operation->request(std::tr1::const_pointer_cast<pvd::PVStructure>(args));
            started = true;
        }
    }

    virtual void channelDisconnect(bool destroy) OVERRIDE FINAL
    {
        Guard G(mutex);
        event.message = "Disconnect";

        callEvent(G);
    }

    virtual void requestDone(
        const epics::pvData::Status& status,
        pva::ChannelRPC::shared_pointer const & operation,
        epics::pvData::PVStructure::shared_pointer const & pvResponse)
    {
        Guard G(mutex);
        if(!cb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        event.value = pvResponse;

        callEvent(G, status.isSuccess()? pvac::GetEvent::Success : pvac::GetEvent::Fail);
    }
};

size_t RPCer::num_instances;

}//namespace

namespace pvac {

Operation
ClientChannel::rpc(GetCallback* cb,
                       const epics::pvData::PVStructure::const_shared_pointer& arguments,
                       epics::pvData::PVStructure::const_shared_pointer pvRequest)
{
    if(!impl) throw std::logic_error("Dead Channel");
    if(!pvRequest)
        pvRequest = pvd::createRequest("field()");

    std::tr1::shared_ptr<RPCer> ret(RPCer::build(cb, arguments));

    {
        Guard G(ret->mutex);
        ret->op = getChannel()->createChannelRPC(ret->internal_shared_from_this(),
                                                 std::tr1::const_pointer_cast<pvd::PVStructure>(pvRequest));
    }

    return Operation(ret);
}

namespace detail {

void registerRefTrackRPC()
{
    epics::registerRefCounter("pvac::RPCer", &RPCer::num_instances);
}

}

}//namespace pvac
