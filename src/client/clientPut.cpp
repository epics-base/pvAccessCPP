/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */

#include <epicsMutex.h>
#include <epicsGuard.h>
#include <epicsEvent.h>
#include <epicsThread.h>

#include <pv/current_function.h>
#include <pv/pvData.h>
#include <pv/bitSet.h>
#include <pv/reftrack.h>

#define epicsExportSharedSymbols
#include "pv/logger.h"
#include "clientpvt.h"
#include "pv/pvAccess.h"

namespace {
using pvac::detail::CallbackGuard;
using pvac::detail::CallbackUse;

struct Putter : public pvac::detail::CallbackStorage,
                public pva::ChannelPutRequester,
                public pvac::Operation::Impl,
                public pvac::detail::wrapped_shared_from_this<Putter>
{
    const bool getcurrent;

    bool started; // whether the put() has actually been sent.  After which point we can't safely re-try.
    operation_type::shared_pointer op;
    pvd::StructureConstPtr puttype;

    pvac::ClientChannel::PutCallback *cb;
    pvac::GetEvent event;

    static size_t num_instances;

    Putter(pvac::ClientChannel::PutCallback* cb, bool getcurrent) :getcurrent(getcurrent), started(false), cb(cb)
    {REFTRACE_INCREMENT(num_instances);}
    virtual ~Putter() {
        CallbackGuard G(*this);
        cb = 0;
        G.wait(); // paranoia
        REFTRACE_DECREMENT(num_instances);
    }

    void callEvent(CallbackGuard& G, pvac::GetEvent::event_t evt = pvac::GetEvent::Fail)
    {
        if(!cb) return;

        event.event = evt;
        pvac::ClientChannel::PutCallback *C=cb;
        cb = 0;
        CallbackUse U(G);
        try {
            C->putDone(event);
        } catch(std::exception& e) {
            LOG(pva::logLevelInfo, "Lost exception during putDone(): %s", e.what());
        }
    }

    virtual std::string name() const OVERRIDE FINAL
    {
        Guard G(mutex);
        return op ? op->getChannel()->getChannelName() : "<dead>";
    }

    // called automatically via wrapped_shared_from_this
    virtual void cancel() OVERRIDE FINAL
    {
        // keepalive for safety in case callback wants to destroy us
        std::tr1::shared_ptr<Putter> keepalive(internal_shared_from_this());
        CallbackGuard G(*this);
        if(started && op) op->cancel();
        callEvent(G, pvac::GetEvent::Cancel);
        G.wait();
    }

    virtual std::string getRequesterName() OVERRIDE FINAL
    {
        Guard G(mutex);
        return op ? op->getChannel()->getRequesterName() : "<dead>";
    }

    virtual void channelPutConnect(
        const epics::pvData::Status& status,
        pva::ChannelPut::shared_pointer const & channelPut,
        epics::pvData::Structure::const_shared_pointer const & structure) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<Putter> keepalive(internal_shared_from_this());
        CallbackGuard G(*this);
        op = channelPut; // we may be called before createChannelPut() has returned.
        puttype = structure;
        if(started || !cb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }
        if(!status.isSuccess()) {
            callEvent(G);

        } else if(getcurrent) {
            // fetch a previous value first
            op->get();
        } else {
            // build Put value immediately
            pvd::BitSet empty;
            pvd::BitSet::shared_pointer tosend(new pvd::BitSet);
            pvac::ClientChannel::PutCallback::Args args(*tosend, empty);
            // args.previous = 0; // implied
            doPut(G, args, channelPut, tosend);
        }
    }

    virtual void channelDisconnect(bool destroy) OVERRIDE FINAL
    {
        CallbackGuard G(*this);
        event.message = "Disconnect";

        callEvent(G);
    }

    void doPut(CallbackGuard& G,
               pvac::ClientChannel::PutCallback::Args& args,
               pva::ChannelPut::shared_pointer const & channelPut,
               const pvd::BitSet::shared_pointer& tosend)
    {
        try {
            pvac::ClientChannel::PutCallback *C(cb);
            CallbackUse U(G);
            C->putBuild(puttype, args);
            if(!args.root)
                throw std::logic_error("No put value provided");
            else if(*args.root->getStructure()!=*puttype)
                throw std::logic_error("Provided put value with wrong type");
        }catch(std::exception& e){
            if(cb) {
                event.message = e.what();
                callEvent(G);
            } else {
                LOG(pva::logLevelInfo, "Lost exception in %s: %s", CURRENT_FUNCTION, e.what());
            }
        }
        // check cb again after UnGuard
        if(cb) {
            started = true;
            channelPut->put(std::tr1::const_pointer_cast<pvd::PVStructure>(args.root), tosend);
        }
    }

    virtual void getDone(
        const epics::pvData::Status& status,
        pva::ChannelPut::shared_pointer const & channelPut,
        epics::pvData::PVStructure::shared_pointer const & pvStructure,
        epics::pvData::BitSet::shared_pointer const & bitSet) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<Putter> keepalive(internal_shared_from_this());
        CallbackGuard G(*this);
        if(!cb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();

            callEvent(G, pvac::GetEvent::Fail);

        } else {
            pvd::BitSet::shared_pointer tosend(new pvd::BitSet);
            pvac::ClientChannel::PutCallback::Args args(*tosend, *bitSet);
            args.previous = pvStructure;
            doPut(G, args, channelPut, tosend);
        }
    }

    virtual void putDone(
        const epics::pvData::Status& status,
        pva::ChannelPut::shared_pointer const & channelPut) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<Putter> keepalive(internal_shared_from_this());
        CallbackGuard G(*this);
        if(!cb) return;

        if(!status.isOK()) {
            event.message = status.getMessage();
        } else {
            event.message.clear();
        }

        callEvent(G, status.isSuccess()? pvac::GetEvent::Success : pvac::GetEvent::Fail);
    }

    virtual void show(std::ostream &strm) const OVERRIDE FINAL
    {
        strm << "Operation(Put"
                "\"" << name() <<"\""
             ")";
    }
};

size_t Putter::num_instances;

} //namespace

namespace pvac {

Operation
ClientChannel::put(PutCallback* cb,
                   epics::pvData::PVStructure::const_shared_pointer pvRequest,
                   bool getprevious)
{
    if(!impl) throw std::logic_error("Dead Channel");
    if(!pvRequest)
        pvRequest = pvd::createRequest("field()");

    std::tr1::shared_ptr<Putter> ret(Putter::build(cb, getprevious));

    {
        Guard G(ret->mutex);
        ret->op = getChannel()->createChannelPut(ret->internal_shared_from_this(),
                                                 std::tr1::const_pointer_cast<pvd::PVStructure>(pvRequest));
    }

    return Operation(ret);

}

namespace detail {

void registerRefTrackPut()
{
    epics::registerRefCounter("pvac::Putter", &Putter::num_instances);
}

}

}//namespace pvac
